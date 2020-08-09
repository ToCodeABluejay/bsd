/*	$OpenBSD: kern_time.c,v 1.135 2020/08/09 19:15:47 cheloha Exp $	*/
/*	$NetBSD: kern_time.c,v 1.20 1996/02/18 11:57:06 fvdl Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)kern_time.c	8.4 (Berkeley) 5/26/95
 */

#include <sys/param.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/rwlock.h>
#include <sys/proc.h>
#include <sys/ktrace.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/stdint.h>
#include <sys/pledge.h>
#include <sys/task.h>
#include <sys/timeout.h>
#include <sys/timetc.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <dev/clock_subr.h>

/* 
 * Time of day and interval timer support.
 *
 * These routines provide the kernel entry points to get and set
 * the time-of-day and per-process interval timers.  Subroutines
 * here provide support for adding and subtracting timeval structures
 * and decrementing interval timers, optionally reloading the interval
 * timers when they expire.
 */

/* This function is used by clock_settime and settimeofday */
int
settime(const struct timespec *ts)
{
	struct timespec now;

	/*
	 * Don't allow the time to be set forward so far it will wrap
	 * and become negative, thus allowing an attacker to bypass
	 * the next check below.  The cutoff is 1 year before rollover
	 * occurs, so even if the attacker uses adjtime(2) to move
	 * the time past the cutoff, it will take a very long time
	 * to get to the wrap point.
	 *
	 * XXX: we check against UINT_MAX until we can figure out
	 *	how to deal with the hardware RTCs.
	 */
	if (ts->tv_sec > UINT_MAX - 365*24*60*60) {
		printf("denied attempt to set clock forward to %lld\n",
		    (long long)ts->tv_sec);
		return (EPERM);
	}
	/*
	 * If the system is secure, we do not allow the time to be
	 * set to an earlier value (it may be slowed using adjtime,
	 * but not set back). This feature prevent interlopers from
	 * setting arbitrary time stamps on files.
	 */
	nanotime(&now);
	if (securelevel > 1 && timespeccmp(ts, &now, <=)) {
		printf("denied attempt to set clock back %lld seconds\n",
		    (long long)now.tv_sec - ts->tv_sec);
		return (EPERM);
	}

	tc_setrealtimeclock(ts);
	KERNEL_LOCK();
	resettodr();
	KERNEL_UNLOCK();

	return (0);
}

int
clock_gettime(struct proc *p, clockid_t clock_id, struct timespec *tp)
{
	struct proc *q;
	int error = 0;

	switch (clock_id) {
	case CLOCK_REALTIME:
		nanotime(tp);
		break;
	case CLOCK_UPTIME:
		nanoruntime(tp);
		break;
	case CLOCK_MONOTONIC:
	case CLOCK_BOOTTIME:
		nanouptime(tp);
		break;
	case CLOCK_PROCESS_CPUTIME_ID:
		nanouptime(tp);
		timespecsub(tp, &curcpu()->ci_schedstate.spc_runtime, tp);
		timespecadd(tp, &p->p_p->ps_tu.tu_runtime, tp);
		timespecadd(tp, &p->p_rtime, tp);
		break;
	case CLOCK_THREAD_CPUTIME_ID:
		nanouptime(tp);
		timespecsub(tp, &curcpu()->ci_schedstate.spc_runtime, tp);
		timespecadd(tp, &p->p_tu.tu_runtime, tp);
		timespecadd(tp, &p->p_rtime, tp);
		break;
	default:
		/* check for clock from pthread_getcpuclockid() */
		if (__CLOCK_TYPE(clock_id) == CLOCK_THREAD_CPUTIME_ID) {
			KERNEL_LOCK();
			q = tfind(__CLOCK_PTID(clock_id) - THREAD_PID_OFFSET);
			if (q == NULL || q->p_p != p->p_p)
				error = ESRCH;
			else
				*tp = q->p_tu.tu_runtime;
			KERNEL_UNLOCK();
		} else
			error = EINVAL;
		break;
	}
	return (error);
}

