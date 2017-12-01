/* $OpenBSD: dwiic.c,v 1.2 2017/12/01 16:06:25 kettenis Exp $ */
/*
 * Synopsys DesignWare I2C controller
 *
 * Copyright (c) 2015-2017 joshua stein <jcs@openbsd.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <dev/i2c/i2cvar.h>

#include <dev/ic/dwiicvar.h>

int		dwiic_match(struct device *, void *, void *);
void		dwiic_attach(struct device *, struct device *, void *);
int		dwiic_detach(struct device *, int);
int		dwiic_activate(struct device *, int);

int		dwiic_init(struct dwiic_softc *);
void		dwiic_enable(struct dwiic_softc *, int);
int		dwiic_intr(void *);

void *		dwiic_i2c_intr_establish(void *, void *, int,
		    int (*)(void *), void *, const char *);
const char *	dwiic_i2c_intr_string(void *, void *);

int		dwiic_i2c_acquire_bus(void *, int);
void		dwiic_i2c_release_bus(void *, int);
uint32_t	dwiic_read(struct dwiic_softc *, int);
void		dwiic_write(struct dwiic_softc *, int, uint32_t);
int		dwiic_i2c_exec(void *, i2c_op_t, i2c_addr_t, const void *,
		    size_t, void *, size_t, int);
void		dwiic_xfer_msg(struct dwiic_softc *);

struct cfdriver dwiic_cd = {
	NULL, "dwiic", DV_DULL
};

int
dwiic_detach(struct device *self, int flags)
{
	struct dwiic_softc *sc = (struct dwiic_softc *)self;

	if (sc->sc_ih != NULL) {
		intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;
	}

	return 0;
}

int
dwiic_activate(struct device *self, int act)
{
	struct dwiic_softc *sc = (struct dwiic_softc *)self;

	switch (act) {
	case DVACT_SUSPEND:
		/* disable controller */
		dwiic_enable(sc, 0);

		/* disable interrupts */
		dwiic_write(sc, DW_IC_INTR_MASK, 0);
		dwiic_read(sc, DW_IC_CLR_INTR);

#if notyet
		/* power down the controller */
		dwiic_acpi_power(sc, 0);
#endif
		break;
	case DVACT_WAKEUP:
#if notyet
		/* power up the controller */
		dwiic_acpi_power(sc, 1);
#endif
		dwiic_init(sc);

		break;
	}

	config_activate_children(self, act);

	return 0;
}

int
dwiic_i2c_print(void *aux, const char *pnp)
{
	struct i2c_attach_args *ia = aux;

	if (pnp != NULL)
		printf("\"%s\" at %s", ia->ia_name, pnp);

	printf(" addr 0x%x", ia->ia_addr);

	return UNCONF;
}

uint32_t
dwiic_read(struct dwiic_softc *sc, int offset)
{
	u_int32_t b = bus_space_read_4(sc->sc_iot, sc->sc_ioh, offset);

	DPRINTF(("%s: read at 0x%x = 0x%x\n", sc->sc_dev.dv_xname, offset, b));

	return b;
}

void
dwiic_write(struct dwiic_softc *sc, int offset, uint32_t val)
{
	DPRINTF(("%s: write at 0x%x: 0x%x\n", sc->sc_dev.dv_xname, offset,
	    val));

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, offset, val);
}

int
dwiic_i2c_acquire_bus(void *cookie, int flags)
{
	struct dwiic_softc *sc = cookie;

	if (cold || sc->sc_poll || (flags & I2C_F_POLL))
		return (0);

	return rw_enter(&sc->sc_i2c_lock, RW_WRITE | RW_INTR);
}

void
dwiic_i2c_release_bus(void *cookie, int flags)
{
	struct dwiic_softc *sc = cookie;

	if (cold || sc->sc_poll || (flags & I2C_F_POLL))
		return;

	rw_exit(&sc->sc_i2c_lock);
}

