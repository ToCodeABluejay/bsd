/*	$OpenBSD: kern_fork.c,v 1.200 2017/09/27 06:45:00 deraadt Exp $	*/
/*	$NetBSD: kern_fork.c,v 1.29 1996/02/09 18:59:34 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_fork.c	8.6 (Berkeley) 4/8/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/file.h>
#include <sys/acct.h>
#include <sys/ktrace.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/pool.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/atomic.h>
#include <sys/pledge.h>
#include <sys/unistd.h>

#include <sys/syscallargs.h>

#include <uvm/uvm.h>
#include <machine/tcb.h>

int	nprocesses = 1;		/* process 0 */
int	nthreads = 1;		/* proc 0 */
int	randompid;		/* when set to 1, pid's go random */
struct	forkstat forkstat;

void fork_return(void *);
pid_t alloctid(void);
pid_t allocpid(void);
int ispidtaken(pid_t);

struct proc *thread_new(struct proc *_parent, vaddr_t _uaddr);
struct process *process_new(struct proc *, struct process *, int);
int fork_check_maxthread(uid_t _uid);

void
fork_return(void *arg)
{
	struct proc *p = (struct proc *)arg;

	if (p->p_p->ps_flags & PS_TRACED)
		psignal(p, SIGTRAP);

	child_return(p);
}

int
sys_fork(struct proc *p, void *v, register_t *retval)
{
	int flags;

	flags = FORK_FORK;
	if (p->p_p->ps_ptmask & PTRACE_FORK)
		flags |= FORK_PTRACE;
	return fork1(p, flags, fork_return, NULL, retval, NULL);
}

int
sys_vfork(struct proc *p, void *v, register_t *retval)
{
	return fork1(p, FORK_VFORK|FORK_PPWAIT, child_return, NULL,
	    retval, NULL);
}

int
sys___tfork(struct proc *p, void *v, register_t *retval)
{
	struct sys___tfork_args /* {
		syscallarg(const struct __tfork) *param;
		syscallarg(size_t) psize;
	} */ *uap = v;
	size_t psize = SCARG(uap, psize);
	struct __tfork param = { 0 };
	int error;

	if (psize == 0 || psize > sizeof(param))
		return EINVAL;
	if ((error = copyin(SCARG(uap, param), &param, psize)))
		return error;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_STRUCT))
		ktrstruct(p, "tfork", &param, sizeof(param));
#endif
#ifdef TCB_INVALID
	if (TCB_INVALID(param.tf_tcb))
		return EINVAL;
#endif /* TCB_INVALID */

	return thread_fork(p, param.tf_stack, param.tf_tcb, param.tf_tid,
	    retval);
}

/*
 * Allocate and initialize a thread (proc) structure, given the parent thread.
 */
struct proc *
thread_new(struct proc *parent, vaddr_t uaddr)
{
	struct proc *p; 

	p = pool_get(&proc_pool, PR_WAITOK);
	p->p_stat = SIDL;			/* protect against others */
	p->p_flag = 0;

	/*
	 * Make a proc table entry for the new process.
	 * Start by zeroing the section of proc that is zero-initialized,
	 * then copy the section that is copied directly from the parent.
	 */
	memset(&p->p_startzero, 0,
	    (caddr_t)&p->p_endzero - (caddr_t)&p->p_startzero);
	memcpy(&p->p_startcopy, &parent->p_startcopy,
	    (caddr_t)&p->p_endcopy - (caddr_t)&p->p_startcopy);
	crhold(p->p_ucred);
	p->p_addr = (struct user *)uaddr;

	/*
	 * Initialize the timeouts.
	 */
	timeout_set(&p->p_sleep_to, endtsleep, p);

	/*
	 * set priority of child to be that of parent
	 * XXX should move p_estcpu into the region of struct proc which gets
	 * copied.
	 */
	scheduler_fork_hook(parent, p);

#ifdef WITNESS
	p->p_sleeplocks = NULL;
#endif

	return p;
}