int
sys_clock_gettime(struct proc *p, void *v, register_t *retval)
{
	struct sys_clock_gettime_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(struct timespec *) tp;
	} */ *uap = v;
	struct timespec ats;
	int error;

	memset(&ats, 0, sizeof(ats));
	if ((error = clock_gettime(p, SCARG(uap, clock_id), &ats)) != 0)
		return (error);

	error = copyout(&ats, SCARG(uap, tp), sizeof(ats));
#ifdef KTRACE
	if (error == 0 && KTRPOINT(p, KTR_STRUCT))
		ktrabstimespec(p, &ats);
#endif
	return (error);
}

int
sys_clock_settime(struct proc *p, void *v, register_t *retval)
{
	struct sys_clock_settime_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(const struct timespec *) tp;
	} */ *uap = v;
	struct timespec ats;
	clockid_t clock_id;
	int error;

	if ((error = suser(p)) != 0)
		return (error);

	if ((error = copyin(SCARG(uap, tp), &ats, sizeof(ats))) != 0)
		return (error);

	clock_id = SCARG(uap, clock_id);
	switch (clock_id) {
	case CLOCK_REALTIME:
		if (!timespecisvalid(&ats))
			return (EINVAL);
		if ((error = settime(&ats)) != 0)
			return (error);
		break;
	default:	/* Other clocks are read-only */
		return (EINVAL);
	}

	return (0);
}

int
sys_clock_getres(struct proc *p, void *v, register_t *retval)
{
	struct sys_clock_getres_args /* {
		syscallarg(clockid_t) clock_id;
		syscallarg(struct timespec *) tp;
	} */ *uap = v;
	clockid_t clock_id;
	struct bintime bt;
	struct timespec ts;
	struct proc *q;
	u_int64_t scale;
	int error = 0, realstathz;

	memset(&ts, 0, sizeof(ts));
	realstathz = (stathz == 0) ? hz : stathz;
	clock_id = SCARG(uap, clock_id);

	switch (clock_id) {
	case CLOCK_REALTIME:
	case CLOCK_MONOTONIC:
	case CLOCK_BOOTTIME:
	case CLOCK_UPTIME:
		memset(&bt, 0, sizeof(bt));
		rw_enter_read(&tc_lock);
		scale = ((1ULL << 63) / tc_getfrequency()) * 2;
		bt.frac = tc_getprecision() * scale;
		rw_exit_read(&tc_lock);
		BINTIME_TO_TIMESPEC(&bt, &ts);
		break;
	case CLOCK_PROCESS_CPUTIME_ID:
	case CLOCK_THREAD_CPUTIME_ID:
		ts.tv_nsec = 1000000000 / realstathz;
		break;
	default:
		/* check for clock from pthread_getcpuclockid() */
		if (__CLOCK_TYPE(clock_id) == CLOCK_THREAD_CPUTIME_ID) {
			KERNEL_LOCK();
			q = tfind(__CLOCK_PTID(clock_id) - THREAD_PID_OFFSET);
			if (q == NULL || q->p_p != p->p_p)
				error = ESRCH;
			else
				ts.tv_nsec = 1000000000 / realstathz;
			KERNEL_UNLOCK();
		} else
			error = EINVAL;
		break;
	}

	if (error == 0 && SCARG(uap, tp)) {
		ts.tv_nsec = MAX(ts.tv_nsec, 1);
		error = copyout(&ts, SCARG(uap, tp), sizeof(ts));
#ifdef KTRACE
		if (error == 0 && KTRPOINT(p, KTR_STRUCT))
			ktrreltimespec(p, &ts);
#endif
	}

	return error;
}

