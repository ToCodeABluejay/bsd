/*	$OpenBSD: linux_syscalls.c,v 1.78 2013/11/03 13:56:03 pirofti Exp $	*/

/*
 * System call names.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from	OpenBSD: syscalls.master,v 1.74 2013/10/25 05:10:03 guenther Exp 
 */

char *linux_syscallnames[] = {
	"syscall",			/* 0 = syscall */
	"exit",			/* 1 = exit */
	"fork",			/* 2 = fork */
	"read",			/* 3 = read */
	"write",			/* 4 = write */
	"open",			/* 5 = open */
	"close",			/* 6 = close */
	"waitpid",			/* 7 = waitpid */
	"creat",			/* 8 = creat */
	"link",			/* 9 = link */
	"unlink",			/* 10 = unlink */
	"execve",			/* 11 = execve */
	"chdir",			/* 12 = chdir */
	"time",			/* 13 = time */
	"mknod",			/* 14 = mknod */
	"chmod",			/* 15 = chmod */
	"lchown16",			/* 16 = lchown16 */
	"break",			/* 17 = break */
	"ostat",			/* 18 = ostat */
	"lseek",			/* 19 = lseek */
	"getpid",			/* 20 = getpid */
	"mount",			/* 21 = mount */
	"umount",			/* 22 = umount */
	"linux_setuid16",			/* 23 = linux_setuid16 */
	"linux_getuid16",			/* 24 = linux_getuid16 */
	"#25 (unimplemented stime)",		/* 25 = unimplemented stime */
#ifdef PTRACE
	"ptrace",			/* 26 = ptrace */
#else
	"#26 (unimplemented ptrace)",		/* 26 = unimplemented ptrace */
#endif
	"alarm",			/* 27 = alarm */
	"ofstat",			/* 28 = ofstat */
	"pause",			/* 29 = pause */
	"utime",			/* 30 = utime */
	"stty",			/* 31 = stty */
	"gtty",			/* 32 = gtty */
	"access",			/* 33 = access */
	"nice",			/* 34 = nice */
	"ftime",			/* 35 = ftime */
	"sync",			/* 36 = sync */
	"kill",			/* 37 = kill */
	"rename",			/* 38 = rename */
	"mkdir",			/* 39 = mkdir */
	"rmdir",			/* 40 = rmdir */
	"dup",			/* 41 = dup */
	"pipe",			/* 42 = pipe */
	"times",			/* 43 = times */
	"prof",			/* 44 = prof */
	"brk",			/* 45 = brk */
	"linux_setgid16",			/* 46 = linux_setgid16 */
	"linux_getgid16",			/* 47 = linux_getgid16 */
	"signal",			/* 48 = signal */
	"linux_geteuid16",			/* 49 = linux_geteuid16 */
	"linux_getegid16",			/* 50 = linux_getegid16 */
#ifdef ACCOUNTING
	"acct",			/* 51 = acct */
#else
	"#51 (unimplemented acct)",		/* 51 = unimplemented acct */
#endif
	"phys",			/* 52 = phys */
	"lock",			/* 53 = lock */
	"ioctl",			/* 54 = ioctl */
	"fcntl",			/* 55 = fcntl */
	"mpx",			/* 56 = mpx */
	"setpgid",			/* 57 = setpgid */
	"ulimit",			/* 58 = ulimit */
	"oldolduname",			/* 59 = oldolduname */
	"umask",			/* 60 = umask */
	"chroot",			/* 61 = chroot */
	"ustat",			/* 62 = ustat */
	"dup2",			/* 63 = dup2 */
	"getppid",			/* 64 = getppid */
	"getpgrp",			/* 65 = getpgrp */
	"setsid",			/* 66 = setsid */
	"sigaction",			/* 67 = sigaction */
	"siggetmask",			/* 68 = siggetmask */
	"sigsetmask",			/* 69 = sigsetmask */
	"setreuid16",			/* 70 = setreuid16 */
	"setregid16",			/* 71 = setregid16 */
	"sigsuspend",			/* 72 = sigsuspend */
	"sigpending",			/* 73 = sigpending */
	"sethostname",			/* 74 = sethostname */
	"setrlimit",			/* 75 = setrlimit */
	"getrlimit",			/* 76 = getrlimit */
	"getrusage",			/* 77 = getrusage */
	"gettimeofday",			/* 78 = gettimeofday */
	"#79 (unimplemented settimeofday)",		/* 79 = unimplemented settimeofday */
	"linux_getgroups",			/* 80 = linux_getgroups */
	"linux_setgroups",			/* 81 = linux_setgroups */
	"oldselect",			/* 82 = oldselect */
	"symlink",			/* 83 = symlink */
	"olstat",			/* 84 = olstat */
	"readlink",			/* 85 = readlink */
	"#86 (unimplemented linux_sys_uselib)",		/* 86 = unimplemented linux_sys_uselib */
	"swapon",			/* 87 = swapon */
	"reboot",			/* 88 = reboot */
	"readdir",			/* 89 = readdir */
	"mmap",			/* 90 = mmap */
	"munmap",			/* 91 = munmap */
	"truncate",			/* 92 = truncate */
	"ftruncate",			/* 93 = ftruncate */
	"fchmod",			/* 94 = fchmod */
	"fchown16",			/* 95 = fchown16 */
	"getpriority",			/* 96 = getpriority */
	"setpriority",			/* 97 = setpriority */
	"profil",			/* 98 = profil */
	"statfs",			/* 99 = statfs */
	"fstatfs",			/* 100 = fstatfs */
#ifdef __i386__
	"ioperm",			/* 101 = ioperm */
#else
	"ioperm",			/* 101 = ioperm */
#endif
	"socketcall",			/* 102 = socketcall */
	"klog",			/* 103 = klog */
	"setitimer",			/* 104 = setitimer */
	"getitimer",			/* 105 = getitimer */
	"stat",			/* 106 = stat */
	"lstat",			/* 107 = lstat */
	"fstat",			/* 108 = fstat */
	"olduname",			/* 109 = olduname */
#ifdef __i386__
	"iopl",			/* 110 = iopl */
#else
	"iopl",			/* 110 = iopl */
#endif
	"vhangup",			/* 111 = vhangup */
	"idle",			/* 112 = idle */
	"vm86old",			/* 113 = vm86old */
	"wait4",			/* 114 = wait4 */
	"swapoff",			/* 115 = swapoff */
	"sysinfo",			/* 116 = sysinfo */
	"ipc",			/* 117 = ipc */
	"fsync",			/* 118 = fsync */
	"sigreturn",			/* 119 = sigreturn */
	"clone",			/* 120 = clone */
	"setdomainname",			/* 121 = setdomainname */
	"uname",			/* 122 = uname */
#ifdef __i386__
	"modify_ldt",			/* 123 = modify_ldt */
#else
	"modify_ldt",			/* 123 = modify_ldt */
#endif
	"adjtimex",			/* 124 = adjtimex */
	"mprotect",			/* 125 = mprotect */
	"sigprocmask",			/* 126 = sigprocmask */
	"create_module",			/* 127 = create_module */
	"init_module",			/* 128 = init_module */
	"delete_module",			/* 129 = delete_module */
	"get_kernel_syms",			/* 130 = get_kernel_syms */
	"quotactl",			/* 131 = quotactl */
	"getpgid",			/* 132 = getpgid */
	"fchdir",			/* 133 = fchdir */
	"bdflush",			/* 134 = bdflush */
	"sysfs",			/* 135 = sysfs */
	"personality",			/* 136 = personality */
	"afs_syscall",			/* 137 = afs_syscall */
	"linux_setfsuid16",			/* 138 = linux_setfsuid16 */
	"linux_getfsuid16",			/* 139 = linux_getfsuid16 */
	"llseek",			/* 140 = llseek */
	"getdents",			/* 141 = getdents */
	"select",			/* 142 = select */
	"flock",			/* 143 = flock */
	"msync",			/* 144 = msync */
	"readv",			/* 145 = readv */
	"writev",			/* 146 = writev */
	"getsid",			/* 147 = getsid */
	"fdatasync",			/* 148 = fdatasync */
	"__sysctl",			/* 149 = __sysctl */
	"mlock",			/* 150 = mlock */
	"munlock",			/* 151 = munlock */
	"mlockall",			/* 152 = mlockall */
	"munlockall",			/* 153 = munlockall */
	"sched_setparam",			/* 154 = sched_setparam */
	"sched_getparam",			/* 155 = sched_getparam */
	"sched_setscheduler",			/* 156 = sched_setscheduler */
	"sched_getscheduler",			/* 157 = sched_getscheduler */
	"sched_yield",			/* 158 = sched_yield */
	"sched_get_priority_max",			/* 159 = sched_get_priority_max */
	"sched_get_priority_min",			/* 160 = sched_get_priority_min */
	"sched_rr_get_interval",			/* 161 = sched_rr_get_interval */
	"nanosleep",			/* 162 = nanosleep */
	"mremap",			/* 163 = mremap */
	"setresuid16",			/* 164 = setresuid16 */
	"getresuid16",			/* 165 = getresuid16 */
	"vm86",			/* 166 = vm86 */
	"query_module",			/* 167 = query_module */
	"poll",			/* 168 = poll */
	"nfsservctl",			/* 169 = nfsservctl */
	"setresgid16",			/* 170 = setresgid16 */
	"getresgid16",			/* 171 = getresgid16 */
	"prctl",			/* 172 = prctl */
	"rt_sigreturn",			/* 173 = rt_sigreturn */
	"rt_sigaction",			/* 174 = rt_sigaction */
	"rt_sigprocmask",			/* 175 = rt_sigprocmask */
	"rt_sigpending",			/* 176 = rt_sigpending */
	"rt_sigtimedwait",			/* 177 = rt_sigtimedwait */
	"rt_queueinfo",			/* 178 = rt_queueinfo */
	"rt_sigsuspend",			/* 179 = rt_sigsuspend */
	"pread",			/* 180 = pread */
	"pwrite",			/* 181 = pwrite */
	"chown16",			/* 182 = chown16 */
	"__getcwd",			/* 183 = __getcwd */
	"capget",			/* 184 = capget */
	"capset",			/* 185 = capset */
	"sigaltstack",			/* 186 = sigaltstack */
	"sendfile",			/* 187 = sendfile */
	"getpmsg",			/* 188 = getpmsg */
	"putpmsg",			/* 189 = putpmsg */
	"vfork",			/* 190 = vfork */
	"ugetrlimit",			/* 191 = ugetrlimit */
	"mmap2",			/* 192 = mmap2 */
	"truncate64",			/* 193 = truncate64 */
	"ftruncate64",			/* 194 = ftruncate64 */
	"stat64",			/* 195 = stat64 */
	"lstat64",			/* 196 = lstat64 */
	"fstat64",			/* 197 = fstat64 */
	"lchown",			/* 198 = lchown */
	"getuid",			/* 199 = getuid */
	"getgid",			/* 200 = getgid */
	"geteuid",			/* 201 = geteuid */
	"getegid",			/* 202 = getegid */
	"setreuid",			/* 203 = setreuid */
	"setregid",			/* 204 = setregid */
	"getgroups",			/* 205 = getgroups */
	"setgroups",			/* 206 = setgroups */
	"fchown",			/* 207 = fchown */
	"setresuid",			/* 208 = setresuid */
	"getresuid",			/* 209 = getresuid */
	"setresgid",			/* 210 = setresgid */
	"getresgid",			/* 211 = getresgid */
	"chown",			/* 212 = chown */
	"setuid",			/* 213 = setuid */
	"setgid",			/* 214 = setgid */
	"setfsuid",			/* 215 = setfsuid */
	"setfsgid",			/* 216 = setfsgid */
	"pivot_root",			/* 217 = pivot_root */
	"mincore",			/* 218 = mincore */
	"madvise",			/* 219 = madvise */
	"getdents64",			/* 220 = getdents64 */
	"fcntl64",			/* 221 = fcntl64 */
	"#222 (unimplemented)",		/* 222 = unimplemented */
	"#223 (unimplemented)",		/* 223 = unimplemented */
	"gettid",			/* 224 = gettid */
	"#225 (unimplemented linux_sys_readahead)",		/* 225 = unimplemented linux_sys_readahead */
	"setxattr",			/* 226 = setxattr */
	"lsetxattr",			/* 227 = lsetxattr */
	"fsetxattr",			/* 228 = fsetxattr */
	"getxattr",			/* 229 = getxattr */
	"lgetxattr",			/* 230 = lgetxattr */
	"fgetxattr",			/* 231 = fgetxattr */
	"listxattr",			/* 232 = listxattr */
	"llistxattr",			/* 233 = llistxattr */
	"flistxattr",			/* 234 = flistxattr */
	"removexattr",			/* 235 = removexattr */
	"lremovexattr",			/* 236 = lremovexattr */
	"fremovexattr",			/* 237 = fremovexattr */
	"#238 (unimplemented linux_sys_tkill)",		/* 238 = unimplemented linux_sys_tkill */
	"#239 (unimplemented linux_sys_sendfile64)",		/* 239 = unimplemented linux_sys_sendfile64 */
	"futex",			/* 240 = futex */
	"#241 (unimplemented linux_sys_sched_setaffinity)",		/* 241 = unimplemented linux_sys_sched_setaffinity */
	"#242 (unimplemented linux_sys_sched_getaffinity)",		/* 242 = unimplemented linux_sys_sched_getaffinity */
	"set_thread_area",			/* 243 = set_thread_area */
	"get_thread_area",			/* 244 = get_thread_area */
	"#245 (unimplemented linux_sys_io_setup)",		/* 245 = unimplemented linux_sys_io_setup */
	"#246 (unimplemented linux_sys_io_destroy)",		/* 246 = unimplemented linux_sys_io_destroy */
	"#247 (unimplemented linux_sys_io_getevents)",		/* 247 = unimplemented linux_sys_io_getevents */
	"#248 (unimplemented linux_sys_io_submit)",		/* 248 = unimplemented linux_sys_io_submit */
	"#249 (unimplemented linux_sys_io_cancel)",		/* 249 = unimplemented linux_sys_io_cancel */
	"fadvise64",			/* 250 = fadvise64 */
	"#251 (unimplemented)",		/* 251 = unimplemented */
	"linux_exit_group",			/* 252 = linux_exit_group */
	"#253 (unimplemented linux_sys_lookup_dcookie)",		/* 253 = unimplemented linux_sys_lookup_dcookie */
	"epoll_create",			/* 254 = epoll_create */
	"epoll_ctl",			/* 255 = epoll_ctl */
	"epoll_wait",			/* 256 = epoll_wait */
	"#257 (unimplemented linux_sys_remap_file_pages)",		/* 257 = unimplemented linux_sys_remap_file_pages */
	"set_tid_address",			/* 258 = set_tid_address */
	"#259 (unimplemented linux_sys_timer_create)",		/* 259 = unimplemented linux_sys_timer_create */
	"#260 (unimplemented linux_sys_timer_settime)",		/* 260 = unimplemented linux_sys_timer_settime */
	"#261 (unimplemented linux_sys_timer_gettime)",		/* 261 = unimplemented linux_sys_timer_gettime */
	"#262 (unimplemented linux_sys_timer_getoverrun)",		/* 262 = unimplemented linux_sys_timer_getoverrun */
	"#263 (unimplemented linux_sys_timer_delete)",		/* 263 = unimplemented linux_sys_timer_delete */
	"#264 (unimplemented linux_sys_clock_settime)",		/* 264 = unimplemented linux_sys_clock_settime */
	"clock_gettime",			/* 265 = clock_gettime */
	"clock_getres",			/* 266 = clock_getres */
	"#267 (unimplemented linux_sys_clock_nanosleep)",		/* 267 = unimplemented linux_sys_clock_nanosleep */
	"statfs64",			/* 268 = statfs64 */
	"fstatfs64",			/* 269 = fstatfs64 */
	"tgkill",			/* 270 = tgkill */
	"#271 (unimplemented linux_sys_utimes)",		/* 271 = unimplemented linux_sys_utimes */
	"#272 (unimplemented linux_sys_fadvise64_64)",		/* 272 = unimplemented linux_sys_fadvise64_64 */
	"#273 (unimplemented linux_sys_vserver)",		/* 273 = unimplemented linux_sys_vserver */
	"#274 (unimplemented linux_sys_mbind)",		/* 274 = unimplemented linux_sys_mbind */
	"#275 (unimplemented linux_sys_get_mempolicy)",		/* 275 = unimplemented linux_sys_get_mempolicy */
	"#276 (unimplemented linux_sys_set_mempolicy)",		/* 276 = unimplemented linux_sys_set_mempolicy */
	"#277 (unimplemented linux_sys_mq_open)",		/* 277 = unimplemented linux_sys_mq_open */
	"#278 (unimplemented linux_sys_mq_unlink)",		/* 278 = unimplemented linux_sys_mq_unlink */
	"#279 (unimplemented linux_sys_mq_timedsend)",		/* 279 = unimplemented linux_sys_mq_timedsend */
	"#280 (unimplemented linux_sys_mq_timedreceive)",		/* 280 = unimplemented linux_sys_mq_timedreceive */
	"#281 (unimplemented linux_sys_mq_notify)",		/* 281 = unimplemented linux_sys_mq_notify */
	"#282 (unimplemented linux_sys_mq_getsetattr)",		/* 282 = unimplemented linux_sys_mq_getsetattr */
	"#283 (unimplemented linux_sys_sys_kexec_load)",		/* 283 = unimplemented linux_sys_sys_kexec_load */
	"#284 (unimplemented linux_sys_waitid)",		/* 284 = unimplemented linux_sys_waitid */
	"#285 (unimplemented / * unused * /)",		/* 285 = unimplemented / * unused * / */
	"#286 (unimplemented linux_sys_add_key)",		/* 286 = unimplemented linux_sys_add_key */
	"#287 (unimplemented linux_sys_request_key)",		/* 287 = unimplemented linux_sys_request_key */
	"#288 (unimplemented linux_sys_keyctl)",		/* 288 = unimplemented linux_sys_keyctl */
	"#289 (unimplemented linux_sys_ioprio_set)",		/* 289 = unimplemented linux_sys_ioprio_set */
	"#290 (unimplemented linux_sys_ioprio_get)",		/* 290 = unimplemented linux_sys_ioprio_get */
	"#291 (unimplemented linux_sys_inotify_init)",		/* 291 = unimplemented linux_sys_inotify_init */
	"#292 (unimplemented linux_sys_inotify_add_watch)",		/* 292 = unimplemented linux_sys_inotify_add_watch */
	"#293 (unimplemented linux_sys_inotify_rm_watch)",		/* 293 = unimplemented linux_sys_inotify_rm_watch */
	"#294 (unimplemented linux_sys_migrate_pages)",		/* 294 = unimplemented linux_sys_migrate_pages */
	"#295 (unimplemented linux_sys_openalinux_sys_t)",		/* 295 = unimplemented linux_sys_openalinux_sys_t */
	"#296 (unimplemented linux_sys_mkdirat)",		/* 296 = unimplemented linux_sys_mkdirat */
	"#297 (unimplemented linux_sys_mknodat)",		/* 297 = unimplemented linux_sys_mknodat */
	"#298 (unimplemented linux_sys_fchownat)",		/* 298 = unimplemented linux_sys_fchownat */
	"#299 (unimplemented linux_sys_futimesat)",		/* 299 = unimplemented linux_sys_futimesat */
	"#300 (unimplemented linux_sys_fstatat64)",		/* 300 = unimplemented linux_sys_fstatat64 */
	"#301 (unimplemented linux_sys_unlinkat)",		/* 301 = unimplemented linux_sys_unlinkat */
	"#302 (unimplemented linux_sys_renameat)",		/* 302 = unimplemented linux_sys_renameat */
	"#303 (unimplemented linux_sys_linkat)",		/* 303 = unimplemented linux_sys_linkat */
	"#304 (unimplemented linux_sys_symlinkat)",		/* 304 = unimplemented linux_sys_symlinkat */
	"#305 (unimplemented linux_sys_readlinkat)",		/* 305 = unimplemented linux_sys_readlinkat */
	"#306 (unimplemented linux_sys_fchmodat)",		/* 306 = unimplemented linux_sys_fchmodat */
	"#307 (unimplemented linux_sys_faccessat)",		/* 307 = unimplemented linux_sys_faccessat */
	"#308 (unimplemented linux_sys_pselect6)",		/* 308 = unimplemented linux_sys_pselect6 */
	"#309 (unimplemented linux_sys_ppoll)",		/* 309 = unimplemented linux_sys_ppoll */
	"#310 (unimplemented linux_sys_unshare)",		/* 310 = unimplemented linux_sys_unshare */
	"set_robust_list",			/* 311 = set_robust_list */
	"get_robust_list",			/* 312 = get_robust_list */
	"#313 (unimplemented splice)",		/* 313 = unimplemented splice */
	"#314 (unimplemented sync_file_range)",		/* 314 = unimplemented sync_file_range */
	"#315 (unimplemented tee)",		/* 315 = unimplemented tee */
	"#316 (unimplemented vmsplice)",		/* 316 = unimplemented vmsplice */
	"#317 (unimplemented move_pages)",		/* 317 = unimplemented move_pages */
	"#318 (unimplemented getcpu)",		/* 318 = unimplemented getcpu */
	"epoll_pwait",			/* 319 = epoll_pwait */
	"#320 (unimplemented utimensat)",		/* 320 = unimplemented utimensat */
	"#321 (unimplemented signalfd)",		/* 321 = unimplemented signalfd */
	"#322 (unimplemented timerfd_create)",		/* 322 = unimplemented timerfd_create */
	"eventfd",			/* 323 = eventfd */
	"#324 (unimplemented fallocate)",		/* 324 = unimplemented fallocate */
	"#325 (unimplemented timerfd_settime)",		/* 325 = unimplemented timerfd_settime */
	"#326 (unimplemented timerfd_gettime)",		/* 326 = unimplemented timerfd_gettime */
	"#327 (unimplemented signalfd4)",		/* 327 = unimplemented signalfd4 */
	"eventfd2",			/* 328 = eventfd2 */
	"epoll_create1",			/* 329 = epoll_create1 */
	"#330 (unimplemented dup3)",		/* 330 = unimplemented dup3 */
	"pipe2",			/* 331 = pipe2 */
	"#332 (unimplemented inotify_init1)",		/* 332 = unimplemented inotify_init1 */
	"#333 (unimplemented preadv)",		/* 333 = unimplemented preadv */
	"#334 (unimplemented pwritev)",		/* 334 = unimplemented pwritev */
	"#335 (unimplemented rt_tgsigqueueinfo)",		/* 335 = unimplemented rt_tgsigqueueinfo */
	"#336 (unimplemented perf_counter_open)",		/* 336 = unimplemented perf_counter_open */
	"#337 (unimplemented recvmmsg)",		/* 337 = unimplemented recvmmsg */
};
