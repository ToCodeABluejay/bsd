/*	$OpenBSD: pluart.c,v 1.2 2017/01/20 08:03:21 patrick Exp $	*/

/*
 * Copyright (c) 2014 Patrick Wildt <patrick@blueri.se>
 * Copyright (c) 2005 Dale Rahn <drahn@dalerahn.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/device.h>
#include <sys/syslog.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/kernel.h>

#include <dev/cons.h>

#ifdef DDB
#include <ddb/db_var.h>
#endif

#include <machine/bus.h>
#include <machine/fdt.h>
#include <arm64/arm64/arm64var.h>
#include <arm64/arm64/arm64_machdep.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>

#define DEVUNIT(x)      (minor(x) & 0x7f)
#define DEVCUA(x)       (minor(x) & 0x80)

#define UART_DR			0x00		/* Data register */
#define UART_DR_DATA(x)		((x) & 0xf)
#define UART_DR_FE		(1 << 8)	/* Framing error */
#define UART_DR_PE		(1 << 9)	/* Parity error */
#define UART_DR_BE		(1 << 10)	/* Break error */
#define UART_DR_OE		(1 << 11)	/* Overrun error */
#define UART_RSR		0x04		/* Receive status register */
#define UART_RSR_FE		(1 << 0)	/* Framing error */
#define UART_RSR_PE		(1 << 1)	/* Parity error */
#define UART_RSR_BE		(1 << 2)	/* Break error */
#define UART_RSR_OE		(1 << 3)	/* Overrun error */
#define UART_ECR		0x04		/* Error clear register */
#define UART_ECR_FE		(1 << 0)	/* Framing error */
#define UART_ECR_PE		(1 << 1)	/* Parity error */
#define UART_ECR_BE		(1 << 2)	/* Break error */
#define UART_ECR_OE		(1 << 3)	/* Overrun error */
#define UART_FR			0x18		/* Flag register */
#define UART_FR_CTS		(1 << 0)	/* Clear to send */
#define UART_FR_DSR		(1 << 1)	/* Data set ready */
#define UART_FR_DCD		(1 << 2)	/* Data carrier detect */
#define UART_FR_BUSY		(1 << 3)	/* UART busy */
#define UART_FR_RXFE		(1 << 4)	/* Receive FIFO empty */
#define UART_FR_TXFF		(1 << 5)	/* Transmit FIFO full */
#define UART_FR_RXFF		(1 << 6)	/* Receive FIFO full */
#define UART_FR_TXFE		(1 << 7)	/* Transmit FIFO empty */
#define UART_FR_RI		(1 << 8)	/* Ring indicator */
#define UART_ILPR		0x20		/* IrDA low-power counter register */
#define UART_ILPR_ILPDVSR	((x) & 0xf)	/* IrDA low-power divisor */
#define UART_IBRD		0x24		/* Integer baud rate register */
#define UART_IBRD_DIVINT	((x) & 0xff)	/* Integer baud rate divisor */
#define UART_FBRD		0x28		/* Fractional baud rate register */
#define UART_FBRD_DIVFRAC	((x) & 0x3f)	/* Fractional baud rate divisor */
#define UART_LCR_H		0x2c		/* Line control register */
#define UART_LCR_H_BRK		(1 << 0)	/* Send break */
#define UART_LCR_H_PEN		(1 << 1)	/* Parity enable */
#define UART_LCR_H_EPS		(1 << 2)	/* Even parity select */
#define UART_LCR_H_STP2		(1 << 3)	/* Two stop bits select */
#define UART_LCR_H_FEN		(1 << 4)	/* Enable FIFOs */
#define UART_LCR_H_WLEN5	(0x0 << 5)	/* Word length: 5 bits */
#define UART_LCR_H_WLEN6	(0x1 << 5)	/* Word length: 6 bits */
#define UART_LCR_H_WLEN7	(0x2 << 5)	/* Word length: 7 bits */
#define UART_LCR_H_WLEN8	(0x3 << 5)	/* Word length: 8 bits */
#define UART_LCR_H_SPS		(1 << 7)	/* Stick parity select */
#define UART_CR			0x30		/* Control register */
#define UART_CR_UARTEN		(1 << 0)	/* UART enable */
#define UART_CR_SIREN		(1 << 1)	/* SIR enable */
#define UART_CR_SIRLP		(1 << 2)	/* IrDA SIR low power mode */
#define UART_CR_LBE		(1 << 7)	/* Loop back enable */
#define UART_CR_TXE		(1 << 8)	/* Transmit enable */
#define UART_CR_RXE		(1 << 9)	/* Receive enable */
#define UART_CR_DTR		(1 << 10)	/* Data transmit enable */
#define UART_CR_RTS		(1 << 11)	/* Request to send */
#define UART_CR_OUT1		(1 << 12)
#define UART_CR_OUT2		(1 << 13)
#define UART_CR_CTSE		(1 << 14)	/* CTS hardware flow control enable */
#define UART_CR_RTSE		(1 << 15)	/* RTS hardware flow control enable */
#define UART_IFLS		0x34		/* Interrupt FIFO level select register */
#define UART_IMSC		0x38		/* Interrupt mask set/clear register */
#define UART_IMSC_RIMIM		(1 << 0)
#define UART_IMSC_CTSMIM	(1 << 1)
#define UART_IMSC_DCDMIM	(1 << 2)
#define UART_IMSC_DSRMIM	(1 << 3)
#define UART_IMSC_RXIM		(1 << 4)
#define UART_IMSC_TXIM		(1 << 5)
#define UART_IMSC_RTIM		(1 << 6)
#define UART_IMSC_FEIM		(1 << 7)
#define UART_IMSC_PEIM		(1 << 8)
#define UART_IMSC_BEIM		(1 << 9)
#define UART_IMSC_OEIM		(1 << 10)
#define UART_RIS		0x3c		/* Raw interrupt status register */
#define UART_MIS		0x40		/* Masked interrupt status register */
#define UART_ICR		0x44		/* Interrupt clear register */
#define UART_DMACR		0x48		/* DMA control register */
#define UART_SPACE		0x100

