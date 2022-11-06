/*	$OpenBSD: qcgpio_fdt.c,v 1.1 2022/11/06 15:33:58 patrick Exp $	*/
/*
 * Copyright (c) 2022 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_gpio.h>
#include <dev/ofw/fdt.h>

/* Registers. */
#define TLMM_GPIO_IN_OUT(pin)		(0x0004 + 0x1000 * (pin))
#define  TLMM_GPIO_IN_OUT_GPIO_IN			(1 << 0)
#define  TLMM_GPIO_IN_OUT_GPIO_OUT			(1 << 1)
#define TLMM_GPIO_INTR_CFG(pin)		(0x0008 + 0x1000 * (pin))
#define  TLMM_GPIO_INTR_CFG_TARGET_PROC_MASK		(0x7 << 5)
#define  TLMM_GPIO_INTR_CFG_TARGET_PROC_RPM		(0x3 << 5)
#define  TLMM_GPIO_INTR_CFG_INTR_RAW_STATUS_EN		(1 << 4)
#define  TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_MASK		(0x3 << 2)
#define  TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_LEVEL		(0x0 << 2)
#define  TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_EDGE_POS	(0x1 << 2)
#define  TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_EDGE_NEG	(0x2 << 2)
#define  TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_EDGE_BOTH	(0x3 << 2)
#define  TLMM_GPIO_INTR_CFG_INTR_POL_CTL		(1 << 1)
#define  TLMM_GPIO_INTR_CFG_INTR_ENABLE			(1 << 0)
#define TLMM_GPIO_INTR_STATUS(pin)	(0x000c + 0x1000 * (pin))
#define  TLMM_GPIO_INTR_STATUS_INTR_STATUS		(1 << 0)

#define HREAD4(sc, reg)							\
	(bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg)))
#define HWRITE4(sc, reg, val)						\
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define HSET4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) | (bits))
#define HCLR4(sc, reg, bits)						\
	HWRITE4((sc), (reg), HREAD4((sc), (reg)) & ~(bits))

struct qcgpio_intrhand {
	int (*ih_func)(void *);
	void *ih_arg;
	void *ih_sc;
	int ih_pin;
};

struct qcgpio_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	void			*sc_ih;

	uint32_t		sc_npins;
	struct qcgpio_intrhand	*sc_pin_ih;

	struct interrupt_controller sc_ic;
};

int	qcgpio_fdt_match(struct device *, void *, void *);
void	qcgpio_fdt_attach(struct device *, struct device *, void *);

const struct cfattach qcgpio_fdt_ca = {
	sizeof(struct qcgpio_softc), qcgpio_fdt_match, qcgpio_fdt_attach
};

int	qcgpio_fdt_read_pin(void *, int);
void	qcgpio_fdt_write_pin(void *, int, int);
void	*qcgpio_fdt_intr_establish(void *, int *, int, struct cpu_info *,
	    int (*)(void *), void *, char *);
void	qcgpio_fdt_intr_disestablish(void *);
void	qcgpio_fdt_intr_enable(void *);
void	qcgpio_fdt_intr_disable(void *);
void	qcgpio_fdt_intr_barrier(void *);
int	qcgpio_fdt_pin_intr(struct qcgpio_softc *, int);
int	qcgpio_fdt_intr(void *);

int
qcgpio_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "qcom,sc8280xp-tlmm");
}

void
qcgpio_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct fdt_attach_args *faa = aux;
	struct qcgpio_softc *sc = (struct qcgpio_softc *)self;

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr, faa->fa_reg[0].size,
	    0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_npins = 230;
	sc->sc_pin_ih = mallocarray(sc->sc_npins, sizeof(*sc->sc_pin_ih),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	sc->sc_ih = fdt_intr_establish(faa->fa_node, IPL_BIO, qcgpio_fdt_intr,
	    sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		goto unmap;
	}

	sc->sc_ic.ic_node = faa->fa_node;
	sc->sc_ic.ic_cookie = sc;
	sc->sc_ic.ic_establish = qcgpio_fdt_intr_establish;
	sc->sc_ic.ic_disestablish = qcgpio_fdt_intr_disestablish;
	sc->sc_ic.ic_enable = qcgpio_fdt_intr_enable;
	sc->sc_ic.ic_disable = qcgpio_fdt_intr_disable;
	sc->sc_ic.ic_barrier = qcgpio_fdt_intr_barrier;
	fdt_intr_register(&sc->sc_ic);

	printf("\n");
	return;

unmap:
	if (sc->sc_ih)
		fdt_intr_disestablish(sc->sc_ih);
	free(sc->sc_pin_ih, M_DEVBUF, sc->sc_npins * sizeof(*sc->sc_pin_ih));
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, faa->fa_reg[0].size);
}

int
qcgpio_fdt_read_pin(void *cookie, int pin)
{
	struct qcgpio_softc *sc = cookie;
	uint32_t reg;

	if (pin < 0 || pin >= sc->sc_npins)
		return 0;

	reg = HREAD4(sc, TLMM_GPIO_IN_OUT(pin));
	return !!(reg & TLMM_GPIO_IN_OUT_GPIO_IN);
}

