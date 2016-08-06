/* $OpenBSD: omap_com.c,v 1.9 2016/08/06 10:07:45 jsg Exp $ */
/*
 * Copyright 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

/* pick up armv7_a4x_bs_tag */
#include <arch/arm/armv7/armv7var.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/armv7/armv7_machdep.h>
#include <armv7/omap/sitara_cm.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>

#define com_isr 8
#define ISR_RECV	(ISR_RXPL | ISR_XMODE | ISR_RCVEIR)

int	omapuart_match(struct device *, void *, void *);
void	omapuart_attach(struct device *, struct device *, void *);
int	omapuart_activate(struct device *, int);

extern int comcnspeed;
extern int comcnmode;

struct cfattach com_omap_ca = {
	sizeof (struct com_softc), omapuart_match, omapuart_attach, NULL,
	omapuart_activate
};

void
omapuart_init_cons(void)
{
	struct fdt_reg reg;
	void *node;

	if ((node = fdt_find_cons("ti,omap3-uart")) == NULL)
		if ((node = fdt_find_cons("ti,omap4-uart")) == NULL)
			return;
	if (fdt_get_reg(node, 0, &reg))
		return;

	comcnattach(&armv7_a4x_bs_tag, reg.addr, comcnspeed, 48000000,
	    comcnmode);
	comdefaultrate = comcnspeed;
}

int
omapuart_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "ti,omap3-uart") ||
	    OF_is_compatible(faa->fa_node, "ti,omap4-uart"));
}

void
omapuart_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_softc *sc = (struct com_softc *)self;
	struct fdt_attach_args *faa = aux;

	if (faa->fa_nreg < 1)
		return;

	sc->sc_iot = &armv7_a4x_bs_tag; /* XXX: This sucks */
	sc->sc_iobase = faa->fa_reg[0].addr;
	sc->sc_frequency = 48000000;
	sc->sc_uarttype = COM_UART_TI16750;

	if (bus_space_map(sc->sc_iot, sc->sc_iobase,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf("%s: bus_space_map failed\n", __func__);
		return;
	}

	sitara_cm_pinctrlbyname(faa->fa_node, "default");

	com_attach_subr(sc);

	(void)arm_intr_establish_fdt(faa->fa_node, IPL_TTY, comintr,
	    sc, sc->sc_dev.dv_xname);
}

int
omapuart_activate(struct device *self, int act)
{
	struct com_softc *sc = (struct com_softc *)self;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp = sc->sc_tty;

	switch (act) {
	case DVACT_SUSPEND:
		break;
	case DVACT_RESUME:
		if (sc->enabled) {
			sc->sc_initialize = 1;
			comparam(tp, &tp->t_termios);
			bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);

			if (ISSET(sc->sc_hwflags, COM_HW_SIR)) {
				bus_space_write_1(iot, ioh, com_isr,
				    ISR_RECV);
			}
		}
		break;
	}
	return 0;
}