/*
 * Initialize common bits of a process structure, given the initial thread.
 */
void
process_initialize(struct process *pr, struct proc *p)
{
	/* initialize the thread links */
	pr->ps_mainproc = p;
	TAILQ_INIT(&pr->ps_threads);
	TAILQ_INSERT_TAIL(&pr->ps_threads, p, p_thr_link);
	pr->ps_refcnt = 1;
	p->p_p = pr;

	/* give the process the same creds as the initial thread */
	pr->ps_ucred = p->p_ucred;
	crhold(pr->ps_ucred);
	KASSERT(p->p_ucred->cr_ref >= 2);	/* new thread and new process */

	LIST_INIT(&pr->ps_children);

	timeout_set(&pr->ps_realit_to, realitexpire, pr);
}


/*
 * Allocate and initialize a new process.
 */
struct process *
process_new(struct proc *p, struct process *parent, int flags)
{
	struct process *pr;

	pr = pool_get(&process_pool, PR_WAITOK);

	/*
	 * Make a process structure for the new process.
	 * Start by zeroing the section of proc that is zero-initialized,
	 * then copy the section that is copied directly from the parent.
	 */
	memset(&pr->ps_startzero, 0,
	    (caddr_t)&pr->ps_endzero - (caddr_t)&pr->ps_startzero);
	memcpy(&pr->ps_startcopy, &parent->ps_startcopy,
	    (caddr_t)&pr->ps_endcopy - (caddr_t)&pr->ps_startcopy);

	process_initialize(pr, p);
	pr->ps_pid = allocpid();

	/* post-copy fixups */
	pr->ps_pptr = parent;
	pr->ps_limit->p_refcnt++;

	/* bump references to the text vnode (for sysctl) */
	pr->ps_textvp = parent->ps_textvp;
	if (pr->ps_textvp)
		vref(pr->ps_textvp);

	pr->ps_flags = parent->ps_flags &
	    (PS_SUGID | PS_SUGIDEXEC | PS_PLEDGE | PS_WXNEEDED);
	if (parent->ps_session->s_ttyvp != NULL)
		pr->ps_flags |= parent->ps_flags & PS_CONTROLT;

	/*
	 * Duplicate sub-structures as needed.
	 * Increase reference counts on shared objects.
	 */
	if (flags & FORK_SHAREFILES)
		pr->ps_fd = fdshare(parent);
	else
		pr->ps_fd = fdcopy(parent);
	if (flags & FORK_SIGHAND)
		pr->ps_sigacts = sigactsshare(parent);
	else
		pr->ps_sigacts = sigactsinit(parent);
	if (flags & FORK_SHAREVM)
		pr->ps_vmspace = uvmspace_share(parent);
	else
		pr->ps_vmspace = uvmspace_fork(parent);

	if (parent->ps_flags & PS_PROFIL)
		startprofclock(pr);
	if (flags & FORK_PTRACE)
		pr->ps_flags |= parent->ps_flags & PS_TRACED;
	if (flags & FORK_NOZOMBIE)
		pr->ps_flags |= PS_NOZOMBIE;
	if (flags & FORK_SYSTEM)
		pr->ps_flags |= PS_SYSTEM;

	/* mark as embryo to protect against others */
	pr->ps_flags |= PS_EMBRYO;

	/* Force visibility of all of the above changes */
	membar_producer();

	/* it's sufficiently inited to be globally visible */
	LIST_INSERT_HEAD(&allprocess, pr, ps_list);

	return pr;
}

/* print the 'table full' message once per 10 seconds */
struct timeval fork_tfmrate = { 10, 0 };