int
sys_nanosleep(struct proc *p, void *v, register_t *retval)
{
	static int chan;
	struct sys_nanosleep_args/* {
		syscallarg(const struct timespec *) rqtp;
		syscallarg(struct timespec *) rmtp;
	} */ *uap = v;
	struct timespec elapsed, remainder, request, start, stop;
	uint64_t nsecs;
	struct timespec *rmtp;
	int copyout_error, error;

	rmtp = SCARG(uap, rmtp);
	error = copyin(SCARG(uap, rqtp), &request, sizeof(request));
	if (error)
		return (error);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_STRUCT))
		ktrreltimespec(p, &request);
#endif

	if (request.tv_sec < 0 || !timespecisvalid(&request))
		return (EINVAL);

	do {
		getnanouptime(&start);
		nsecs = MAX(1, MIN(TIMESPEC_TO_NSEC(&request), MAXTSLP));
		error = tsleep_nsec(&chan, PWAIT | PCATCH, "nanosleep", nsecs);
		getnanouptime(&stop);
		timespecsub(&stop, &start, &elapsed);
		timespecsub(&request, &elapsed, &request);
		if (request.tv_sec < 0)
			timespecclear(&request);
		if (error != EWOULDBLOCK)
			break;
	} while (timespecisset(&request));

	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;

	if (rmtp) {
		memset(&remainder, 0, sizeof(remainder));
		remainder = request;
		copyout_error = copyout(&remainder, rmtp, sizeof(remainder));
		if (copyout_error)
			error = copyout_error;
#ifdef KTRACE
		if (copyout_error == 0 && KTRPOINT(p, KTR_STRUCT))
			ktrreltimespec(p, &remainder);
#endif
	}

	return error;
}

int
sys_gettimeofday(struct proc *p, void *v, register_t *retval)
{
	struct sys_gettimeofday_args /* {
		syscallarg(struct timeval *) tp;
		syscallarg(struct timezone *) tzp;
	} */ *uap = v;
	struct timeval atv;
	static const struct timezone zerotz = { 0, 0 };
	struct timeval *tp;
	struct timezone *tzp;
	int error = 0;

	tp = SCARG(uap, tp);
	tzp = SCARG(uap, tzp);

	if (tp) {
		memset(&atv, 0, sizeof(atv));
		microtime(&atv);
		if ((error = copyout(&atv, tp, sizeof (atv))))
			return (error);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrabstimeval(p, &atv);
#endif
	}
	if (tzp)
		error = copyout(&zerotz, tzp, sizeof(zerotz));
	return (error);
}

int
sys_settimeofday(struct proc *p, void *v, register_t *retval)
{
	struct sys_settimeofday_args /* {
		syscallarg(const struct timeval *) tv;
		syscallarg(const struct timezone *) tzp;
	} */ *uap = v;
	struct timezone atz;
	struct timeval atv;
	const struct timeval *tv;
	const struct timezone *tzp;
	int error;

	tv = SCARG(uap, tv);
	tzp = SCARG(uap, tzp);

	if ((error = suser(p)))
		return (error);
	/* Verify all parameters before changing time. */
	if (tv && (error = copyin(tv, &atv, sizeof(atv))))
		return (error);
	if (tzp && (error = copyin(tzp, &atz, sizeof(atz))))
		return (error);
	if (tv) {
		struct timespec ts;

		if (!timerisvalid(&atv))
			return (EINVAL);
		TIMEVAL_TO_TIMESPEC(&atv, &ts);
		if ((error = settime(&ts)) != 0)
			return (error);
	}

	return (0);
}

#define ADJFREQ_MAX (500000000LL << 32)
#define ADJFREQ_MIN (-500000000LL << 32)

