/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	NetBSD: syscalls.master,v 1.14 1995/12/26 10:06:15 jonathan Exp 
 */

#define	ULTRIX_SYS_syscall	0
#define	ULTRIX_SYS_exit	1
#define	ULTRIX_SYS_fork	2
#define	ULTRIX_SYS_read	3
#define	ULTRIX_SYS_write	4
#define	ULTRIX_SYS_open	5
#define	ULTRIX_SYS_close	6
#define	ULTRIX_SYS_owait	7
#define	ULTRIX_SYS_creat	8
#define	ULTRIX_SYS_link	9
#define	ULTRIX_SYS_unlink	10
#define	ULTRIX_SYS_execv	11
#define	ULTRIX_SYS_chdir	12
				/* 13 is obsolete time */
#define	ULTRIX_SYS_mknod	14
#define	ULTRIX_SYS_chmod	15
#define	ULTRIX_SYS_chown	16
#define	ULTRIX_SYS_break	17
				/* 18 is obsolete stat */
#define	ULTRIX_SYS_lseek	19
#define	ULTRIX_SYS_getpid	20
#define	ULTRIX_SYS_mount	21
				/* 22 is obsolete sysV_unmount */
#define	ULTRIX_SYS_setuid	23
#define	ULTRIX_SYS_getuid	24
				/* 25 is obsolete v7 stime */
				/* 26 is obsolete v7 ptrace */
				/* 27 is obsolete v7 alarm */
				/* 28 is obsolete v7 fstat */
				/* 29 is obsolete v7 pause */
				/* 30 is obsolete v7 utime */
				/* 31 is obsolete v7 stty */
				/* 32 is obsolete v7 gtty */
#define	ULTRIX_SYS_access	33
				/* 34 is obsolete v7 nice */
				/* 35 is obsolete v7 ftime */
#define	ULTRIX_SYS_sync	36
#define	ULTRIX_SYS_kill	37
#define	ULTRIX_SYS_ostat	38
				/* 39 is obsolete v7 setpgrp */
#define	ULTRIX_SYS_olstat	40
#define	ULTRIX_SYS_dup	41
#define	ULTRIX_SYS_pipe	42
				/* 43 is obsolete v7 times */
#define	ULTRIX_SYS_profil	44
				/* 46 is obsolete v7 setgid */
#define	ULTRIX_SYS_getgid	47
#define	ULTRIX_SYS_acct	51
#define	ULTRIX_SYS_ioctl	54
#define	ULTRIX_SYS_reboot	55
#define	ULTRIX_SYS_symlink	57
#define	ULTRIX_SYS_readlink	58
#define	ULTRIX_SYS_execve	59
#define	ULTRIX_SYS_umask	60
#define	ULTRIX_SYS_chroot	61
#define	ULTRIX_SYS_fstat	62
#define	ULTRIX_SYS_getpagesize	64
#define	ULTRIX_SYS_vfork	66
				/* 67 is obsolete vread */
				/* 68 is obsolete vwrite */