struct pluart_softc {
	struct device	sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	struct soft_intrhand *sc_si;
	void *sc_irq;
	struct tty	*sc_tty;
	struct timeout	sc_diag_tmo;
	struct timeout	sc_dtr_tmo;
	int		sc_overflows;
	int		sc_floods;
	int		sc_errors;
	int		sc_halt;
	u_int16_t	sc_ucr1;
	u_int16_t	sc_ucr2;
	u_int16_t	sc_ucr3;
	u_int16_t	sc_ucr4;
	u_int8_t	sc_hwflags;
#define COM_HW_NOIEN    0x01
#define COM_HW_FIFO     0x02
#define COM_HW_SIR      0x20
#define COM_HW_CONSOLE  0x40
#define COM_HW_KGDB     0x80
	u_int8_t	sc_swflags;
#define COM_SW_SOFTCAR  0x01
#define COM_SW_CLOCAL   0x02
#define COM_SW_CRTSCTS  0x04
#define COM_SW_MDMBUF   0x08
#define COM_SW_PPS      0x10
	int		sc_fifolen;

	u_int8_t	sc_initialize;
	u_int8_t	sc_cua;
	u_int16_t 	*sc_ibuf, *sc_ibufp, *sc_ibufhigh, *sc_ibufend;
#define UART_IBUFSIZE 128
#define UART_IHIGHWATER 100
	u_int16_t		sc_ibufs[2][UART_IBUFSIZE];

	struct clk	*sc_clk;
};

int  pluartprobe(struct device *parent, void *self, void *aux);
void pluartattach(struct device *parent, struct device *self, void *aux);

void pluartcnprobe(struct consdev *cp);
void pluartcnprobe(struct consdev *cp);
void pluartcninit(struct consdev *cp);
int pluartcnattach(bus_space_tag_t iot, bus_addr_t iobase, int rate,
    tcflag_t cflag);
int pluartcngetc(dev_t dev);
void pluartcnputc(dev_t dev, int c);
void pluartcnpollc(dev_t dev, int on);
int  pluart_param(struct tty *tp, struct termios *t);
void pluart_start(struct tty *);
void pluart_pwroff(struct pluart_softc *sc);
void pluart_diag(void *arg);
void pluart_raisedtr(void *arg);
void pluart_softint(void *arg);
struct pluart_softc *pluart_sc(dev_t dev);