int
sys_adjfreq(struct proc *p, void *v, register_t *retval)
{
	struct sys_adjfreq_args /* {
		syscallarg(const int64_t *) freq;
		syscallarg(int64_t *) oldfreq;
	} */ *uap = v;
	int error = 0;
	int64_t f, oldf;
	const int64_t *freq = SCARG(uap, freq);
	int64_t *oldfreq = SCARG(uap, oldfreq);

	if (freq) {
		if ((error = suser(p)))
			return (error);
		if ((error = copyin(freq, &f, sizeof(f))))
			return (error);
		if (f < ADJFREQ_MIN || f > ADJFREQ_MAX)
			return (EINVAL);
	}

	rw_enter(&tc_lock, (freq == NULL) ? RW_READ : RW_WRITE);
	if (oldfreq) {
		tc_adjfreq(&oldf, NULL);
		if ((error = copyout(&oldf, oldfreq, sizeof(oldf))))
			goto out;
	}
	if (freq)
		tc_adjfreq(NULL, &f);
out:
	rw_exit(&tc_lock);
	return (error);
}

int
sys_adjtime(struct proc *p, void *v, register_t *retval)
{
	struct sys_adjtime_args /* {
		syscallarg(const struct timeval *) delta;
		syscallarg(struct timeval *) olddelta;
	} */ *uap = v;
	struct timeval atv;
	const struct timeval *delta = SCARG(uap, delta);
	struct timeval *olddelta = SCARG(uap, olddelta);
	int64_t adjustment, remaining;	
	int error;

	error = pledge_adjtime(p, delta);
	if (error)
		return error;

	if (delta) {
		if ((error = suser(p)))
			return (error);
		if ((error = copyin(delta, &atv, sizeof(struct timeval))))
			return (error);
		if (!timerisvalid(&atv))
			return (EINVAL);

		if (atv.tv_sec > INT64_MAX / 1000000)
			return EINVAL;
		if (atv.tv_sec < INT64_MIN / 1000000)
			return EINVAL;
		adjustment = atv.tv_sec * 1000000;
		if (adjustment > INT64_MAX - atv.tv_usec)
			return EINVAL;
		adjustment += atv.tv_usec;

		rw_enter_write(&tc_lock);
	}

	if (olddelta) {
		tc_adjtime(&remaining, NULL);
		memset(&atv, 0, sizeof(atv));
		atv.tv_sec =  remaining / 1000000;
		atv.tv_usec = remaining % 1000000;
		if (atv.tv_usec < 0) {
			atv.tv_usec += 1000000;
			atv.tv_sec--;
		}

		if ((error = copyout(&atv, olddelta, sizeof(struct timeval))))
			goto out;
	}

	if (delta)
		tc_adjtime(NULL, &adjustment);
out:
	if (delta)
		rw_exit_write(&tc_lock);
	return (error);
}


struct mutex itimer_mtx = MUTEX_INITIALIZER(IPL_CLOCK);

/*
 * Get value of an interval timer.  The process virtual and
 * profiling virtual time timers are kept internally in the
 * way they are specified externally: in time until they expire.
 *
 * The real time interval timer's it_value, in contrast, is kept as an 
 * absolute time rather than as a delta, so that it is easy to keep
 * periodic real-time signals from drifting.
 *
 * Virtual time timers are processed in the hardclock() routine of
 * kern_clock.c.  The real time timer is processed by a timeout
 * routine, called from the softclock() routine.  Since a callout
 * may be delayed in real time due to interrupt processing in the system,
 * it is possible for the real time timeout routine (realitexpire, given below),
 * to be delayed in real time past when it is supposed to occur.  It
 * does not suffice, therefore, to reload the real timer .it_value from the
 * real time timers .it_interval.  Rather, we compute the next time in
 * absolute time the timer should go off.
 */
