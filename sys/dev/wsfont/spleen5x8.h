/*	$OpenBSD: spleen5x8.h,v 1.4 2019/10/03 08:29:18 fcambus Exp $ */

/*
 * Copyright (c) 2018-2019 Frederic Cambus <fcambus@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static u_char spleen5x8_data[];

struct wsdisplay_font spleen5x8 = {
	"Spleen 5x8",			/* typeface name */
	0,				/* index */
	' ',				/* firstchar */
	128 - ' ',			/* numchars */
	WSDISPLAY_FONTENC_ISO,		/* encoding */
	5,				/* width */
	8,				/* height */
	1,				/* stride */
	WSDISPLAY_FONTORDER_L2R,	/* bit order */
	WSDISPLAY_FONTORDER_L2R,	/* byte order */
	NULL,				/* cookie */
	spleen5x8_data			/* data */
};

static u_char spleen5x8_data[] = {
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */

	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */

	0x50, 	/* .*.*.... */
	0x50, 	/* .*.*.... */
	0x50, 	/* .*.*.... */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x50, 	/* .*.*.... */
	0xf8, 	/* *****... */
	0x50, 	/* .*.*.... */
	0x50, 	/* .*.*.... */
	0xf8, 	/* *****... */
	0x50, 	/* .*.*.... */
	0x00, 	/* ........ */

	0x20, 	/* ..*..... */
	0x70, 	/* .***.... */
	0xa0, 	/* *.*..... */
	0x60, 	/* .**..... */
	0x30, 	/* ..**.... */
	0x30, 	/* ..**.... */
	0xe0, 	/* ***..... */
	0x20, 	/* ..*..... */

	0x10, 	/* ...*.... */
	0x90, 	/* *..*.... */
	0xa0, 	/* *.*..... */
	0x20, 	/* ..*..... */
	0x40, 	/* .*...... */
	0x50, 	/* .*.*.... */
	0x90, 	/* *..*.... */
	0x80, 	/* *....... */

	0x20, 	/* ..*..... */
	0x50, 	/* .*.*.... */
	0x50, 	/* .*.*.... */
	0x60, 	/* .**..... */
	0xa0, 	/* *.*..... */
	0x90, 	/* *..*.... */
	0x68, 	/* .**.*... */
	0x00, 	/* ........ */

	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */

	0x10, 	/* ...*.... */
	0x20, 	/* ..*..... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x20, 	/* ..*..... */
	0x10, 	/* ...*.... */

	0x40, 	/* .*...... */
	0x20, 	/* ..*..... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0x20, 	/* ..*..... */
	0x40, 	/* .*...... */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0xf0, 	/* ****.... */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0xf8, 	/* *****... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x40, 	/* .*...... */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0xf0, 	/* ****.... */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */

	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */

	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0xb0, 	/* *.**.... */
	0xd0, 	/* **.*.... */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x60, 	/* .**..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x10, 	/* ...*.... */
	0x60, 	/* .**..... */
	0x80, 	/* *....... */
	0xf0, 	/* ****.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x20, 	/* ..*..... */
	0x10, 	/* ...*.... */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x80, 	/* *....... */
	0xa0, 	/* *.*..... */
	0xa0, 	/* *.*..... */
	0xf0, 	/* ****.... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0xf0, 	/* ****.... */
	0x80, 	/* *....... */
	0xf0, 	/* ****.... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0xe0, 	/* ***..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x80, 	/* *....... */
	0xe0, 	/* ***..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0xf0, 	/* ****.... */
	0x90, 	/* *..*.... */
	0x10, 	/* ...*.... */
	0x20, 	/* ..*..... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x70, 	/* .***.... */
	0x10, 	/* ...*.... */
	0x60, 	/* .**..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x40, 	/* .*...... */

	0x00, 	/* ........ */
	0x10, 	/* ...*.... */
	0x20, 	/* ..*..... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x20, 	/* ..*..... */
	0x10, 	/* ...*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0xf0, 	/* ****.... */
	0x00, 	/* ........ */
	0xf0, 	/* ****.... */
	0x00, 	/* ........ */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x40, 	/* .*...... */
	0x20, 	/* ..*..... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0x20, 	/* ..*..... */
	0x40, 	/* .*...... */
	0x00, 	/* ........ */

	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x10, 	/* ...*.... */
	0x20, 	/* ..*..... */
	0x40, 	/* .*...... */
	0x00, 	/* ........ */
	0x40, 	/* .*...... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0xb0, 	/* *.**.... */
	0xb0, 	/* *.**.... */
	0x80, 	/* *....... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0xf0, 	/* ****.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0xe0, 	/* ***..... */
	0x90, 	/* *..*.... */
	0xe0, 	/* ***..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0xe0, 	/* ***..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0xe0, 	/* ***..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0xe0, 	/* ***..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x80, 	/* *....... */
	0xe0, 	/* ***..... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0xe0, 	/* ***..... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x80, 	/* *....... */
	0xb0, 	/* *.**.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0xf0, 	/* ****.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0xc0, 	/* **...... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0xe0, 	/* ***..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0xf0, 	/* ****.... */
	0xf0, 	/* ****.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0xd0, 	/* **.*.... */
	0xd0, 	/* **.*.... */
	0xb0, 	/* *.**.... */
	0xb0, 	/* *.**.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0xe0, 	/* ***..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0xe0, 	/* ***..... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x30, 	/* ..**.... */

	0x00, 	/* ........ */
	0xe0, 	/* ***..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0xe0, 	/* ***..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x80, 	/* *....... */
	0x60, 	/* .**..... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0xe0, 	/* ***..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0xf8, 	/* *****... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x60, 	/* .**..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0xf0, 	/* ****.... */
	0xf0, 	/* ****.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x70, 	/* .***.... */
	0x10, 	/* ...*.... */
	0xe0, 	/* ***..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0xf0, 	/* ****.... */
	0x10, 	/* ...*.... */
	0x20, 	/* ..*..... */
	0x40, 	/* .*...... */
	0x80, 	/* *....... */
	0xf0, 	/* ****.... */
	0x00, 	/* ........ */

	0x70, 	/* .***.... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x70, 	/* .***.... */

	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */

	0x70, 	/* .***.... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0x70, 	/* .***.... */

	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x50, 	/* .*.*.... */
	0x88, 	/* *...*... */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0xf0, 	/* ****.... */

	0x40, 	/* .*...... */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x10, 	/* ...*.... */
	0x70, 	/* .***.... */
	0x90, 	/* *..*.... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0xe0, 	/* ***..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0xe0, 	/* ***..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */
	0x70, 	/* .***.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x90, 	/* *..*.... */
	0xf0, 	/* ****.... */
	0x80, 	/* *....... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x30, 	/* ..**.... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0xe0, 	/* ***..... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x10, 	/* ...*.... */
	0xe0, 	/* ***..... */

	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0xe0, 	/* ***..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x30, 	/* ..**.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0xc0, 	/* **...... */

	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x90, 	/* *..*.... */
	0xa0, 	/* *.*..... */
	0xc0, 	/* **...... */
	0xa0, 	/* *.*..... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x30, 	/* ..**.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0xf0, 	/* ****.... */
	0xf0, 	/* ****.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0xe0, 	/* ***..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0xe0, 	/* ***..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0xe0, 	/* ***..... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x70, 	/* .***.... */
	0x10, 	/* ...*.... */
	0x10, 	/* ...*.... */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x90, 	/* *..*.... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x80, 	/* *....... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x70, 	/* .***.... */
	0x80, 	/* *....... */
	0x60, 	/* .**..... */
	0x10, 	/* ...*.... */
	0xe0, 	/* ***..... */
	0x00, 	/* ........ */

	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0xe0, 	/* ***..... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x30, 	/* ..**.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x70, 	/* .***.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x60, 	/* .**..... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0xf0, 	/* ****.... */
	0xf0, 	/* ****.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x60, 	/* .**..... */
	0x60, 	/* .**..... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x90, 	/* *..*.... */
	0x70, 	/* .***.... */
	0x10, 	/* ...*.... */
	0xe0, 	/* ***..... */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0xf0, 	/* ****.... */
	0x10, 	/* ...*.... */
	0x20, 	/* ..*..... */
	0x40, 	/* .*...... */
	0xf0, 	/* ****.... */
	0x00, 	/* ........ */

	0x30, 	/* ..**.... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0xc0, 	/* **...... */
	0xc0, 	/* **...... */
	0x40, 	/* .*...... */
	0x40, 	/* .*...... */
	0x30, 	/* ..**.... */

	0x00, 	/* ........ */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x00, 	/* ........ */

	0xc0, 	/* **...... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0x30, 	/* ..**.... */
	0x30, 	/* ..**.... */
	0x20, 	/* ..*..... */
	0x20, 	/* ..*..... */
	0xc0, 	/* **...... */

	0x00, 	/* ........ */
	0x48, 	/* .*..*... */
	0xb0, 	/* *.**.... */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */

	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
	0x00, 	/* ........ */
};
