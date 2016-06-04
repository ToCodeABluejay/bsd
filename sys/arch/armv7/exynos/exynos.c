/* $OpenBSD: exynos.c,v 1.12 2016/06/04 18:09:16 jsg Exp $ */
/*
 * Copyright (c) 2005,2008 Dale Rahn <drahn@openbsd.com>
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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
#include <sys/systm.h>

#include <machine/bus.h>

#include <arm/mainbus/mainbus.h>
#include <armv7/armv7/armv7var.h>

int	exynos_match(struct device *, void *, void *);
void	exynos4_init();
void	exynos5_init();

struct cfattach exynos_ca = {
	sizeof(struct armv7_softc), exynos_match, armv7_attach, NULL,
	config_activate_children
};

struct cfdriver exynos_cd = {
	NULL, "exynos", DV_DULL
};

struct board_dev chromebook_devs[] = {
	{ "exmct",	0 },
	{ "exdog",	0 },
	{ "exclock",	0 },
	{ "expower",	0 },
	{ "exsysreg",	0 },
//	{ "exuart",	1 },
	{ "exgpio",	0 },
	{ "exgpio",	1 },
	{ "exgpio",	2 },
	{ "exgpio",	3 },
	{ "exgpio",	4 },
	{ "exgpio",	5 },
	{ "exehci",	0 },
	{ "exiic",	4 },
//	{ "exesdhc",	2 },
//	{ "exesdhc",	3 },
	{ "exdisplay",	0 },
	{ NULL,		0 }
};

/* Samsung Mobile NURI board */
struct board_dev nuri_devs[] = {
	{ "exmct",	0 },
	{ "exdog",	0 },
//	{ "exclock",	0 },
	{ "expower",	0 },
	{ "exsysreg",	0 },
//	{ "exuart",	0 },
	{ "exuart",	1 },
	{ "exuart",	2 },
	{ "exuart",	3 },
	{ "exgpio",	0 },
	{ "exgpio",	1 },
	{ "exgpio",	2 },
	{ "exgpio",	3 },
	{ "exgpio",	4 },
	{ "exgpio",	5 },
//	{ "exehci",	0 },
	{ "exiic",	0 },
//	{ "exesdhc",	2 },
	{ NULL,		0 }
};

/*
 * S5PC210/Exynos4210 reference board
 * has a LAN9215 (emulated in qemu as LAN9118)
 */
struct board_dev smdkc210_devs[] = {
	{ "exmct",	0 },
	{ "exdog",	0 },
//	{ "exclock",	0 },
	{ "expower",	0 },
	{ "exsysreg",	0 },
//	{ "exuart",	0 },
	{ "exuart",	1 },
	{ "exuart",	2 },
	{ "exuart",	3 },
	{ "exgpio",	0 },
	{ "exgpio",	1 },
	{ "exgpio",	2 },
	{ "exgpio",	3 },
	{ "exgpio",	4 },
	{ "exgpio",	5 },
//	{ "exehci",	0 },
	{ "exiic",	0 },
//	{ "exesdhc",	2 },
	{ NULL,		0 }
};

struct armv7_board exynos_boards[] = {
	{
		BOARD_ID_EXYNOS5_CHROMEBOOK,
		chromebook_devs,
		exynos5_init,
	},
	{
		BOARD_ID_EXYNOS4_NURI,
		nuri_devs,
		exynos4_init,
	},
	{
		BOARD_ID_EXYNOS4_SMDKC210,
		smdkc210_devs,
		exynos4_init,
	},
	{ 0, NULL, NULL },
};

struct board_dev *
exynos_board_devs(void)
{
	int i;

	for (i = 0; exynos_boards[i].board_id != 0; i++) {
		if (exynos_boards[i].board_id == board_id)
			return (exynos_boards[i].devs);
	}
	return (NULL);
}

void
exynos_board_init(void)
{
	int i;

	for (i = 0; exynos_boards[i].board_id != 0; i++) {
		if (exynos_boards[i].board_id == board_id) {
			exynos_boards[i].init();
			break;
		}
	}
}

int
exynos_match(struct device *parent, void *cfdata, void *aux)
{
	union mainbus_attach_args *ma = (union mainbus_attach_args *)aux;
	struct cfdata *cf = (struct cfdata *)cfdata;

	if (ma->ma_name == NULL)
		return (0);

	if (strcmp(cf->cf_driver->cd_name, ma->ma_name) != 0)
		return (0);

	return (exynos_board_devs() != NULL);
}
