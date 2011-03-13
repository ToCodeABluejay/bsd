/*	$OpenBSD: libsa.h,v 1.2 2011/03/13 00:13:53 deraadt Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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

#include <lib/libsa/stand.h>

#define DEFAULT_KERNEL_ADDRESS  0

#ifdef DEBUG
#define DPRINTF(x)      printf x;
#else
#define DPRINTF(x)
#endif

/*
 * com
 */
void com_probe(struct consdev *);
void com_init(struct consdev *);
int com_getc(dev_t);
void com_putc(dev_t, int);

/*
 * wd
 */
int wdstrategy(void *, int, daddr32_t, size_t, void *, size_t *);
int wdopen(struct open_file *, ...);
int wdclose(struct open_file *);