int
sys_getitimer(struct proc *p, void *v, register_t *retval)
{
	struct sys_getitimer_args /* {
		syscallarg(int) which;
		syscallarg(struct itimerval *) itv;
	} */ *uap = v;
	struct itimerval aitv;
	struct itimerspec *itimer;
	int which;

	which = SCARG(uap, which);

	if (which < ITIMER_REAL || which > ITIMER_PROF)
		return (EINVAL);
	itimer = &p->p_p->ps_timer[which];
	memset(&aitv, 0, sizeof(aitv));
	mtx_enter(&itimer_mtx);
	TIMESPEC_TO_TIMEVAL(&aitv.it_interval, &itimer->it_interval);
	TIMESPEC_TO_TIMEVAL(&aitv.it_value, &itimer->it_value);
	mtx_leave(&itimer_mtx);

	if (which == ITIMER_REAL) {
		struct timeval now;

		getmicrouptime(&now);
		/*
		 * Convert from absolute to relative time in .it_value
		 * part of real time timer.  If time for real time timer
		 * has passed return 0, else return difference between
		 * current time and time for the timer to go off.
		 */
		if (timerisset(&aitv.it_value)) {
			if (timercmp(&aitv.it_value, &now, <))
				timerclear(&aitv.it_value);
			else
				timersub(&aitv.it_value, &now,
				    &aitv.it_value);
		}
	}

	return (copyout(&aitv, SCARG(uap, itv), sizeof (struct itimerval)));
}

int
sys_setitimer(struct proc *p, void *v, register_t *retval)
{
	struct sys_setitimer_args /* {
		syscallarg(int) which;
		syscallarg(const struct itimerval *) itv;
		syscallarg(struct itimerval *) oitv;
	} */ *uap = v;
	struct sys_getitimer_args getargs;
	struct itimerspec aits;
	struct itimerval aitv;
	const struct itimerval *itvp;
	struct itimerval *oitv;
	struct process *pr = p->p_p;
	int error;
	int timo;
	int which;

	which = SCARG(uap, which);
	oitv = SCARG(uap, oitv);

	if (which < ITIMER_REAL || which > ITIMER_PROF)
		return (EINVAL);
	itvp = SCARG(uap, itv);
	if (itvp && (error = copyin((void *)itvp, (void *)&aitv,
	    sizeof(struct itimerval))))
		return (error);
	if (oitv != NULL) {
		SCARG(&getargs, which) = which;
		SCARG(&getargs, itv) = oitv;
		if ((error = sys_getitimer(p, &getargs, retval)))
			return (error);
	}
	if (itvp == 0)
		return (0);
	if (itimerfix(&aitv.it_value) || itimerfix(&aitv.it_interval))
		return (EINVAL);
	TIMEVAL_TO_TIMESPEC(&aitv.it_value, &aits.it_value);
	TIMEVAL_TO_TIMESPEC(&aitv.it_interval, &aits.it_interval);
	if (which == ITIMER_REAL) {
		struct timespec cts;

		timeout_del(&pr->ps_realit_to);
		getnanouptime(&cts);
		if (timespecisset(&aits.it_value)) {
			timo = tstohz(&aits.it_value);
			timeout_add(&pr->ps_realit_to, timo);
			timespecadd(&aits.it_value, &cts, &aits.it_value);
		}
		pr->ps_timer[ITIMER_REAL] = aits;
	} else {
		mtx_enter(&itimer_mtx);
		pr->ps_timer[which] = aits;
		mtx_leave(&itimer_mtx);
	}

	return (0);
}

/*
 * Real interval timer expired:
 * send process whose timer expired an alarm signal.
 * If time is not set up to reload, then just return.
 * Else compute next time timer should go off which is > current time.
 * This is where delay in processing this timeout causes multiple
 * SIGALRM calls to be compressed into one.
 */
void
realitexpire(void *arg)
{
	struct process *pr = arg;
	struct itimerspec *tp = &pr->ps_timer[ITIMER_REAL];

	prsignal(pr, SIGALRM);
	if (!timespecisset(&tp->it_interval)) {
		timespecclear(&tp->it_value);
		return;
	}
	for (;;) {
		struct timespec cts, nts;
		int timo;

		timespecadd(&tp->it_value, &tp->it_interval, &tp->it_value);
		getnanouptime(&cts);
		if (timespeccmp(&tp->it_value, &cts, >)) {
			nts = tp->it_value;
			timespecsub(&nts, &cts, &nts);
			timo = tstohz(&nts) - 1;
			if (timo <= 0)
				timo = 1;
			if ((pr->ps_flags & PS_EXITING) == 0)
				timeout_add(&pr->ps_realit_to, timo);
			return;
		}
	}
}