int pluart_intr(void *);

/* XXX - we imitate 'com' serial ports and take over their entry points */
/* XXX: These belong elsewhere */
cdev_decl(pluart);

struct cfdriver pluart_cd = {
	NULL, "pluart", DV_TTY
};

struct cfattach pluart_ca = {
	sizeof(struct pluart_softc), pluartprobe, pluartattach
};

bus_space_tag_t	pluartconsiot;
bus_space_handle_t pluartconsioh;
bus_addr_t	pluartconsaddr;
tcflag_t	pluartconscflag = TTYDEF_CFLAG;
int		pluartdefaultrate = B38400;

void
pluart_init_cons(void)
{
	struct fdt_reg reg;
	void *node;

	if ((node = fdt_find_cons("arm,pl011")) == NULL)
		return;
	if (fdt_get_reg(node, 0, &reg))
		return;

	pluartcnattach(&arm64_bs_tag, reg.addr, B115200, TTYDEF_CFLAG);
}

int
pluartprobe(struct device *parent, void *self, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "arm,pl011");
}

struct cdevsw pluartdev =
	cdev_tty_init(3/*XXX NUART */ ,pluart);		/* 8: serial port */

void
pluartattach(struct device *parent, struct device *self, void *aux)
{
	struct fdt_attach_args *faa = aux;
	struct pluart_softc *sc = (struct pluart_softc *) self;
	int maj;

	if (faa->fa_nreg < 1) {
		printf(": no register data\n");
		return;
	}

	sc->sc_irq = arm_intr_establish_fdt(faa->fa_node, IPL_TTY, pluart_intr,
	    sc, sc->sc_dev.dv_xname);

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr, faa->fa_reg[0].size,
	    0, &sc->sc_ioh))
		panic("pluartattach: bus_space_map failed!");

	if (stdout_node == faa->fa_node) {
		/* Locate the major number. */
		for (maj = 0; maj < nchrdev; maj++)
			if (cdevsw[maj].d_open == pluartopen)
				break;
		cn_tab->cn_dev = makedev(maj, sc->sc_dev.dv_unit);

		printf(": console");
	}

	timeout_set(&sc->sc_diag_tmo, pluart_diag, sc);
	timeout_set(&sc->sc_dtr_tmo, pluart_raisedtr, sc);
	sc->sc_si = softintr_establish(IPL_TTY, pluart_softint, sc);

	if(sc->sc_si == NULL)
		panic("%s: can't establish soft interrupt.",
		    sc->sc_dev.dv_xname);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, UART_IMSC, (UART_IMSC_RXIM | UART_IMSC_TXIM));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, UART_ICR, 0x7ff);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, UART_LCR_H,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, UART_LCR_H) &
	    ~UART_LCR_H_FEN);

	printf("\n");
}

int
pluart_intr(void *arg)
{
	struct pluart_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp = sc->sc_tty;
	u_int16_t fr;
	u_int16_t *p;
	u_int16_t c;

	bus_space_write_4(iot, ioh, UART_ICR, -1);

	if (sc->sc_tty == NULL)
		return(0);

	fr = bus_space_read_4(iot, ioh, UART_FR);
	if (ISSET(fr, UART_FR_TXFE) && ISSET(tp->t_state, TS_BUSY)) {
		CLR(tp->t_state, TS_BUSY | TS_FLUSH);
		if (sc->sc_halt > 0)
			wakeup(&tp->t_outq);
		(*linesw[tp->t_line].l_start)(tp);
	}

	if(!ISSET(bus_space_read_4(iot, ioh, UART_FR), UART_FR_RXFF))
		return 0;

	p = sc->sc_ibufp;

	while(ISSET(bus_space_read_4(iot, ioh, UART_FR), UART_FR_RXFF)) {
		c = bus_space_read_1(iot, ioh, UART_DR);
		if (p >= sc->sc_ibufend) {
			sc->sc_floods++;
			if (sc->sc_errors++ == 0)
				timeout_add(&sc->sc_diag_tmo, 60 * hz);
		} else {
			*p++ = c;
			if (p == sc->sc_ibufhigh && ISSET(tp->t_cflag, CRTSCTS)) {
				/* XXX */
				//CLR(sc->sc_ucr3, IMXUART_CR3_DSR);
				//bus_space_write_4(iot, ioh, IMXUART_UCR3,
				//    sc->sc_ucr3);
			}
		}
		/* XXX - msr stuff ? */
	}
	sc->sc_ibufp = p;

	softintr_schedule(sc->sc_si);

	return 1;
}

