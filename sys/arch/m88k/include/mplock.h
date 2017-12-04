/*	$OpenBSD: mplock.h,v 1.3 2017/12/04 09:51:03 mpi Exp $	*/

/*
 * Copyright (c) 2004 Niklas Hallqvist.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _M88K_MPLOCK_H_
#define _M88K_MPLOCK_H_

#ifndef _LOCORE

/*
 * Really simple spinlock implementation with recursive capabilities.
 * Correctness is paramount, no fancyness allowed.
 */

struct __mp_lock {
	__cpu_simple_lock_t mpl_lock;
	volatile struct cpu_info *mpl_cpu;
	volatile int mpl_count;
};

static __inline void
__mp_lock_init(struct __mp_lock *mpl)
{
	__cpu_simple_lock_init(&mpl->mpl_lock);
	mpl->mpl_cpu = NULL;
	mpl->mpl_count = 0;
}

void	__mp_lock(struct __mp_lock *);
void	__mp_unlock(struct __mp_lock *);
int	__mp_release_all(struct __mp_lock *);
int	__mp_release_all_but_one(struct __mp_lock *);

static __inline__ void
__mp_acquire_count(struct __mp_lock *mpl, int count)
{
	while (count--)
		__mp_lock(mpl);
}

static __inline__ int
__mp_lock_held(struct __mp_lock *mpl, struct cpu_info *ci)
{
	return mpl->mpl_cpu == ci;
}

#endif

#endif /* !_MACHINE_MPLOCK_H */