int
dwiic_init(struct dwiic_softc *sc)
{
	uint32_t reg;

	/* make sure we're talking to a device we know */
	reg = dwiic_read(sc, DW_IC_COMP_TYPE);
	if (reg != DW_IC_COMP_TYPE_VALUE) {
		DPRINTF(("%s: invalid component type 0x%x\n",
		    sc->sc_dev.dv_xname, reg));
		return 1;
	}

	/* disable the adapter */
	dwiic_enable(sc, 0);

	/* write standard-mode SCL timing parameters */
	dwiic_write(sc, DW_IC_SS_SCL_HCNT, sc->ss_hcnt);
	dwiic_write(sc, DW_IC_SS_SCL_LCNT, sc->ss_lcnt);

	/* and fast-mode SCL timing parameters */
	dwiic_write(sc, DW_IC_FS_SCL_HCNT, sc->fs_hcnt);
	dwiic_write(sc, DW_IC_FS_SCL_LCNT, sc->fs_lcnt);

	/* SDA hold time */
	reg = dwiic_read(sc, DW_IC_COMP_VERSION);
	if (reg >= DW_IC_SDA_HOLD_MIN_VERS)
		dwiic_write(sc, DW_IC_SDA_HOLD, sc->sda_hold_time);

	/* FIFO threshold levels */
	sc->tx_fifo_depth = 32;
	sc->rx_fifo_depth = 32;
	dwiic_write(sc, DW_IC_TX_TL, sc->tx_fifo_depth / 2);
	dwiic_write(sc, DW_IC_RX_TL, 0);

	/* configure as i2c master with fast speed */
	sc->master_cfg = DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE |
	    DW_IC_CON_RESTART_EN | DW_IC_CON_SPEED_FAST;
	dwiic_write(sc, DW_IC_CON, sc->master_cfg);

	return 0;
}

void
dwiic_enable(struct dwiic_softc *sc, int enable)
{
	int retries;

	for (retries = 100; retries > 0; retries--) {
		dwiic_write(sc, DW_IC_ENABLE, enable);
		if ((dwiic_read(sc, DW_IC_ENABLE_STATUS) & 1) == enable)
			return;

		DELAY(25);
	}

	printf("%s: failed to %sable\n", sc->sc_dev.dv_xname,
	    (enable ? "en" : "dis"));
}

