/*	$OpenBSD: subr_log.c,v 1.34 2015/12/05 10:11:53 tedu Exp $	*/
/*	$NetBSD: subr_log.c,v 1.11 1996/03/30 22:24:44 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)subr_log.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Error log buffer for kernel printf's.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/msgbuf.h>
#include <sys/file.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/syslog.h>
#include <sys/poll.h>
#include <sys/malloc.h>
#include <sys/filedesc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <sys/mount.h>
#include <sys/syscallargs.h>

#define LOG_RDPRI	(PZERO + 1)

#define LOG_ASYNC	0x04
#define LOG_RDWAIT	0x08

struct logsoftc {
	int	sc_state;		/* see above for possibilities */
	struct	selinfo sc_selp;	/* process waiting on select call */
	int	sc_pgid;		/* process/group for async I/O */
	uid_t	sc_siguid;		/* uid for process that set sc_pgid */
	uid_t	sc_sigeuid;		/* euid for process that set sc_pgid */
} logsoftc;

int	log_open;			/* also used in log() */
int	msgbufmapped;			/* is the message buffer mapped */
struct	msgbuf *msgbufp;		/* the mapped buffer, itself. */
struct	msgbuf *consbufp;		/* console message buffer. */
struct file *syslogf;

void filt_logrdetach(struct knote *kn);
int filt_logread(struct knote *kn, long hint);
   
struct filterops logread_filtops =
	{ 1, NULL, filt_logrdetach, filt_logread};

int dosendsyslog(struct proc *, const char *, size_t, int, enum uio_seg);

void
initmsgbuf(caddr_t buf, size_t bufsize)
{
	struct msgbuf *mbp;
	long new_bufs;

	/* Sanity-check the given size. */
	if (bufsize < sizeof(struct msgbuf))
		return;

	mbp = msgbufp = (struct msgbuf *)buf;

	new_bufs = bufsize - offsetof(struct msgbuf, msg_bufc);
	if ((mbp->msg_magic != MSG_MAGIC) || (mbp->msg_bufs != new_bufs) ||
	    (mbp->msg_bufr < 0) || (mbp->msg_bufr >= mbp->msg_bufs) ||
	    (mbp->msg_bufx < 0) || (mbp->msg_bufx >= mbp->msg_bufs)) {
		/*
		 * If the buffer magic number is wrong, has changed
		 * size (which shouldn't happen often), or is
		 * internally inconsistent, initialize it.
		 */

		memset(buf, 0, bufsize);
		mbp->msg_magic = MSG_MAGIC;
		mbp->msg_bufs = new_bufs;
	}
	
	/* Always start new buffer data on a new line. */
	if (mbp->msg_bufx > 0 && mbp->msg_bufc[mbp->msg_bufx - 1] != '\n')
		msgbuf_putchar(msgbufp, '\n');

	/* mark it as ready for use. */
	msgbufmapped = 1;
}

void
initconsbuf(void)
{
	long new_bufs;

	/* Set up a buffer to collect /dev/console output */
	consbufp = malloc(CONSBUFSIZE, M_TEMP, M_NOWAIT|M_ZERO);
	if (consbufp) {
		new_bufs = CONSBUFSIZE - offsetof(struct msgbuf, msg_bufc);
		consbufp->msg_magic = MSG_MAGIC;
		consbufp->msg_bufs = new_bufs;
	}
}

void
msgbuf_putchar(struct msgbuf *mbp, const char c) 
{
	if (mbp->msg_magic != MSG_MAGIC)
		/* Nothing we can do */
		return;

	mbp->msg_bufc[mbp->msg_bufx++] = c;
	mbp->msg_bufl = min(mbp->msg_bufl+1, mbp->msg_bufs);
	if (mbp->msg_bufx < 0 || mbp->msg_bufx >= mbp->msg_bufs)
		mbp->msg_bufx = 0;
	/* If the buffer is full, keep the most recent data. */
	if (mbp->msg_bufr == mbp->msg_bufx) {
		if (++mbp->msg_bufr >= mbp->msg_bufs)
			mbp->msg_bufr = 0;
	}
}