int
pluart_param(struct tty *tp, struct termios *t)
{
	struct pluart_softc *sc = pluart_cd.cd_devs[DEVUNIT(tp->t_dev)];
	//bus_space_tag_t iot = sc->sc_iot;
	//bus_space_handle_t ioh = sc->sc_ioh;
	int ospeed = t->c_ospeed;
	int error;
	tcflag_t oldcflag;


	if (t->c_ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return EINVAL;

	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		return EINVAL;
	case CS6:
		return EINVAL;
	case CS7:
		//CLR(sc->sc_ucr2, IMXUART_CR2_WS);
		break;
	case CS8:
		//SET(sc->sc_ucr2, IMXUART_CR2_WS);
		break;
	}
//	bus_space_write_4(iot, ioh, IMXUART_UCR2, sc->sc_ucr2);

	/*
	if (ISSET(t->c_cflag, PARENB)) {
		SET(sc->sc_ucr2, IMXUART_CR2_PREN);
		bus_space_write_4(iot, ioh, IMXUART_UCR2, sc->sc_ucr2);
	}
	*/
	/* STOPB - XXX */
	if (ospeed == 0) {
		/* lower dtr */
	}

	if (ospeed != 0) {
		while (ISSET(tp->t_state, TS_BUSY)) {
			++sc->sc_halt;
			error = ttysleep(tp, &tp->t_outq,
			    TTOPRI | PCATCH, "pluartprm", 0);
			--sc->sc_halt;
			if (error) {
				pluart_start(tp);
				return (error);
			}
		}
		/* set speed */
	}

	/* setup fifo */

	/* When not using CRTSCTS, RTS follows DTR. */
	/* sc->sc_dtr = MCR_DTR; */


	/* and copy to tty */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	oldcflag = tp->t_cflag;
	tp->t_cflag = t->c_cflag;

        /*
	 * If DCD is off and MDMBUF is changed, ask the tty layer if we should
	 * stop the device.
	 */
	 /* XXX */

	pluart_start(tp);

	return 0;
}

void
pluart_start(struct tty *tp)
{
	struct pluart_softc *sc = pluart_cd.cd_devs[DEVUNIT(tp->t_dev)];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	int s;
	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP))
		goto out;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (ISSET(tp->t_state, TS_ASLEEP)) {
			CLR(tp->t_state, TS_ASLEEP);
			wakeup(&tp->t_outq);
		}
		if (tp->t_outq.c_cc == 0)
			goto out;
		selwakeup(&tp->t_wsel);
	}
	SET(tp->t_state, TS_BUSY);

	if (ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
		u_char buffer[64];	/* largest fifo */
		int i, n;

		n = q_to_b(&tp->t_outq, buffer,
		    min(sc->sc_fifolen, sizeof buffer));
		for (i = 0; i < n; i++) {
			bus_space_write_4(iot, ioh, UART_DR, buffer[i]);
		}
		bzero(buffer, n);
	} else if (tp->t_outq.c_cc != 0)
		bus_space_write_4(iot, ioh, UART_DR, getc(&tp->t_outq));

out:
	splx(s);
}

void
pluart_pwroff(struct pluart_softc *sc)
{
}

void
pluart_diag(void *arg)
{
	struct pluart_softc *sc = arg;
	int overflows, floods;
	int s;

	s = spltty();
	sc->sc_errors = 0;
	overflows = sc->sc_overflows;
	sc->sc_overflows = 0;
	floods = sc->sc_floods;
	sc->sc_floods = 0;
	splx(s);
	log(LOG_WARNING, "%s: %d silo overflow%s, %d ibuf overflow%s\n",
	    sc->sc_dev.dv_xname,
	    overflows, overflows == 1 ? "" : "s",
	    floods, floods == 1 ? "" : "s");
}

void
pluart_raisedtr(void *arg)
{
	//struct pluart_softc *sc = arg;

	//SET(sc->sc_ucr3, IMXUART_CR3_DSR); /* XXX */
	//bus_space_write_4(sc->sc_iot, sc->sc_ioh, IMXUART_UCR3, sc->sc_ucr3);
}