int
dwiic_i2c_exec(void *cookie, i2c_op_t op, i2c_addr_t addr, const void *cmdbuf,
    size_t cmdlen, void *buf, size_t len, int flags)
{
	struct dwiic_softc *sc = cookie;
	u_int32_t ic_con, st, cmd, resp;
	int retries, tx_limit, rx_avail, x, readpos;
	uint8_t *b;

	if (sc->sc_busy)
		return 1;

	sc->sc_busy++;

	DPRINTF(("%s: %s: op %d, addr 0x%02x, cmdlen %zu, len %zu, "
	    "flags 0x%02x\n", sc->sc_dev.dv_xname, __func__, op, addr, cmdlen,
	    len, flags));

	/* setup transfer */
	sc->sc_i2c_xfer.op = op;
	sc->sc_i2c_xfer.buf = buf;
	sc->sc_i2c_xfer.len = len;
	sc->sc_i2c_xfer.flags = flags;
	sc->sc_i2c_xfer.error = 0;

	/* wait for bus to be idle */
	for (retries = 100; retries > 0; retries--) {
		st = dwiic_read(sc, DW_IC_STATUS);
		if (!(st & DW_IC_STATUS_ACTIVITY))
			break;
		DELAY(1000);
	}
	DPRINTF(("%s: %s: status 0x%x\n", sc->sc_dev.dv_xname, __func__, st));
	if (st & DW_IC_STATUS_ACTIVITY) {
		sc->sc_busy = 0;
		return (1);
	}

	if (cold || sc->sc_poll)
		flags |= I2C_F_POLL;

	/* disable controller */
	dwiic_enable(sc, 0);

	/* set slave address */
	ic_con = dwiic_read(sc, DW_IC_CON);
	ic_con &= ~DW_IC_CON_10BITADDR_MASTER;
	dwiic_write(sc, DW_IC_CON, ic_con);
	dwiic_write(sc, DW_IC_TAR, addr);

	/* disable interrupts */
	dwiic_write(sc, DW_IC_INTR_MASK, 0);
	dwiic_read(sc, DW_IC_CLR_INTR);

	/* enable controller */
	dwiic_enable(sc, 1);

	/* wait until the controller is ready for commands */
	if (flags & I2C_F_POLL)
		DELAY(200);
	else {
		dwiic_read(sc, DW_IC_CLR_INTR);
		dwiic_write(sc, DW_IC_INTR_MASK, DW_IC_INTR_TX_EMPTY);

		if (tsleep(&sc->sc_writewait, PRIBIO, "dwiic", hz / 2) != 0)
			printf("%s: timed out waiting for tx_empty intr\n",
			    sc->sc_dev.dv_xname);
	}

	/* send our command, one byte at a time */
	if (cmdlen > 0) {
		b = (void *)cmdbuf;

		DPRINTF(("%s: %s: sending cmd (len %zu):", sc->sc_dev.dv_xname,
		    __func__, cmdlen));
		for (x = 0; x < cmdlen; x++)
			DPRINTF((" %02x", b[x]));
		DPRINTF(("\n"));

		tx_limit = sc->tx_fifo_depth - dwiic_read(sc, DW_IC_TXFLR);
		if (cmdlen > tx_limit) {
			/* TODO */
			printf("%s: can't write %zu (> %d)\n",
			    sc->sc_dev.dv_xname, cmdlen, tx_limit);
			sc->sc_i2c_xfer.error = 1;
			sc->sc_busy = 0;
			return (1);
		}

		for (x = 0; x < cmdlen; x++) {
			cmd = b[x];
			/*
			 * Generate STOP condition if this is the last
			 * byte of the transfer.
			 */
			if (x == (cmdlen - 1) && len == 0 && I2C_OP_STOP_P(op))
				cmd |= DW_IC_DATA_CMD_STOP;
			dwiic_write(sc, DW_IC_DATA_CMD, cmd);
		}
	}

	b = (void *)buf;
	x = readpos = 0;
	tx_limit = sc->tx_fifo_depth - dwiic_read(sc, DW_IC_TXFLR);

	DPRINTF(("%s: %s: need to read %zu bytes, can send %d read reqs\n",
		sc->sc_dev.dv_xname, __func__, len, tx_limit));

	while (x < len) {
		if (I2C_OP_WRITE_P(op))
			cmd = b[x];
		else
			cmd = DW_IC_DATA_CMD_READ;

		/*
		 * Generate RESTART condition if we're reversing
		 * direction.
		 */
		if (x == 0 && cmdlen > 0 && I2C_OP_READ_P(op))
			cmd |= DW_IC_DATA_CMD_RESTART;
		/*
		 * Generate STOP conditon on the last byte of the
		 * transfer.
		 */
		if (x == (len - 1) && I2C_OP_STOP_P(op))
			cmd |= DW_IC_DATA_CMD_STOP;

		dwiic_write(sc, DW_IC_DATA_CMD, cmd);

		tx_limit--;
		x++;

		/*
		 * As TXFLR fills up, we need to clear it out by reading all
		 * available data.
		 */
		while (tx_limit == 0 || x == len) {
			DPRINTF(("%s: %s: tx_limit %d, sent %d read reqs\n",
			    sc->sc_dev.dv_xname, __func__, tx_limit, x));

			if (flags & I2C_F_POLL) {
				for (retries = 100; retries > 0; retries--) {
					rx_avail = dwiic_read(sc, DW_IC_RXFLR);
					if (rx_avail > 0)
						break;
					DELAY(50);
				}
			} else {
				dwiic_read(sc, DW_IC_CLR_INTR);
				dwiic_write(sc, DW_IC_INTR_MASK,
				    DW_IC_INTR_RX_FULL);

				if (tsleep(&sc->sc_readwait, PRIBIO, "dwiic",
				    hz / 2) != 0)
					printf("%s: timed out waiting for "
					    "rx_full intr\n",
					    sc->sc_dev.dv_xname);

				rx_avail = dwiic_read(sc, DW_IC_RXFLR);
			}

			if (rx_avail == 0) {
				printf("%s: timed out reading remaining %d\n",
				    sc->sc_dev.dv_xname,
				    (int)(len - 1 - readpos));
				sc->sc_i2c_xfer.error = 1;
				sc->sc_busy = 0;

				return (1);
			}

			DPRINTF(("%s: %s: %d avail to read (%zu remaining)\n",
			    sc->sc_dev.dv_xname, __func__, rx_avail,
			    len - readpos));

			while (rx_avail > 0) {
				resp = dwiic_read(sc, DW_IC_DATA_CMD);
				if (readpos < len) {
					b[readpos] = resp;
					readpos++;
				}
				rx_avail--;
			}

			if (readpos >= len)
				break;

			DPRINTF(("%s: still need to read %d bytes\n",
			    sc->sc_dev.dv_xname, (int)(len - readpos)));
			tx_limit = sc->tx_fifo_depth -
			    dwiic_read(sc, DW_IC_TXFLR);
		}
	}

	sc->sc_busy = 0;

	return 0;
}

