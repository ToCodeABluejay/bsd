/*	$OpenBSD: intr.c,v 1.2 2004/11/26 21:21:28 miod Exp $	*/
/*	$NetBSD: intr.c,v 1.2 1998/08/25 04:03:56 scottr Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass, Gordon W. Ross, and Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Link and dispatch interrupts.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/vmmeter.h>
#include <sys/evcount.h>

#include <uvm/uvm_extern.h>

#include <net/netisr.h>

#include <machine/cpu.h>
#include <machine/intr.h>

#define	NISR	8
#define	ISRLOC	0x18

void	intr_init(void);
void	netintr(void);

#ifdef DEBUG
int	intr_debug = 0;
#endif

struct intrhand intrs[NISR];
extern	int intrcnt[];		/* from locore.s */

/*
 * Establish an autovectored interrupt handler.
 * Called by driver attach functions.
 *
 * XXX Warning!  DO NOT use Macintosh ROM traps from an interrupt handler
 * established by this routine, either directly or indirectly, without
 * properly saving and restoring all registers.  If not, chaos _will_
 * ensue!  (sar 19980806)
 */
void
intr_establish(int (*func)(void *), void *arg, int ipl, const char *name)
{
	struct intrhand *ih;

#ifdef DIAGNOSTIC
	if (ipl < 0 || ipl >= NISR)
		panic("intr_establish: bad ipl %d", ipl);
#endif

	ih = &intrs[ipl];

#ifdef DIAGNOSTIC
	if (ih->ih_fn != NULL)
		panic("intr_establish: attempt to share ipl %d", ipl);
#endif

	ih->ih_fn = func;
	ih->ih_arg = arg;
	ih->ih_ipl = ipl;
	evcount_attach(&ih->ih_count, name, (void *)&ih->ih_ipl, &evcount_intr);
}

/*
 * Disestablish an interrupt handler.
 */
void
intr_disestablish(int ipl)
{
	struct intrhand *ih;

#ifdef DIAGNOSTIC
	if (ipl < 0 || ipl >= NISR)
		panic("intr_disestablish: bad ipl %d", ipl);
#endif

	ih = &intrs[ipl];

#ifdef DIAGNOSTIC
	if (ih->ih_fn == NULL)
		panic("intr_disestablish: no vector on ipl %d", ipl);
#endif

	ih->ih_fn = NULL;
	evcount_detach(&ih->ih_count);
}

/*
 * This is the dispatcher called by the low-level
 * assembly language interrupt routine.
 *
 * XXX Note: see the warning in intr_establish()
 */
void
intr_dispatch(int evec)	/* format | vector offset */
{
	struct intrhand *ih;
	int ipl, vec;

	vec = (evec & 0x0fff) >> 2;
	ipl = vec - ISRLOC;
#ifdef DIAGNOSTIC
	if (ipl < 0 || ipl >= NISR)
		panic("intr_dispatch: bad vec 0x%x", vec);
#endif

	uvmexp.intrs++;
	ih = &intrs[ipl];
	if (ih->ih_fn != NULL) {
		if ((*ih->ih_fn)(ih->ih_arg) != 0)
			ih->ih_count.ec_count++;
	} else {
#if 0
		printf("spurious interrupt, ipl %d\n", ipl);
#endif
	}
}

int netisr;

void
netintr()
{
	int s, isr;

	for (;;) {
		s = splimp();
		isr = netisr;
		netisr = 0;
		splx(s);
		
		if (isr == 0)
			return;

#define DONETISR(bit, fn) do {		\
	if (isr & (1 << bit))		\
		(fn)();			\
} while (0)

#include <net/netisr_dispatch.h>

#undef  DONETISR
	}
}