/*
 * Check that a proposed value to load into the .it_value or
 * .it_interval part of an interval timer is acceptable.
 */
int
itimerfix(struct timeval *tv)
{

	if (tv->tv_sec < 0 || tv->tv_sec > 100000000 ||
	    tv->tv_usec < 0 || tv->tv_usec >= 1000000)
		return (EINVAL);

	if (tv->tv_sec == 0 && tv->tv_usec != 0 && tv->tv_usec < tick)
		tv->tv_usec = tick;

	return (0);
}

/*
 * Decrement an interval timer by the given number of nanoseconds.
 * If the timer expires and it is periodic then reload it.  When reloading
 * the timer we subtract any overrun from the next period so that the timer
 * does not drift.
 */
int
itimerdecr(struct itimerspec *itp, long nsec)
{
	struct timespec decrement;

	NSEC_TO_TIMESPEC(nsec, &decrement);

	mtx_enter(&itimer_mtx);

	/*
	 * Double-check that the timer is enabled.  A different thread
	 * in setitimer(2) may have disabled it while we were entering
	 * the mutex.
	 */
	if (!timespecisset(&itp->it_value)) {
		mtx_leave(&itimer_mtx);
		return (1);
	}

	/*
	 * The timer is enabled.  Update and reload it as needed.
	 */
	timespecsub(&itp->it_value, &decrement, &itp->it_value);
	if (itp->it_value.tv_sec >= 0 && timespecisset(&itp->it_value)) {
		mtx_leave(&itimer_mtx);
		return (1);
	}
	if (!timespecisset(&itp->it_interval)) {
		timespecclear(&itp->it_value);
		mtx_leave(&itimer_mtx);
		return (0);
	}
	while (itp->it_value.tv_sec < 0 || !timespecisset(&itp->it_value))
		timespecadd(&itp->it_value, &itp->it_interval, &itp->it_value);
	mtx_leave(&itimer_mtx);
	return (0);
}

/*
 * ratecheck(): simple time-based rate-limit checking.  see ratecheck(9)
 * for usage and rationale.
 */
int
ratecheck(struct timeval *lasttime, const struct timeval *mininterval)
{
	struct timeval tv, delta;
	int rv = 0;

	getmicrouptime(&tv);

	timersub(&tv, lasttime, &delta);

	/*
	 * check for 0,0 is so that the message will be seen at least once,
	 * even if interval is huge.
	 */
	if (timercmp(&delta, mininterval, >=) ||
	    (lasttime->tv_sec == 0 && lasttime->tv_usec == 0)) {
		*lasttime = tv;
		rv = 1;
	}

	return (rv);
}

/*
 * ppsratecheck(): packets (or events) per second limitation.
 */
int
ppsratecheck(struct timeval *lasttime, int *curpps, int maxpps)
{
	struct timeval tv, delta;
	int rv;

	microuptime(&tv);

	timersub(&tv, lasttime, &delta);

	/*
	 * check for 0,0 is so that the message will be seen at least once.
	 * if more than one second have passed since the last update of
	 * lasttime, reset the counter.
	 *
	 * we do increment *curpps even in *curpps < maxpps case, as some may
	 * try to use *curpps for stat purposes as well.
	 */
	if (maxpps == 0)
		rv = 0;
	else if ((lasttime->tv_sec == 0 && lasttime->tv_usec == 0) ||
	    delta.tv_sec >= 1) {
		*lasttime = tv;
		*curpps = 0;
		rv = 1;
	} else if (maxpps < 0)
		rv = 1;
	else if (*curpps < maxpps)
		rv = 1;
	else
		rv = 0;

#if 1 /*DIAGNOSTIC?*/
	/* be careful about wrap-around */
	if (*curpps + 1 > *curpps)
		*curpps = *curpps + 1;
#else
	/*
	 * assume that there's not too many calls to this function.
	 * not sure if the assumption holds, as it depends on *caller's*
	 * behavior, not the behavior of this function.
	 * IMHO it is wrong to make assumption on the caller's behavior,
	 * so the above #if is #if 1, not #ifdef DIAGNOSTIC.
	 */
	*curpps = *curpps + 1;
#endif

	return (rv);
}