uint32_t
dwiic_read_clear_intrbits(struct dwiic_softc *sc)
{
       uint32_t stat;

       stat = dwiic_read(sc, DW_IC_INTR_STAT);

       if (stat & DW_IC_INTR_RX_UNDER)
	       dwiic_read(sc, DW_IC_CLR_RX_UNDER);
       if (stat & DW_IC_INTR_RX_OVER)
	       dwiic_read(sc, DW_IC_CLR_RX_OVER);
       if (stat & DW_IC_INTR_TX_OVER)
	       dwiic_read(sc, DW_IC_CLR_TX_OVER);
       if (stat & DW_IC_INTR_RD_REQ)
	       dwiic_read(sc, DW_IC_CLR_RD_REQ);
       if (stat & DW_IC_INTR_TX_ABRT)
	       dwiic_read(sc, DW_IC_CLR_TX_ABRT);
       if (stat & DW_IC_INTR_RX_DONE)
	       dwiic_read(sc, DW_IC_CLR_RX_DONE);
       if (stat & DW_IC_INTR_ACTIVITY)
	       dwiic_read(sc, DW_IC_CLR_ACTIVITY);
       if (stat & DW_IC_INTR_STOP_DET)
	       dwiic_read(sc, DW_IC_CLR_STOP_DET);
       if (stat & DW_IC_INTR_START_DET)
	       dwiic_read(sc, DW_IC_CLR_START_DET);
       if (stat & DW_IC_INTR_GEN_CALL)
	       dwiic_read(sc, DW_IC_CLR_GEN_CALL);

       return stat;
}

int
dwiic_intr(void *arg)
{
	struct dwiic_softc *sc = arg;
	uint32_t en, stat;

	en = dwiic_read(sc, DW_IC_ENABLE);
	/* probably for the other controller */
	if (!en)
		return 0;

	stat = dwiic_read_clear_intrbits(sc);
	DPRINTF(("%s: %s: enabled=0x%x stat=0x%x\n", sc->sc_dev.dv_xname,
	    __func__, en, stat));
	if (!(stat & ~DW_IC_INTR_ACTIVITY))
		return 1;

	if (stat & DW_IC_INTR_TX_ABRT)
		sc->sc_i2c_xfer.error = 1;

	if (sc->sc_i2c_xfer.flags & I2C_F_POLL)
		DPRINTF(("%s: %s: intr in poll mode?\n", sc->sc_dev.dv_xname,
		    __func__));
	else {
		if (stat & DW_IC_INTR_RX_FULL) {
			dwiic_write(sc, DW_IC_INTR_MASK, 0);
			DPRINTF(("%s: %s: waking up reader\n",
			    sc->sc_dev.dv_xname, __func__));
			wakeup(&sc->sc_readwait);
		}
		if (stat & DW_IC_INTR_TX_EMPTY) {
			dwiic_write(sc, DW_IC_INTR_MASK, 0);
			DPRINTF(("%s: %s: waking up writer\n",
			    sc->sc_dev.dv_xname, __func__));
			wakeup(&sc->sc_writewait);
		}
	}

	return 1;
}