int
fork_check_maxthread(uid_t uid)
{
	/*
	 * Although process entries are dynamically created, we still keep
	 * a global limit on the maximum number we will create. We reserve
	 * the last 5 processes to root. The variable nprocesses is the
	 * current number of processes, maxprocess is the limit.  Similar
	 * rules for threads (struct proc): we reserve the last 5 to root;
	 * the variable nthreads is the current number of procs, maxthread is
	 * the limit.
	 */
	if ((nthreads >= maxthread - 5 && uid != 0) || nthreads >= maxthread) {
		static struct timeval lasttfm;

		if (ratecheck(&lasttfm, &fork_tfmrate))
			tablefull("proc");
		return EAGAIN;
	}
	nthreads++;

	return 0;
}

static inline void
fork_thread_start(struct proc *p, struct proc *parent, int flags)
{
	int s;

	SCHED_LOCK(s);
	p->p_stat = SRUN;
	p->p_cpu = sched_choosecpu_fork(parent, flags);
	setrunqueue(p);
	SCHED_UNLOCK(s);
}

int
fork1(struct proc *curp, int flags, void (*func)(void *), void *arg,
    register_t *retval, struct proc **rnewprocp)
{
	struct process *curpr = curp->p_p;
	struct process *pr;
	struct proc *p;
	uid_t uid = curp->p_ucred->cr_ruid;
	struct vmspace *vm;
	int count;
	vaddr_t uaddr;
	int error;
	struct  ptrace_state *newptstat = NULL;

	KASSERT((flags & ~(FORK_FORK | FORK_VFORK | FORK_PPWAIT | FORK_PTRACE
	    | FORK_IDLE | FORK_SHAREVM | FORK_SHAREFILES | FORK_NOZOMBIE
	    | FORK_SYSTEM | FORK_SIGHAND)) == 0);
	KASSERT((flags & FORK_SIGHAND) == 0 || (flags & FORK_SHAREVM));
	KASSERT(func != NULL);

	if ((error = fork_check_maxthread(uid)))
		return error;

	if ((nprocesses >= maxprocess - 5 && uid != 0) ||
	    nprocesses >= maxprocess) {
		static struct timeval lasttfm;

		if (ratecheck(&lasttfm, &fork_tfmrate))
			tablefull("process");
		nthreads--;
		return EAGAIN;
	}
	nprocesses++;

	/*
	 * Increment the count of processes running with this uid.
	 * Don't allow a nonprivileged user to exceed their current limit.
	 */
	count = chgproccnt(uid, 1);
	if (uid != 0 && count > curp->p_rlimit[RLIMIT_NPROC].rlim_cur) {
		(void)chgproccnt(uid, -1);
		nprocesses--;
		nthreads--;
		return EAGAIN;
	}

	uaddr = uvm_uarea_alloc();
	if (uaddr == 0) {
		(void)chgproccnt(uid, -1);
		nprocesses--;
		nthreads--;
		return (ENOMEM);
	}

	/*
	 * From now on, we're committed to the fork and cannot fail.
	 */
	p = thread_new(curp, uaddr);
	pr = process_new(p, curpr, flags);

	p->p_fd		= pr->ps_fd;
	p->p_vmspace	= pr->ps_vmspace;
	if (pr->ps_flags & PS_SYSTEM)
		atomic_setbits_int(&p->p_flag, P_SYSTEM);

	if (flags & FORK_PPWAIT) {
		atomic_setbits_int(&pr->ps_flags, PS_PPWAIT);
		atomic_setbits_int(&curpr->ps_flags, PS_ISPWAIT);
	}

#ifdef KTRACE
	/*
	 * Copy traceflag and tracefile if enabled.
	 * If not inherited, these were zeroed above.
	 */
	if (curpr->ps_traceflag & KTRFAC_INHERIT)
		ktrsettrace(pr, curpr->ps_traceflag, curpr->ps_tracevp,
		    curpr->ps_tracecred);
#endif

	/*
	 * Finish creating the child thread.  cpu_fork() will copy
	 * and update the pcb and make the child ready to run.  If
	 * this is a normal user fork, the child will exit directly
	 * to user mode via child_return() on its first time slice
	 * and will not return here.  If this is a kernel thread,
	 * the specified entry point will be executed.
	 */
	cpu_fork(curp, p, NULL, NULL, func, arg ? arg : p);

	vm = pr->ps_vmspace;

	if (flags & FORK_FORK) {
		forkstat.cntfork++;
		forkstat.sizfork += vm->vm_dsize + vm->vm_ssize;
	} else if (flags & FORK_VFORK) {
		forkstat.cntvfork++;
		forkstat.sizvfork += vm->vm_dsize + vm->vm_ssize;
	} else {
		forkstat.cntkthread++;
	}

	if (pr->ps_flags & PS_TRACED && flags & FORK_FORK)
		newptstat = malloc(sizeof(*newptstat), M_SUBPROC, M_WAITOK);

	p->p_tid = alloctid();

	LIST_INSERT_HEAD(&allproc, p, p_list);
	LIST_INSERT_HEAD(TIDHASH(p->p_tid), p, p_hash);
	LIST_INSERT_HEAD(PIDHASH(pr->ps_pid), pr, ps_hash);
	LIST_INSERT_AFTER(curpr, pr, ps_pglist);
	LIST_INSERT_HEAD(&curpr->ps_children, pr, ps_sibling);

	if (pr->ps_flags & PS_TRACED) {
		pr->ps_oppid = curpr->ps_pid;
		if (pr->ps_pptr != curpr->ps_pptr)
			proc_reparent(pr, curpr->ps_pptr);

		/*
		 * Set ptrace status.
		 */
		if (newptstat != NULL) {
			pr->ps_ptstat = newptstat;
			newptstat = NULL;
			curpr->ps_ptstat->pe_report_event = PTRACE_FORK;
			pr->ps_ptstat->pe_report_event = PTRACE_FORK;
			curpr->ps_ptstat->pe_other_pid = pr->ps_pid;
			pr->ps_ptstat->pe_other_pid = curpr->ps_pid;
		}
	}

	/*
	 * For new processes, set accounting bits and mark as complete.
	 */
	getnanotime(&pr->ps_start);
	pr->ps_acflag = AFORK;
	atomic_clearbits_int(&pr->ps_flags, PS_EMBRYO);

	if ((flags & FORK_IDLE) == 0)
		fork_thread_start(p, curp, flags);
	else
		p->p_cpu = arg;

	free(newptstat, M_SUBPROC, sizeof(*newptstat));

	/*
	 * Notify any interested parties about the new process.
	 */
	KNOTE(&curpr->ps_klist, NOTE_FORK | pr->ps_pid);

	/*
	 * Update stats now that we know the fork was successful.
	 */
	uvmexp.forks++;
	if (flags & FORK_PPWAIT)
		uvmexp.forks_ppwait++;
	if (flags & FORK_SHAREVM)
		uvmexp.forks_sharevm++;

	/*
	 * Pass a pointer to the new process to the caller.
	 */
	if (rnewprocp != NULL)
		*rnewprocp = p;

	/*
	 * Preserve synchronization semantics of vfork.  If waiting for
	 * child to exec or exit, set PS_PPWAIT on child and PS_ISPWAIT
	 * on ourselves, and sleep on our process for the latter flag
	 * to go away.
	 * XXX Need to stop other rthreads in the parent
	 */
	if (flags & FORK_PPWAIT)
		while (curpr->ps_flags & PS_ISPWAIT)
			tsleep(curpr, PWAIT, "ppwait", 0);

	/*
	 * If we're tracing the child, alert the parent too.
	 */
	if ((flags & FORK_PTRACE) && (curpr->ps_flags & PS_TRACED))
		psignal(curp, SIGTRAP);

	/*
	 * Return child pid to parent process
	 */
	if (retval != NULL) {
		retval[0] = pr->ps_pid;
		retval[1] = 0;
	}
	return (0);
}