todr_chip_handle_t todr_handle;
int inittodr_done;

#define MINYEAR		((OpenBSD / 100) - 1)	/* minimum plausible year */

/*
 * inittodr:
 *
 *      Initialize time from the time-of-day register.
 */
void
inittodr(time_t base)
{
	time_t deltat;
	struct timeval rtctime;
	struct timespec ts;
	int badbase;

	inittodr_done = 1;

	if (base < (MINYEAR - 1970) * SECYR) {
		printf("WARNING: preposterous time in file system\n");
		/* read the system clock anyway */
		base = (MINYEAR - 1970) * SECYR;
		badbase = 1;
	} else
		badbase = 0;

	rtctime.tv_sec = base;
	rtctime.tv_usec = 0;

	if (todr_handle == NULL ||
	    todr_gettime(todr_handle, &rtctime) != 0 ||
	    rtctime.tv_sec < (MINYEAR - 1970) * SECYR) {
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the TODR.
		 */
		rtctime.tv_sec = base;
		rtctime.tv_usec = 0;
		if (todr_handle != NULL && !badbase)
			printf("WARNING: bad clock chip time\n");
		ts.tv_sec = rtctime.tv_sec;
		ts.tv_nsec = rtctime.tv_usec * 1000;
		tc_setclock(&ts);
		goto bad;
	} else {
		ts.tv_sec = rtctime.tv_sec;
		ts.tv_nsec = rtctime.tv_usec * 1000;
		tc_setclock(&ts);
	}

	if (!badbase) {
		/*
		 * See if we gained/lost two or more days; if
		 * so, assume something is amiss.
		 */
		deltat = rtctime.tv_sec - base;
		if (deltat < 0)
			deltat = -deltat;
		if (deltat < 2 * SECDAY)
			return;         /* all is well */
#ifndef SMALL_KERNEL
		printf("WARNING: clock %s %lld days\n",
		    rtctime.tv_sec < base ? "lost" : "gained",
		    (long long)(deltat / SECDAY));
#endif
	}
 bad:
	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

/*
 * resettodr:
 *
 *      Reset the time-of-day register with the current time.
 */
void
resettodr(void)
{
	struct timeval rtctime;

	/*
	 * Skip writing the RTC if inittodr(9) never ran.  We don't
	 * want to overwrite a reasonable value with a nonsense value.
	 */
	if (!inittodr_done)
		return;

	microtime(&rtctime);

	if (todr_handle != NULL &&
	    todr_settime(todr_handle, &rtctime) != 0)
		printf("WARNING: can't update clock chip time\n");
}

void
todr_attach(struct todr_chip_handle *todr)
{
	todr_handle = todr;
}

#define RESETTODR_PERIOD	1800

void periodic_resettodr(void *);
void perform_resettodr(void *);

struct timeout resettodr_to = TIMEOUT_INITIALIZER(periodic_resettodr, NULL);
struct task resettodr_task = TASK_INITIALIZER(perform_resettodr, NULL);

void
periodic_resettodr(void *arg __unused)
{
	task_add(systq, &resettodr_task);
}

void
perform_resettodr(void *arg __unused)
{
	resettodr();
	timeout_add_sec(&resettodr_to, RESETTODR_PERIOD);
}

void
start_periodic_resettodr(void)
{
	timeout_add_sec(&resettodr_to, RESETTODR_PERIOD);
}

void
stop_periodic_resettodr(void)
{
	timeout_del(&resettodr_to);
	task_del(systq, &resettodr_task);
}