int
logopen(dev_t dev, int flags, int mode, struct proc *p)
{
	if (log_open)
		return (EBUSY);
	log_open = 1;
	return (0);
}

int
logclose(dev_t dev, int flag, int mode, struct proc *p)
{

	if (syslogf)
		FRELE(syslogf, p);
	syslogf = NULL;
	log_open = 0;
	logsoftc.sc_state = 0;
	return (0);
}

int
logread(dev_t dev, struct uio *uio, int flag)
{
	struct msgbuf *mbp = msgbufp;
	long l;
	int s;
	int error = 0;

	s = splhigh();
	while (mbp->msg_bufr == mbp->msg_bufx) {
		if (flag & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		logsoftc.sc_state |= LOG_RDWAIT;
		error = tsleep(mbp, LOG_RDPRI | PCATCH,
			       "klog", 0);
		if (error) {
			splx(s);
			return (error);
		}
	}
	splx(s);
	logsoftc.sc_state &= ~LOG_RDWAIT;

	while (uio->uio_resid > 0) {
		l = mbp->msg_bufx - mbp->msg_bufr;
		if (l < 0)
			l = mbp->msg_bufs - mbp->msg_bufr;
		l = min(l, uio->uio_resid);
		if (l == 0)
			break;
		error = uiomovei(&mbp->msg_bufc[mbp->msg_bufr], (int)l, uio);
		if (error)
			break;
		mbp->msg_bufr += l;
		if (mbp->msg_bufr < 0 || mbp->msg_bufr >= mbp->msg_bufs)
			mbp->msg_bufr = 0;
	}
	return (error);
}

int
logpoll(dev_t dev, int events, struct proc *p)
{
	int revents = 0;
	int s = splhigh();

	if (events & (POLLIN | POLLRDNORM)) {
		if (msgbufp->msg_bufr != msgbufp->msg_bufx)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &logsoftc.sc_selp);
	}
	splx(s);
	return (revents);
}

int
logkqfilter(dev_t dev, struct knote *kn)
{
	struct klist *klist;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &logsoftc.sc_selp.si_note;
		kn->kn_fop = &logread_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = (void *)msgbufp;

	s = splhigh();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return (0);
}

void
filt_logrdetach(struct knote *kn)
{
	int s = splhigh();

	SLIST_REMOVE(&logsoftc.sc_selp.si_note, kn, knote, kn_selnext);
	splx(s);
}

int
filt_logread(struct knote *kn, long hint)
{
	struct  msgbuf *p = (struct  msgbuf *)kn->kn_hook;

	kn->kn_data = (int)(p->msg_bufx - p->msg_bufr);

	return (p->msg_bufx != p->msg_bufr);
}

void
logwakeup(void)
{
	if (!log_open)
		return;
	selwakeup(&logsoftc.sc_selp);
	if (logsoftc.sc_state & LOG_ASYNC)
		csignal(logsoftc.sc_pgid, SIGIO,
		    logsoftc.sc_siguid, logsoftc.sc_sigeuid);
	if (logsoftc.sc_state & LOG_RDWAIT) {
		wakeup(msgbufp);
		logsoftc.sc_state &= ~LOG_RDWAIT;
	}
}

