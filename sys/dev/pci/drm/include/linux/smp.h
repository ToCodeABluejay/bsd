/* Public domain. */

#ifndef _LINUX_SMP_H
#define _LINUX_SMP_H

/* sparc64 cpu.h needs time.h and siginfo.h (indirect via param.h) */
#include <sys/param.h>
#include <machine/cpu.h>

#define smp_processor_id()	(curcpu()->ci_cpuid)

#endif