void
pluart_softint(void *arg)
{
	struct pluart_softc *sc = arg;
	struct tty *tp;
	u_int16_t *ibufp;
	u_int16_t *ibufend;
	int c;
	int err;
	int s;

	if (sc == NULL || sc->sc_ibufp == sc->sc_ibuf)
		return;

	tp = sc->sc_tty;
	s = spltty();

	ibufp = sc->sc_ibuf;
	ibufend = sc->sc_ibufp;

	if (ibufp == ibufend || tp == NULL || !ISSET(tp->t_state, TS_ISOPEN)) {
		splx(s);
		return;
	}

	sc->sc_ibufp = sc->sc_ibuf = (ibufp == sc->sc_ibufs[0]) ?
	    sc->sc_ibufs[1] : sc->sc_ibufs[0];
	sc->sc_ibufhigh = sc->sc_ibuf + UART_IHIGHWATER;
	sc->sc_ibufend = sc->sc_ibuf + UART_IBUFSIZE;

#if 0
	if (ISSET(tp->t_cflag, CRTSCTS) &&
	    !ISSET(sc->sc_ucr3, IMXUART_CR3_DSR)) {
		/* XXX */
		SET(sc->sc_ucr3, IMXUART_CR3_DSR);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, IMXUART_UCR3,
		    sc->sc_ucr3);
	}
#endif

	splx(s);

	while (ibufp < ibufend) {
		c = *ibufp++;
		/*
		if (ISSET(c, IMXUART_RX_OVERRUN)) {
			sc->sc_overflows++;
			if (sc->sc_errors++ == 0)
				timeout_add(&sc->sc_diag_tmo, 60 * hz);
		}
		*/
		/* This is ugly, but fast. */

		err = 0;
		/*
		if (ISSET(c, IMXUART_RX_PRERR))
			err |= TTY_PE;
		if (ISSET(c, IMXUART_RX_FRMERR))
			err |= TTY_FE;
		*/
		c = (c & 0xff) | err;
		(*linesw[tp->t_line].l_rint)(c, tp);
	}
}

