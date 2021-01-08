/*	$OpenBSD: vmalloc.h,v 1.2 2021/01/08 23:02:09 kettenis Exp $	*/
/*
 * Copyright (c) 2013, 2014, 2015 Mark Kettenis
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

#ifndef _LINUX_VMALLOC_H
#define _LINUX_VMALLOC_H

#include <sys/types.h>
#include <sys/malloc.h>
#include <uvm/uvm_extern.h>
#include <linux/types.h>
#include <linux/overflow.h>

void	*vmap(struct vm_page **, unsigned int, unsigned long, pgprot_t);
void	vunmap(void *, size_t);

void	*vmalloc(unsigned long);
void	*vzalloc(unsigned long);
void	vfree(const void *);

#endif
