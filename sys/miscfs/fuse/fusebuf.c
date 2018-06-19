/* $OpenBSD: fusebuf.c,v 1.15 2018/06/19 11:27:54 helg Exp $ */
/*
 * Copyright (c) 2012-2013 Sylvestre Gallon <ccna.syl@gmail.com>
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
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/fusebuf.h>

#include "fusefs_node.h"
#include "fusefs.h"

struct fusebuf *
fb_setup(size_t len, ino_t ino, int op, struct proc *p)
{
	struct fusebuf *fbuf;

	fbuf = pool_get(&fusefs_fbuf_pool, PR_WAITOK | PR_ZERO);
	fbuf->fb_len = len;
	fbuf->fb_err = 0;
	arc4random_buf(&fbuf->fb_uuid, sizeof fbuf->fb_uuid);
	fbuf->fb_type = op;
	fbuf->fb_ino = ino;
	/*
	 * When exposed to userspace, thread IDs have THREAD_PID_OFFSET added
	 * to keep them from overlapping the PID range.
	 */
	fbuf->fb_tid = p->p_tid + THREAD_PID_OFFSET;
	fbuf->fb_uid = p->p_ucred->cr_uid;
	fbuf->fb_gid = p->p_ucred->cr_gid;
	fbuf->fb_umask = p->p_p->ps_fd->fd_cmask;
	if (len == 0)
		fbuf->fb_dat = NULL;
	else
		fbuf->fb_dat = (uint8_t *)malloc(len, M_FUSEFS,
		    M_WAITOK | M_ZERO);

	return (fbuf);
}

int
fb_queue(dev_t dev, struct fusebuf *fbuf)
{
	int error = 0;

	fuse_device_queue_fbuf(dev, fbuf);

	if ((error = tsleep(fbuf, PWAIT, "fuse", TSLEEP_TIMEOUT * hz))) {
		fuse_device_cleanup(dev, fbuf);
		return (error);
	}

	return (fbuf->fb_err);
}

void
fb_delete(struct fusebuf *fbuf)
{
	if (fbuf != NULL) {
		free(fbuf->fb_dat, M_FUSEFS, fbuf->fb_len);
		pool_put(&fusefs_fbuf_pool, fbuf);
	}
}