int
thread_fork(struct proc *curp, void *stack, void *tcb, pid_t *tidptr,
    register_t *retval)
{
	struct process *pr = curp->p_p;
	struct proc *p;
	pid_t tid;
	vaddr_t uaddr;
	int error;

	if (stack == NULL)
		return EINVAL;

	if ((error = fork_check_maxthread(curp->p_ucred->cr_ruid)))
		return error;

	uaddr = uvm_uarea_alloc();
	if (uaddr == 0) {
		nthreads--;
		return ENOMEM;
	}

	/*
	 * From now on, we're committed to the fork and cannot fail.
	 */
	p = thread_new(curp, uaddr);
	atomic_setbits_int(&p->p_flag, P_THREAD);
	sigstkinit(&p->p_sigstk);

	/* other links */
	p->p_p = pr;
	pr->ps_refcnt++;

	/* local copies */
	p->p_fd		= pr->ps_fd;
	p->p_vmspace	= pr->ps_vmspace;

	/*
	 * Finish creating the child thread.  cpu_fork() will copy
	 * and update the pcb and make the child ready to run.  The
	 * child will exit directly to user mode via child_return()
	 * on its first time slice and will not return here.
	 */
	cpu_fork(curp, p, stack, tcb, child_return, p);

	p->p_tid = alloctid();

	LIST_INSERT_HEAD(&allproc, p, p_list);
	LIST_INSERT_HEAD(TIDHASH(p->p_tid), p, p_hash);
	TAILQ_INSERT_TAIL(&pr->ps_threads, p, p_thr_link);

	/*
	 * if somebody else wants to take us to single threaded mode,
	 * count ourselves in.
	 */
	if (pr->ps_single) {
		pr->ps_singlecount++;
		atomic_setbits_int(&p->p_flag, P_SUSPSINGLE);
	}

	/*
	 * Return tid to parent thread and copy it out to userspace
	 */
	retval[0] = tid = p->p_tid + THREAD_PID_OFFSET;
	retval[1] = 0;
	if (tidptr != NULL) {
		if (copyout(&tid, tidptr, sizeof(tid)))
			psignal(curp, SIGSEGV);
	}

	fork_thread_start(p, curp, 0);

	/*
	 * Update stats now that we know the fork was successful.
	 */
	forkstat.cnttfork++;
	uvmexp.forks++;
	uvmexp.forks_sharevm++;

	return 0;
}