int
pluartopen(dev_t dev, int flag, int mode, struct proc *p)
{
	int unit = DEVUNIT(dev);
	struct pluart_softc *sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct tty *tp;
	int s;
	int error = 0;

	if (unit >= pluart_cd.cd_ndevs)
		return ENXIO;
	sc = pluart_cd.cd_devs[unit];
	if (sc == NULL)
		return ENXIO;

	s = spltty();
	if (sc->sc_tty == NULL)
		tp = sc->sc_tty = ttymalloc(0);
	else
		tp = sc->sc_tty;

	splx(s);

	tp->t_oproc = pluart_start;
	tp->t_param = pluart_param;
	tp->t_dev = dev;

	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		SET(tp->t_state, TS_WOPEN);
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;

		if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			tp->t_cflag = pluartconscflag;
		else
			tp->t_cflag = TTYDEF_CFLAG;
		if (ISSET(sc->sc_swflags, COM_SW_CLOCAL))
			SET(tp->t_cflag, CLOCAL);
		if (ISSET(sc->sc_swflags, COM_SW_CRTSCTS))
			SET(tp->t_cflag, CRTSCTS);
		if (ISSET(sc->sc_swflags, COM_SW_MDMBUF))
			SET(tp->t_cflag, MDMBUF);
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = pluartdefaultrate;

		s = spltty();

		sc->sc_initialize = 1;
		pluart_param(tp, &tp->t_termios);
		ttsetwater(tp);
		sc->sc_ibufp = sc->sc_ibuf = sc->sc_ibufs[0];
		sc->sc_ibufhigh = sc->sc_ibuf + UART_IHIGHWATER;
		sc->sc_ibufend = sc->sc_ibuf + UART_IBUFSIZE;

		iot = sc->sc_iot;
		ioh = sc->sc_ioh;

#if 0
		sc->sc_ucr1 = bus_space_read_4(iot, ioh, IMXUART_UCR1);
		sc->sc_ucr2 = bus_space_read_4(iot, ioh, IMXUART_UCR2);
		sc->sc_ucr3 = bus_space_read_4(iot, ioh, IMXUART_UCR3);
		sc->sc_ucr4 = bus_space_read_4(iot, ioh, IMXUART_UCR4);

		/* interrupt after one char on tx/rx */
		/* reference frequency divider: 1 */
		bus_space_write_4(iot, ioh, IMXUART_UFCR,
		    1 << IMXUART_FCR_TXTL_SH |
		    5 << IMXUART_FCR_RFDIV_SH |
		    1 << IMXUART_FCR_RXTL_SH);

		bus_space_write_4(iot, ioh, IMXUART_UBIR,
		    (pluartdefaultrate / 100) - 1);

		/* formula: clk / (rfdiv * 1600) */
		bus_space_write_4(iot, ioh, IMXUART_UBMR,
		    (clk_get_rate(sc->sc_clk) * 1000) / 1600);

		SET(sc->sc_ucr1, IMXUART_CR1_EN|IMXUART_CR1_RRDYEN);
		SET(sc->sc_ucr2, IMXUART_CR2_TXEN|IMXUART_CR2_RXEN);
		bus_space_write_4(iot, ioh, IMXUART_UCR1, sc->sc_ucr1);
		bus_space_write_4(iot, ioh, IMXUART_UCR2, sc->sc_ucr2);

		/* sc->sc_mcr = MCR_DTR | MCR_RTS;  XXX */
		SET(sc->sc_ucr3, IMXUART_CR3_DSR); /* XXX */
		bus_space_write_4(iot, ioh, IMXUART_UCR3, sc->sc_ucr3);
#endif

		SET(tp->t_state, TS_CARR_ON); /* XXX */


	} else if (ISSET(tp->t_state, TS_XCLUDE) && p->p_ucred->cr_uid != 0)
		return EBUSY;
	else
		s = spltty();

	if (DEVCUA(dev)) {
		if (ISSET(tp->t_state, TS_ISOPEN)) {
			splx(s);
			return EBUSY;
		}
		sc->sc_cua = 1;
	} else {
		/* tty (not cua) device; wait for carrier if necessary */
		if (ISSET(flag, O_NONBLOCK)) {
			if (sc->sc_cua) {
				/* Opening TTY non-blocking... but the CUA is busy */
				splx(s);
				return EBUSY;
			}
		} else {
			while (sc->sc_cua ||
			    (!ISSET(tp->t_cflag, CLOCAL) &&
				!ISSET(tp->t_state, TS_CARR_ON))) {
				SET(tp->t_state, TS_WOPEN);
				error = ttysleep(tp, &tp->t_rawq,
				    TTIPRI | PCATCH, ttopen, 0);
				/*
				 * If TS_WOPEN has been reset, that means the
				 * cua device has been closed.  We don't want
				 * to fail in that case,
				 * so just go around again.
				 */
				if (error && ISSET(tp->t_state, TS_WOPEN)) {
					CLR(tp->t_state, TS_WOPEN);
					if (!sc->sc_cua && !ISSET(tp->t_state,
					    TS_ISOPEN))
						pluart_pwroff(sc);
					splx(s);
					return error;
				}
			}
		}
	}
	splx(s);
	return (*linesw[tp->t_line].l_open)(dev,tp,p);
}

int
pluartclose(dev_t dev, int flag, int mode, struct proc *p)
{
	int unit = DEVUNIT(dev);
	struct pluart_softc *sc = pluart_cd.cd_devs[unit];
	//bus_space_tag_t iot = sc->sc_iot;
	//bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp = sc->sc_tty;
	int s;

	/* XXX This is for cons.c. */
	if (!ISSET(tp->t_state, TS_ISOPEN))
		return 0;

	(*linesw[tp->t_line].l_close)(tp, flag, p);
	s = spltty();
	if (ISSET(tp->t_state, TS_WOPEN)) {
		/* tty device is waiting for carrier; drop dtr then re-raise */
		//CLR(sc->sc_ucr3, IMXUART_CR3_DSR);
		//bus_space_write_4(iot, ioh, IMXUART_UCR3, sc->sc_ucr3);
		timeout_add(&sc->sc_dtr_tmo, hz * 2);
	} else {
		/* no one else waiting; turn off the uart */
		pluart_pwroff(sc);
	}
	CLR(tp->t_state, TS_BUSY | TS_FLUSH);

	sc->sc_cua = 0;
	splx(s);
	ttyclose(tp);

	return 0;
}

