/*	$OpenBSD: yeeloong_machdep.c,v 1.1 2010/02/05 20:51:22 miod Exp $	*/

/*
 * Copyright (c) 2009, 2010 Miodrag Vallat.
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

/*
 * Lemote Yeeloong specific code and configuration data.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>

#include <dev/pci/pcireg.h>

#include <loongson/dev/bonitovar.h>
#include <loongson/dev/bonito_irq.h>

int	 yeeloong_intr_map(int, int, int);

const struct bonito_config yeeloong_bonito = {
	.bc_adbase = 11,

	.bc_gpioIE = LOONGSON_INTRMASK_GPIO,
	.bc_intEdge = LOONGSON_INTRMASK_PCI_SYSERR |
	    LOONGSON_INTRMASK_PCI_PARERR,
	.bc_intSteer = 0,
	.bc_intPol = LOONGSON_INTRMASK_DRAM_PARERR |
	    LOONGSON_INTRMASK_PCI_SYSERR | LOONGSON_INTRMASK_PCI_PARERR |
	    LOONGSON_INTRMASK_INT0 | LOONGSON_INTRMASK_INT1,

	.bc_intr_map = yeeloong_intr_map
};

int
yeeloong_intr_map(int dev, int fn, int pin)
{
	switch (dev) {
	/* onboard devices, only pin A is wired */
	case 6:
	case 7:
	case 8:
	case 9:
		if (pin == PCI_INTERRUPT_PIN_A)
			return BONITO_DIRECT_IRQ(LOONGSON_INTR_PCIA +
			    (dev - 6));
		break;
	/* PCI slot */
	case 10:
		return BONITO_DIRECT_IRQ(LOONGSON_INTR_PCIA +
		    (pin - PCI_INTERRUPT_PIN_A));
	/* Geode chip */
	case 14:
		switch (fn) {
		case 1:	/* Flash */
			return BONITO_ISA_IRQ(6);
		case 2:	/* AC97 */
			return BONITO_ISA_IRQ(9);
		case 4:	/* OHCI */
		case 5:	/* EHCI */
			return BONITO_ISA_IRQ(11);
		}
		break;
	default:
		break;
	}

	return -1;
}