/* Find an unused tid */
pid_t
alloctid(void)
{
	pid_t tid;

	do {
		/* (0 .. TID_MASK+1] */
		tid = 1 + (arc4random() & TID_MASK);
	} while (tfind(tid) != NULL);

	return (tid);
}

/*
 * Checks for current use of a pid, either as a pid or pgid.
 */
pid_t oldpids[128];
int
ispidtaken(pid_t pid)
{
	uint32_t i;

	for (i = 0; i < nitems(oldpids); i++)
		if (pid == oldpids[i])
			return (1);

	if (prfind(pid) != NULL)
		return (1);
	if (pgfind(pid) != NULL)
		return (1);
	if (zombiefind(pid) != NULL)
		return (1);
	return (0);
}

/* Find an unused pid */
pid_t
allocpid(void)
{
	static pid_t lastpid;
	pid_t pid;

	if (!randompid) {
		/* only used early on for system processes */
		pid = ++lastpid;
	} else {
		/* Find an unused pid satisfying lastpid < pid <= PID_MAX */
		do {
			pid = arc4random_uniform(PID_MAX - lastpid) + 1 +
			    lastpid;
		} while (ispidtaken(pid));
	}

	return pid;
}

void
freepid(pid_t pid)
{
	static uint32_t idx;

	oldpids[idx++ % nitems(oldpids)] = pid;
}

#if defined(MULTIPROCESSOR)
/*
 * XXX This is a slight hack to get newly-formed processes to
 * XXX acquire the kernel lock as soon as they run.
 */
void
proc_trampoline_mp(void)
{
	SCHED_ASSERT_LOCKED();
	__mp_unlock(&sched_lock);
	spl0();
	SCHED_ASSERT_UNLOCKED();
	KERNEL_ASSERT_UNLOCKED();

	KERNEL_LOCK();
}
#endif