int
pluartread(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tty;

	tty = pluarttty(dev);
	if (tty == NULL)
		return ENODEV;

	return((*linesw[tty->t_line].l_read)(tty, uio, flag));
}

int
pluartwrite(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tty;

	tty = pluarttty(dev);
	if (tty == NULL)
		return ENODEV;

	return((*linesw[tty->t_line].l_write)(tty, uio, flag));
}

int
pluartioctl( dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct pluart_softc *sc;
	struct tty *tp;
	int error;

	sc = pluart_sc(dev);
	if (sc == NULL)
		return (ENODEV);

	tp = sc->sc_tty;
	if (tp == NULL)
		return (ENXIO);

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch(cmd) {
	case TIOCSBRK:
		break;
	case TIOCCBRK:
		break;
	case TIOCSDTR:
		break;
	case TIOCCDTR:
		break;
	case TIOCMSET:
		break;
	case TIOCMBIS:
		break;
	case TIOCMBIC:
		break;
	case TIOCMGET:
		break;
	case TIOCGFLAGS:
		break;
	case TIOCSFLAGS:
		error = suser(p, 0);
		if (error != 0)
			return(EPERM);
		break;
	default:
		return (ENOTTY);
	}

	return 0;
}

int
pluartstop(struct tty *tp, int flag)
{
	return 0;
}

struct tty *
pluarttty(dev_t dev)
{
	int unit;
	struct pluart_softc *sc;
	unit = DEVUNIT(dev);
	if (unit >= pluart_cd.cd_ndevs)
		return NULL;
	sc = (struct pluart_softc *)pluart_cd.cd_devs[unit];
	if (sc == NULL)
		return NULL;
	return sc->sc_tty;
}

struct pluart_softc *
pluart_sc(dev_t dev)
{
	int unit;
	struct pluart_softc *sc;
	unit = DEVUNIT(dev);
	if (unit >= pluart_cd.cd_ndevs)
		return NULL;
	sc = (struct pluart_softc *)pluart_cd.cd_devs[unit];
	return sc;
}


/* serial console */
void
pluartcnprobe(struct consdev *cp)
{
	cp->cn_dev = makedev(8 /* XXX */, 0);
	cp->cn_pri = CN_MIDPRI;
}

void
pluartcninit(struct consdev *cp)
{
}

int
pluartcnattach(bus_space_tag_t iot, bus_addr_t iobase, int rate, tcflag_t cflag)
{
	static struct consdev pluartcons = {
		NULL, NULL, pluartcngetc, pluartcnputc, pluartcnpollc, NULL,
		NODEV, CN_MIDPRI
	};

	if (bus_space_map(iot, iobase, UART_SPACE, 0, &pluartconsioh))
			return ENOMEM;

	/* Disable FIFO. */
	bus_space_write_4(iot, pluartconsioh, UART_LCR_H,
	    bus_space_read_4(iot, pluartconsioh, UART_LCR_H) & ~UART_LCR_H_FEN);

	cn_tab = &pluartcons;
	cn_tab->cn_dev = makedev(8 /* XXX */, 0);
	cdevsw[8] = pluartdev; 	/* KLUDGE */

	pluartconsiot = iot;
	pluartconsaddr = iobase;
	pluartconscflag = cflag;

	return 0;
}

int
pluartcngetc(dev_t dev)
{
	int c;
	int s;
	s = splhigh();
	while((bus_space_read_4(pluartconsiot, pluartconsioh, UART_FR) &
	    UART_FR_RXFF) == 0)
		;
	c = bus_space_read_4(pluartconsiot, pluartconsioh, UART_DR);
	splx(s);
	return c;
}

void
pluartcnputc(dev_t dev, int c)
{
	int s;
	s = splhigh();
	while((bus_space_read_4(pluartconsiot, pluartconsioh, UART_FR) &
	    UART_FR_TXFE) == 0)
		;
	bus_space_write_4(pluartconsiot, pluartconsioh, UART_DR, (uint8_t)c);
	splx(s);
}

void
pluartcnpollc(dev_t dev, int on)
{
}