void
qcgpio_fdt_write_pin(void *cookie, int pin, int val)
{
	struct qcgpio_softc *sc = cookie;

	if (pin < 0 || pin >= sc->sc_npins)
		return;

	if (val) {
		HSET4(sc, TLMM_GPIO_IN_OUT(pin),
		    TLMM_GPIO_IN_OUT_GPIO_OUT);
	} else {
		HCLR4(sc, TLMM_GPIO_IN_OUT(pin),
		    TLMM_GPIO_IN_OUT_GPIO_OUT);
	}
}

void *
qcgpio_fdt_intr_establish(void *cookie, int *cells, int ipl,
    struct cpu_info *ci, int (*func)(void *), void *arg, char *name)
{
	struct qcgpio_softc *sc = cookie;
	uint32_t reg;
	int pin = cells[0];
	int level = cells[1];

	if (pin < 0 || pin >= sc->sc_npins)
		return NULL;

	sc->sc_pin_ih[pin].ih_func = func;
	sc->sc_pin_ih[pin].ih_arg = arg;
	sc->sc_pin_ih[pin].ih_pin = pin;
	sc->sc_pin_ih[pin].ih_sc = sc;

	reg = HREAD4(sc, TLMM_GPIO_INTR_CFG(pin));
	reg &= ~TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_MASK;
	reg &= ~TLMM_GPIO_INTR_CFG_INTR_POL_CTL;
	switch (level) {
	case 1:
		reg |= TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_EDGE_POS |
		    TLMM_GPIO_INTR_CFG_INTR_POL_CTL;
		break;
	case 2:
		reg |= TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_EDGE_NEG |
		    TLMM_GPIO_INTR_CFG_INTR_POL_CTL;
		break;
	case 3:
		reg |= TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_EDGE_BOTH;
		break;
	case 4:
		reg |= TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_LEVEL |
		    TLMM_GPIO_INTR_CFG_INTR_POL_CTL;
		break;
	case 8:
		reg |= TLMM_GPIO_INTR_CFG_INTR_DECT_CTL_LEVEL;
		break;
	default:
		printf("%s: unsupported interrupt mode/polarity\n",
		    sc->sc_dev.dv_xname);
		break;
	}
	reg &= ~TLMM_GPIO_INTR_CFG_TARGET_PROC_MASK;
	reg |= TLMM_GPIO_INTR_CFG_TARGET_PROC_RPM;
	reg |= TLMM_GPIO_INTR_CFG_INTR_RAW_STATUS_EN;
	reg |= TLMM_GPIO_INTR_CFG_INTR_ENABLE;
	HWRITE4(sc, TLMM_GPIO_INTR_CFG(pin), reg);

	return &sc->sc_pin_ih[pin];
}

void
qcgpio_fdt_intr_disestablish(void *cookie)
{
	struct qcgpio_intrhand *ih = cookie;

	qcgpio_fdt_intr_disable(cookie);
	ih->ih_func = NULL;
}

void
qcgpio_fdt_intr_enable(void *cookie)
{
	struct qcgpio_intrhand *ih = cookie;
	struct qcgpio_softc *sc = ih->ih_sc;
	int pin = ih->ih_pin;

	if (pin < 0 || pin >= sc->sc_npins)
		return;

	HSET4(sc, TLMM_GPIO_INTR_CFG(pin),
	    TLMM_GPIO_INTR_CFG_INTR_ENABLE);
}

void
qcgpio_fdt_intr_disable(void *cookie)
{
	struct qcgpio_intrhand *ih = cookie;
	struct qcgpio_softc *sc = ih->ih_sc;
	int pin = ih->ih_pin;

	if (pin < 0 || pin >= sc->sc_npins)
		return;

	HCLR4(sc, TLMM_GPIO_INTR_CFG(pin),
	    TLMM_GPIO_INTR_CFG_INTR_ENABLE);
}

void
qcgpio_fdt_intr_barrier(void *cookie)
{
	struct qcgpio_intrhand *ih = cookie;
	struct qcgpio_softc *sc = ih->ih_sc;

	intr_barrier(sc->sc_ih);
}

int
qcgpio_fdt_intr(void *arg)
{
	struct qcgpio_softc *sc = arg;
	int pin, handled = 0;
	uint32_t stat;

	for (pin = 0; pin < sc->sc_npins; pin++) {
		if (sc->sc_pin_ih[pin].ih_func == NULL)
			continue;

		stat = HREAD4(sc, TLMM_GPIO_INTR_STATUS(pin));
		if (stat & TLMM_GPIO_INTR_STATUS_INTR_STATUS) {
			sc->sc_pin_ih[pin].ih_func(sc->sc_pin_ih[pin].ih_arg);
			handled = 1;
		}
		HWRITE4(sc, TLMM_GPIO_INTR_STATUS(pin),
		    stat & ~TLMM_GPIO_INTR_STATUS_INTR_STATUS);
	}

	return handled;
}