#define	ULTRIX_SYS_sbrk	69
#define	ULTRIX_SYS_sstk	70
#define	ULTRIX_SYS_mmap	71
#define	ULTRIX_SYS_vadvise	72
#define	ULTRIX_SYS_munmap	73
#define	ULTRIX_SYS_mprotect	74
#define	ULTRIX_SYS_madvise	75
#define	ULTRIX_SYS_vhangup	76
#define	ULTRIX_SYS_mincore	78
#define	ULTRIX_SYS_getgroups	79
#define	ULTRIX_SYS_setgroups	80
#define	ULTRIX_SYS_getpgrp	81
#define	ULTRIX_SYS_setpgrp	82
#define	ULTRIX_SYS_setitimer	83
#define	ULTRIX_SYS_wait3	84
#define	ULTRIX_SYS_swapon	85
#define	ULTRIX_SYS_getitimer	86
#define	ULTRIX_SYS_gethostname	87
#define	ULTRIX_SYS_sethostname	88
#define	ULTRIX_SYS_getdtablesize	89
#define	ULTRIX_SYS_dup2	90
#define	ULTRIX_SYS_fcntl	92
#define	ULTRIX_SYS_select	93
#define	ULTRIX_SYS_fsync	95
#define	ULTRIX_SYS_setpriority	96
#define	ULTRIX_SYS_socket	97
#define	ULTRIX_SYS_connect	98
#define	ULTRIX_SYS_accept	99
#define	ULTRIX_SYS_getpriority	100
#define	ULTRIX_SYS_send	101
#define	ULTRIX_SYS_recv	102
#define	ULTRIX_SYS_sigreturn	103
#define	ULTRIX_SYS_bind	104
#define	ULTRIX_SYS_setsockopt	105
#define	ULTRIX_SYS_listen	106
#define	ULTRIX_SYS_sigvec	108
#define	ULTRIX_SYS_sigblock	109
#define	ULTRIX_SYS_sigsetmask	110
#define	ULTRIX_SYS_sigsuspend	111
#define	ULTRIX_SYS_sigstack	112
#define	ULTRIX_SYS_recvmsg	113
#define	ULTRIX_SYS_sendmsg	114
				/* 115 is obsolete vtrace */
#define	ULTRIX_SYS_gettimeofday	116
#define	ULTRIX_SYS_getrusage	117
#define	ULTRIX_SYS_getsockopt	118
#define	ULTRIX_SYS_readv	120
#define	ULTRIX_SYS_writev	121
#define	ULTRIX_SYS_settimeofday	122
#define	ULTRIX_SYS_fchown	123
#define	ULTRIX_SYS_fchmod	124
#define	ULTRIX_SYS_recvfrom	125
#define	ULTRIX_SYS_setreuid	126
#define	ULTRIX_SYS_setregid	127
#define	ULTRIX_SYS_rename	128
#define	ULTRIX_SYS_truncate	129
#define	ULTRIX_SYS_ftruncate	130
#define	ULTRIX_SYS_flock	131
#define	ULTRIX_SYS_sendto	133
#define	ULTRIX_SYS_shutdown	134
#define	ULTRIX_SYS_socketpair	135
#define	ULTRIX_SYS_mkdir	136
#define	ULTRIX_SYS_rmdir	137
#define	ULTRIX_SYS_utimes	138
#define	ULTRIX_SYS_sigcleanup	139
#define	ULTRIX_SYS_adjtime	140
#define	ULTRIX_SYS_getpeername	141
#define	ULTRIX_SYS_gethostid	142
#define	ULTRIX_SYS_getrlimit	144
#define	ULTRIX_SYS_setrlimit	145
#define	ULTRIX_SYS_killpg	146
#define	ULTRIX_SYS_getsockname	150
#define	ULTRIX_SYS_nfssvc	158
#define	ULTRIX_SYS_getdirentries	159
#define	ULTRIX_SYS_statfs	160
#define	ULTRIX_SYS_fstatfs	161
#define	ULTRIX_SYS_async_daemon	163
#define	ULTRIX_SYS_getfh	164
#define	ULTRIX_SYS_getdomainname	165
#define	ULTRIX_SYS_setdomainname	166
#define	ULTRIX_SYS_quotactl	168
#define	ULTRIX_SYS_exportfs	169
#define	ULTRIX_SYS_uname	179
#define	ULTRIX_SYS_ustat	183
#define	ULTRIX_SYS_getmnt	184
#define	ULTRIX_SYS_sigpending	187
#define	ULTRIX_SYS_setsid	188
#define	ULTRIX_SYS_waitpid	189
#define	ULTRIX_SYS_getsysinfo	256
#define	ULTRIX_SYS_setsysinfo	257
#define	ULTRIX_SYS_MAXSYSCALL	258