int
logioctl(dev_t dev, u_long com, caddr_t data, int flag, struct proc *p)
{
	struct file *fp;
	long l;
	int error, s;

	switch (com) {

	/* return number of characters immediately available */
	case FIONREAD:
		s = splhigh();
		l = msgbufp->msg_bufx - msgbufp->msg_bufr;
		splx(s);
		if (l < 0)
			l += msgbufp->msg_bufs;
		*(int *)data = l;
		break;

	case FIONBIO:
		break;

	case FIOASYNC:
		if (*(int *)data)
			logsoftc.sc_state |= LOG_ASYNC;
		else
			logsoftc.sc_state &= ~LOG_ASYNC;
		break;

	case TIOCSPGRP:
		logsoftc.sc_pgid = *(int *)data;
		logsoftc.sc_siguid = p->p_ucred->cr_ruid;
		logsoftc.sc_sigeuid = p->p_ucred->cr_uid;
		break;

	case TIOCGPGRP:
		*(int *)data = logsoftc.sc_pgid;
		break;

	case LIOCSFD:
		if ((error = suser(p, 0)) != 0)
			return (error);
		if ((error = getsock(p, *(int *)data, &fp)) != 0)
			return (error);
		if (syslogf)
			FRELE(syslogf, p);
		syslogf = fp;
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

int
sys_sendsyslog(struct proc *p, void *v, register_t *retval)
{
	struct sys_sendsyslog_args /* {
		syscallarg(const void *) buf;
		syscallarg(size_t) nbyte;
	} */ *uap = v;
	struct sys_sendsyslog2_args oap;

	SCARG(&oap, buf) = SCARG(uap, buf);
	SCARG(&oap, nbyte) = SCARG(uap, nbyte);
	SCARG(&oap, flags) = 0;
	return sys_sendsyslog2(p, &oap, retval);
}

int
sys_sendsyslog2(struct proc *p, void *v, register_t *retval)
{
	struct sys_sendsyslog2_args /* {
		syscallarg(const void *) buf;
		syscallarg(size_t) nbyte;
		syscallarg(int) flags;
	} */ *uap = v;
	int error;
#ifndef SMALL_KERNEL
	static int dropped_count, orig_error;
	int len;
	char buf[64];

	if (dropped_count) {
		len = snprintf(buf, sizeof(buf),
		    "<%d>sendsyslog: dropped %d message%s, error %d",
		    LOG_KERN|LOG_WARNING, dropped_count,
		    dropped_count == 1 ? "" : "s", orig_error);
		error = dosendsyslog(p, buf, MIN((size_t)len, sizeof(buf) - 1),
		    SCARG(uap, flags), UIO_SYSSPACE);
		if (error) {
			dropped_count++;
			return (error);
		}
		dropped_count = 0;
	}
#endif
	error = dosendsyslog(p, SCARG(uap, buf), SCARG(uap, nbyte),
	    SCARG(uap, flags), UIO_USERSPACE);
#ifndef SMALL_KERNEL
	if (error && error != ENOTCONN) {
		dropped_count++;
		orig_error = error;
	}
#endif
	return (error);
}

int
dosendsyslog(struct proc *p, const char *buf, size_t nbyte, int flags,
    enum uio_seg sflg)
{
#ifdef KTRACE
	struct iovec *ktriov = NULL;
	int iovlen;
#endif
	extern struct tty *constty;
	struct iovec aiov;
	struct uio auio;
	struct file *f;
	size_t len;
	int error;

	if (syslogf == NULL) {
		if (constty && (flags & LOG_CONS)) {
			int i;

			/* Skip syslog prefix */
			if (nbyte >= 4 && buf[0] == '<' &&
			    buf[3] == '>') {
				buf += 4;
				nbyte -= 4;
			}
			for (i = 0; i < nbyte; i++)
				tputchar(buf[i], constty);
			tputchar('\n', constty);
		}
		return (ENOTCONN);
	}
	f = syslogf;
	FREF(f);

	aiov.iov_base = (char *)buf;
	aiov.iov_len = nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_segflg = sflg;
	auio.uio_rw = UIO_WRITE;
	auio.uio_procp = p;
	auio.uio_offset = 0;
	auio.uio_resid = aiov.iov_len;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO)) {
		ktriov = mallocarray(auio.uio_iovcnt, sizeof(struct iovec),
		    M_TEMP, M_WAITOK);
		iovlen = auio.uio_iovcnt * sizeof (struct iovec);

		memcpy(ktriov, auio.uio_iov, iovlen);
	}
#endif

	len = auio.uio_resid;
	error = sosend(f->f_data, NULL, &auio, NULL, NULL, 0);
	if (error == 0)
		len -= auio.uio_resid;

#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p, -1, UIO_WRITE, ktriov, len);
		free(ktriov, M_TEMP, iovlen);
	}
#endif
	FRELE(f, p);
	return error;
}
