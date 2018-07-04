/* $OpenBSD: process_machdep.c,v 1.4 2018/07/04 17:52:29 drahn Exp $ */
/*
 * Copyright (c) 2014 Patrick Wildt <patrick@blueri.se>
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

/*
 * This file may seem a bit stylized, but that so that it's easier to port.
 * Functions to be implemented here are:
 *
 * process_read_regs(proc, regs)
 *	Get the current user-visible register set from the process
 *	and copy it into the regs structure (<machine/reg.h>).
 *	The process is stopped at the time read_regs is called.
 *
 * process_write_regs(proc, regs)
 *	Update the current register set from the passed in regs
 *	structure.  Take care to avoid clobbering special CPU
 *	registers or privileged bits in the PSL.
 *	The process is stopped at the time write_regs is called.
 *
 * process_sstep(proc, sstep)
 *	Arrange for the process to trap or not trap depending on sstep
 *	after executing a single instruction.
 *
 * process_set_pc(proc)
 *	Set the process's program counter.
 */

#include <sys/param.h>

#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/systm.h>
#include <sys/user.h>

#include <machine/pcb.h>
#include <machine/reg.h>

#include <arm64/armreg.h>

int
process_read_regs(struct proc *p, struct reg *regs)
{
	struct trapframe *tf = p->p_addr->u_pcb.pcb_tf;

	memcpy(&regs->r_reg[0], &tf->tf_x[0], sizeof(regs->r_reg));
	regs->r_lr = tf->tf_lr;
	regs->r_sp = tf->tf_sp;
	regs->r_pc = tf->tf_elr;
	regs->r_spsr = tf->tf_spsr;
	regs->r_tpidr = (uint64_t)p->p_addr->u_pcb.pcb_tcb;

	return(0);
}

int
process_read_fpregs(struct proc *p, struct fpreg *regs)
{
	if (p->p_addr->u_pcb.pcb_flags & PCB_FPU)
		memcpy(regs, &p->p_addr->u_pcb.pcb_fpstate, sizeof(*regs));
	else
		memset(regs, 0, sizeof(*regs));

	return(0);
}

#ifdef	PTRACE

int
process_write_regs(struct proc *p, struct reg *regs)
{
	struct trapframe *tf = p->p_addr->u_pcb.pcb_tf;

	memcpy(&tf->tf_x[0], &regs->r_reg[0], sizeof(tf->tf_x));
	tf->tf_lr = regs->r_lr;
	tf->tf_sp = regs->r_sp;
	tf->tf_elr = regs->r_pc;
	tf->tf_spsr = (0xff0f0000 & regs->r_spsr) | (tf->tf_spsr & 0x0000ffff);
	p->p_addr->u_pcb.pcb_tcb = (void *)regs->r_tpidr;
	return(0);
}

int
process_write_fpregs(struct proc *p,  struct fpreg *regs)
{
	p->p_addr->u_pcb.pcb_flags |= PCB_FPU;
	memcpy(&p->p_addr->u_pcb.pcb_fpstate, regs,
	    sizeof(p->p_addr->u_pcb.pcb_fpstate));
	return(0);
}

int
process_sstep(struct proc *p, int sstep)
{
	if (sstep) {
		p->p_addr->u_pcb.pcb_flags |= PCB_SINGLESTEP;
	} else {
		p->p_addr->u_pcb.pcb_flags &= ~(PCB_SINGLESTEP);
	}
	return 0;
}

int
process_set_pc(struct proc *p, caddr_t addr)
{
	struct trapframe *tf = p->p_addr->u_pcb.pcb_tf;
	tf->tf_elr = (uint64_t)addr;
	return (0);
}

#endif	/* PTRACE */
