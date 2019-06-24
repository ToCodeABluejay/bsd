/*	$OpenBSD: syscall.h,v 1.203 2019/06/24 12:51:05 visa Exp $	*/

/*
 * System call numbers.
 *
 * DO NOT EDIT-- this file is automatically generated.
 * created from;	OpenBSD: syscalls.master,v 1.193 2019/06/24 12:49:54 visa Exp 
 */

/* syscall: "syscall" ret: "int" args: "int" "..." */
#define	SYS_syscall	0

/* syscall: "exit" ret: "void" args: "int" */
#define	SYS_exit	1

/* syscall: "fork" ret: "int" args: */
#define	SYS_fork	2

/* syscall: "read" ret: "ssize_t" args: "int" "void *" "size_t" */
#define	SYS_read	3

/* syscall: "write" ret: "ssize_t" args: "int" "const void *" "size_t" */
#define	SYS_write	4

/* syscall: "open" ret: "int" args: "const char *" "int" "..." */
#define	SYS_open	5

/* syscall: "close" ret: "int" args: "int" */
#define	SYS_close	6

/* syscall: "getentropy" ret: "int" args: "void *" "size_t" */
#define	SYS_getentropy	7

/* syscall: "__tfork" ret: "int" args: "const struct __tfork *" "size_t" */
#define	SYS___tfork	8

/* syscall: "link" ret: "int" args: "const char *" "const char *" */
#define	SYS_link	9

/* syscall: "unlink" ret: "int" args: "const char *" */
#define	SYS_unlink	10

/* syscall: "wait4" ret: "pid_t" args: "pid_t" "int *" "int" "struct rusage *" */
#define	SYS_wait4	11

/* syscall: "chdir" ret: "int" args: "const char *" */
#define	SYS_chdir	12

/* syscall: "fchdir" ret: "int" args: "int" */
#define	SYS_fchdir	13

/* syscall: "mknod" ret: "int" args: "const char *" "mode_t" "dev_t" */
#define	SYS_mknod	14

/* syscall: "chmod" ret: "int" args: "const char *" "mode_t" */
#define	SYS_chmod	15

/* syscall: "chown" ret: "int" args: "const char *" "uid_t" "gid_t" */
#define	SYS_chown	16

/* syscall: "break" ret: "int" args: "char *" */
#define	SYS_break	17

/* syscall: "getdtablecount" ret: "int" args: */
#define	SYS_getdtablecount	18

/* syscall: "getrusage" ret: "int" args: "int" "struct rusage *" */
#define	SYS_getrusage	19

/* syscall: "getpid" ret: "pid_t" args: */
#define	SYS_getpid	20

/* syscall: "mount" ret: "int" args: "const char *" "const char *" "int" "void *" */
#define	SYS_mount	21

/* syscall: "unmount" ret: "int" args: "const char *" "int" */
#define	SYS_unmount	22

/* syscall: "setuid" ret: "int" args: "uid_t" */
#define	SYS_setuid	23

/* syscall: "getuid" ret: "uid_t" args: */
#define	SYS_getuid	24

/* syscall: "geteuid" ret: "uid_t" args: */
#define	SYS_geteuid	25

/* syscall: "ptrace" ret: "int" args: "int" "pid_t" "caddr_t" "int" */
#define	SYS_ptrace	26

/* syscall: "recvmsg" ret: "ssize_t" args: "int" "struct msghdr *" "int" */
#define	SYS_recvmsg	27

/* syscall: "sendmsg" ret: "ssize_t" args: "int" "const struct msghdr *" "int" */
#define	SYS_sendmsg	28

/* syscall: "recvfrom" ret: "ssize_t" args: "int" "void *" "size_t" "int" "struct sockaddr *" "socklen_t *" */
#define	SYS_recvfrom	29

/* syscall: "accept" ret: "int" args: "int" "struct sockaddr *" "socklen_t *" */
#define	SYS_accept	30

/* syscall: "getpeername" ret: "int" args: "int" "struct sockaddr *" "socklen_t *" */
#define	SYS_getpeername	31

/* syscall: "getsockname" ret: "int" args: "int" "struct sockaddr *" "socklen_t *" */
#define	SYS_getsockname	32

/* syscall: "access" ret: "int" args: "const char *" "int" */
#define	SYS_access	33

/* syscall: "chflags" ret: "int" args: "const char *" "u_int" */
#define	SYS_chflags	34

/* syscall: "fchflags" ret: "int" args: "int" "u_int" */
#define	SYS_fchflags	35

/* syscall: "sync" ret: "void" args: */
#define	SYS_sync	36

				/* 37 is obsolete o58_kill */
/* syscall: "stat" ret: "int" args: "const char *" "struct stat *" */
#define	SYS_stat	38

/* syscall: "getppid" ret: "pid_t" args: */
#define	SYS_getppid	39

/* syscall: "lstat" ret: "int" args: "const char *" "struct stat *" */
#define	SYS_lstat	40

/* syscall: "dup" ret: "int" args: "int" */
#define	SYS_dup	41

/* syscall: "fstatat" ret: "int" args: "int" "const char *" "struct stat *" "int" */
#define	SYS_fstatat	42

/* syscall: "getegid" ret: "gid_t" args: */
#define	SYS_getegid	43

/* syscall: "profil" ret: "int" args: "caddr_t" "size_t" "u_long" "u_int" */
#define	SYS_profil	44

/* syscall: "ktrace" ret: "int" args: "const char *" "int" "int" "pid_t" */
#define	SYS_ktrace	45

/* syscall: "sigaction" ret: "int" args: "int" "const struct sigaction *" "struct sigaction *" */
#define	SYS_sigaction	46

/* syscall: "getgid" ret: "gid_t" args: */
#define	SYS_getgid	47

/* syscall: "sigprocmask" ret: "int" args: "int" "sigset_t" */
#define	SYS_sigprocmask	48

				/* 49 is obsolete ogetlogin */
/* syscall: "setlogin" ret: "int" args: "const char *" */
#define	SYS_setlogin	50

/* syscall: "acct" ret: "int" args: "const char *" */
#define	SYS_acct	51

/* syscall: "sigpending" ret: "int" args: */
#define	SYS_sigpending	52

/* syscall: "fstat" ret: "int" args: "int" "struct stat *" */
#define	SYS_fstat	53

/* syscall: "ioctl" ret: "int" args: "int" "u_long" "..." */
#define	SYS_ioctl	54

/* syscall: "reboot" ret: "int" args: "int" */
#define	SYS_reboot	55

/* syscall: "revoke" ret: "int" args: "const char *" */
#define	SYS_revoke	56

/* syscall: "symlink" ret: "int" args: "const char *" "const char *" */
#define	SYS_symlink	57

/* syscall: "readlink" ret: "ssize_t" args: "const char *" "char *" "size_t" */
#define	SYS_readlink	58

/* syscall: "execve" ret: "int" args: "const char *" "char *const *" "char *const *" */
#define	SYS_execve	59

/* syscall: "umask" ret: "mode_t" args: "mode_t" */
#define	SYS_umask	60

/* syscall: "chroot" ret: "int" args: "const char *" */
#define	SYS_chroot	61

/* syscall: "getfsstat" ret: "int" args: "struct statfs *" "size_t" "int" */
#define	SYS_getfsstat	62

/* syscall: "statfs" ret: "int" args: "const char *" "struct statfs *" */
#define	SYS_statfs	63

/* syscall: "fstatfs" ret: "int" args: "int" "struct statfs *" */
#define	SYS_fstatfs	64

/* syscall: "fhstatfs" ret: "int" args: "const fhandle_t *" "struct statfs *" */
#define	SYS_fhstatfs	65

/* syscall: "vfork" ret: "int" args: */
#define	SYS_vfork	66

/* syscall: "gettimeofday" ret: "int" args: "struct timeval *" "struct timezone *" */
#define	SYS_gettimeofday	67

/* syscall: "settimeofday" ret: "int" args: "const struct timeval *" "const struct timezone *" */
#define	SYS_settimeofday	68

/* syscall: "setitimer" ret: "int" args: "int" "const struct itimerval *" "struct itimerval *" */
#define	SYS_setitimer	69

/* syscall: "getitimer" ret: "int" args: "int" "struct itimerval *" */
#define	SYS_getitimer	70

/* syscall: "select" ret: "int" args: "int" "fd_set *" "fd_set *" "fd_set *" "struct timeval *" */
#define	SYS_select	71

/* syscall: "kevent" ret: "int" args: "int" "const struct kevent *" "int" "struct kevent *" "int" "const struct timespec *" */
#define	SYS_kevent	72

/* syscall: "munmap" ret: "int" args: "void *" "size_t" */
#define	SYS_munmap	73

/* syscall: "mprotect" ret: "int" args: "void *" "size_t" "int" */
#define	SYS_mprotect	74

/* syscall: "madvise" ret: "int" args: "void *" "size_t" "int" */
#define	SYS_madvise	75

/* syscall: "utimes" ret: "int" args: "const char *" "const struct timeval *" */
#define	SYS_utimes	76

/* syscall: "futimes" ret: "int" args: "int" "const struct timeval *" */
#define	SYS_futimes	77

/* syscall: "getgroups" ret: "int" args: "int" "gid_t *" */
#define	SYS_getgroups	79

/* syscall: "setgroups" ret: "int" args: "int" "const gid_t *" */
#define	SYS_setgroups	80

/* syscall: "getpgrp" ret: "int" args: */
#define	SYS_getpgrp	81

/* syscall: "setpgid" ret: "int" args: "pid_t" "pid_t" */
#define	SYS_setpgid	82

/* syscall: "futex" ret: "int" args: "uint32_t *" "int" "int" "const struct timespec *" "uint32_t *" */
#define	SYS_futex	83

/* syscall: "utimensat" ret: "int" args: "int" "const char *" "const struct timespec *" "int" */
#define	SYS_utimensat	84

/* syscall: "futimens" ret: "int" args: "int" "const struct timespec *" */
#define	SYS_futimens	85

/* syscall: "kbind" ret: "int" args: "const struct __kbind *" "size_t" "int64_t" */
#define	SYS_kbind	86

/* syscall: "clock_gettime" ret: "int" args: "clockid_t" "struct timespec *" */
#define	SYS_clock_gettime	87

/* syscall: "clock_settime" ret: "int" args: "clockid_t" "const struct timespec *" */
#define	SYS_clock_settime	88

/* syscall: "clock_getres" ret: "int" args: "clockid_t" "struct timespec *" */
#define	SYS_clock_getres	89

/* syscall: "dup2" ret: "int" args: "int" "int" */
#define	SYS_dup2	90

/* syscall: "nanosleep" ret: "int" args: "const struct timespec *" "struct timespec *" */
#define	SYS_nanosleep	91

/* syscall: "fcntl" ret: "int" args: "int" "int" "..." */
#define	SYS_fcntl	92

/* syscall: "accept4" ret: "int" args: "int" "struct sockaddr *" "socklen_t *" "int" */
#define	SYS_accept4	93

/* syscall: "__thrsleep" ret: "int" args: "const volatile void *" "clockid_t" "const struct timespec *" "void *" "const int *" */
#define	SYS___thrsleep	94

/* syscall: "fsync" ret: "int" args: "int" */
#define	SYS_fsync	95

/* syscall: "setpriority" ret: "int" args: "int" "id_t" "int" */
#define	SYS_setpriority	96

/* syscall: "socket" ret: "int" args: "int" "int" "int" */
#define	SYS_socket	97

/* syscall: "connect" ret: "int" args: "int" "const struct sockaddr *" "socklen_t" */
#define	SYS_connect	98

/* syscall: "getdents" ret: "int" args: "int" "void *" "size_t" */
#define	SYS_getdents	99

/* syscall: "getpriority" ret: "int" args: "int" "id_t" */
#define	SYS_getpriority	100

/* syscall: "pipe2" ret: "int" args: "int *" "int" */
#define	SYS_pipe2	101

/* syscall: "dup3" ret: "int" args: "int" "int" "int" */
#define	SYS_dup3	102

/* syscall: "sigreturn" ret: "int" args: "struct sigcontext *" */
#define	SYS_sigreturn	103

/* syscall: "bind" ret: "int" args: "int" "const struct sockaddr *" "socklen_t" */
#define	SYS_bind	104

/* syscall: "setsockopt" ret: "int" args: "int" "int" "int" "const void *" "socklen_t" */
#define	SYS_setsockopt	105

/* syscall: "listen" ret: "int" args: "int" "int" */
#define	SYS_listen	106

/* syscall: "chflagsat" ret: "int" args: "int" "const char *" "u_int" "int" */
#define	SYS_chflagsat	107

/* syscall: "pledge" ret: "int" args: "const char *" "const char *" */
#define	SYS_pledge	108

/* syscall: "ppoll" ret: "int" args: "struct pollfd *" "u_int" "const struct timespec *" "const sigset_t *" */
#define	SYS_ppoll	109

/* syscall: "pselect" ret: "int" args: "int" "fd_set *" "fd_set *" "fd_set *" "const struct timespec *" "const sigset_t *" */
#define	SYS_pselect	110

/* syscall: "sigsuspend" ret: "int" args: "int" */
#define	SYS_sigsuspend	111

/* syscall: "sendsyslog" ret: "int" args: "const char *" "size_t" "int" */
#define	SYS_sendsyslog	112

/* syscall: "unveil" ret: "int" args: "const char *" "const char *" */
#define	SYS_unveil	114

/* syscall: "__realpath" ret: "int" args: "const char *" "char *" */
#define	SYS___realpath	115

				/* 116 is obsolete t32_gettimeofday */
				/* 117 is obsolete t32_getrusage */
/* syscall: "getsockopt" ret: "int" args: "int" "int" "int" "void *" "socklen_t *" */
#define	SYS_getsockopt	118

/* syscall: "thrkill" ret: "int" args: "pid_t" "int" "void *" */
#define	SYS_thrkill	119

/* syscall: "readv" ret: "ssize_t" args: "int" "const struct iovec *" "int" */
#define	SYS_readv	120

/* syscall: "writev" ret: "ssize_t" args: "int" "const struct iovec *" "int" */
#define	SYS_writev	121

/* syscall: "kill" ret: "int" args: "int" "int" */
#define	SYS_kill	122

/* syscall: "fchown" ret: "int" args: "int" "uid_t" "gid_t" */
#define	SYS_fchown	123

/* syscall: "fchmod" ret: "int" args: "int" "mode_t" */
#define	SYS_fchmod	124

				/* 125 is obsolete orecvfrom */
/* syscall: "setreuid" ret: "int" args: "uid_t" "uid_t" */
#define	SYS_setreuid	126

/* syscall: "setregid" ret: "int" args: "gid_t" "gid_t" */
#define	SYS_setregid	127

/* syscall: "rename" ret: "int" args: "const char *" "const char *" */
#define	SYS_rename	128

				/* 129 is obsolete otruncate */
				/* 130 is obsolete oftruncate */
/* syscall: "flock" ret: "int" args: "int" "int" */
#define	SYS_flock	131

/* syscall: "mkfifo" ret: "int" args: "const char *" "mode_t" */
#define	SYS_mkfifo	132

/* syscall: "sendto" ret: "ssize_t" args: "int" "const void *" "size_t" "int" "const struct sockaddr *" "socklen_t" */
#define	SYS_sendto	133

/* syscall: "shutdown" ret: "int" args: "int" "int" */
#define	SYS_shutdown	134

/* syscall: "socketpair" ret: "int" args: "int" "int" "int" "int *" */
#define	SYS_socketpair	135

/* syscall: "mkdir" ret: "int" args: "const char *" "mode_t" */
#define	SYS_mkdir	136

/* syscall: "rmdir" ret: "int" args: "const char *" */
#define	SYS_rmdir	137

				/* 138 is obsolete t32_utimes */
				/* 139 is obsolete 4.2 sigreturn */
/* syscall: "adjtime" ret: "int" args: "const struct timeval *" "struct timeval *" */
#define	SYS_adjtime	140

/* syscall: "getlogin_r" ret: "int" args: "char *" "u_int" */
#define	SYS_getlogin_r	141

				/* 142 is obsolete ogethostid */
				/* 143 is obsolete osethostid */
				/* 144 is obsolete ogetrlimit */
				/* 145 is obsolete osetrlimit */
				/* 146 is obsolete okillpg */
/* syscall: "setsid" ret: "int" args: */
#define	SYS_setsid	147

/* syscall: "quotactl" ret: "int" args: "const char *" "int" "int" "char *" */
#define	SYS_quotactl	148

				/* 149 is obsolete oquota */
				/* 150 is obsolete ogetsockname */
/* syscall: "nfssvc" ret: "int" args: "int" "void *" */
#define	SYS_nfssvc	155

				/* 156 is obsolete ogetdirentries */
				/* 157 is obsolete statfs25 */
				/* 158 is obsolete fstatfs25 */
/* syscall: "getfh" ret: "int" args: "const char *" "fhandle_t *" */
#define	SYS_getfh	161

				/* 162 is obsolete ogetdomainname */
				/* 163 is obsolete osetdomainname */
/* syscall: "sysarch" ret: "int" args: "int" "void *" */
#define	SYS_sysarch	165

				/* 169 is obsolete semsys10 */
				/* 170 is obsolete msgsys10 */
				/* 171 is obsolete shmsys10 */
/* syscall: "pread" ret: "ssize_t" args: "int" "void *" "size_t" "int" "off_t" */
#define	SYS_pread	173

/* syscall: "pwrite" ret: "ssize_t" args: "int" "const void *" "size_t" "int" "off_t" */
#define	SYS_pwrite	174

/* syscall: "setgid" ret: "int" args: "gid_t" */
#define	SYS_setgid	181

/* syscall: "setegid" ret: "int" args: "gid_t" */
#define	SYS_setegid	182

/* syscall: "seteuid" ret: "int" args: "uid_t" */
#define	SYS_seteuid	183

				/* 184 is obsolete lfs_bmapv */
				/* 185 is obsolete lfs_markv */
				/* 186 is obsolete lfs_segclean */
				/* 187 is obsolete lfs_segwait */
				/* 188 is obsolete stat35 */
				/* 189 is obsolete fstat35 */
				/* 190 is obsolete lstat35 */
/* syscall: "pathconf" ret: "long" args: "const char *" "int" */
#define	SYS_pathconf	191

/* syscall: "fpathconf" ret: "long" args: "int" "int" */
#define	SYS_fpathconf	192

/* syscall: "swapctl" ret: "int" args: "int" "const void *" "int" */
#define	SYS_swapctl	193

/* syscall: "getrlimit" ret: "int" args: "int" "struct rlimit *" */
#define	SYS_getrlimit	194

/* syscall: "setrlimit" ret: "int" args: "int" "const struct rlimit *" */
#define	SYS_setrlimit	195

				/* 196 is obsolete ogetdirentries48 */
/* syscall: "mmap" ret: "void *" args: "void *" "size_t" "int" "int" "int" "long" "off_t" */
#define	SYS_mmap	197

/* syscall: "__syscall" ret: "quad_t" args: "quad_t" "..." */
#define	SYS___syscall	198

/* syscall: "lseek" ret: "off_t" args: "int" "int" "off_t" "int" */
#define	SYS_lseek	199

/* syscall: "truncate" ret: "int" args: "const char *" "int" "off_t" */
#define	SYS_truncate	200

/* syscall: "ftruncate" ret: "int" args: "int" "int" "off_t" */
#define	SYS_ftruncate	201

/* syscall: "sysctl" ret: "int" args: "const int *" "u_int" "void *" "size_t *" "void *" "size_t" */
#define	SYS_sysctl	202

/* syscall: "mlock" ret: "int" args: "const void *" "size_t" */
#define	SYS_mlock	203

/* syscall: "munlock" ret: "int" args: "const void *" "size_t" */
#define	SYS_munlock	204

				/* 206 is obsolete t32_futimes */
/* syscall: "getpgid" ret: "pid_t" args: "pid_t" */
#define	SYS_getpgid	207

				/* 208 is obsolete nnpfspioctl */
/* syscall: "utrace" ret: "int" args: "const char *" "const void *" "size_t" */
#define	SYS_utrace	209

/* syscall: "semget" ret: "int" args: "key_t" "int" "int" */
#define	SYS_semget	221

				/* 222 is obsolete semop35 */
				/* 223 is obsolete semconfig35 */
/* syscall: "msgget" ret: "int" args: "key_t" "int" */
#define	SYS_msgget	225

/* syscall: "msgsnd" ret: "int" args: "int" "const void *" "size_t" "int" */
#define	SYS_msgsnd	226

/* syscall: "msgrcv" ret: "int" args: "int" "void *" "size_t" "long" "int" */
#define	SYS_msgrcv	227

/* syscall: "shmat" ret: "void *" args: "int" "const void *" "int" */
#define	SYS_shmat	228

/* syscall: "shmdt" ret: "int" args: "const void *" */
#define	SYS_shmdt	230

				/* 231 is obsolete shmget35 */
				/* 232 is obsolete t32_clock_gettime */
				/* 233 is obsolete t32_clock_settime */
				/* 234 is obsolete t32_clock_getres */
				/* 240 is obsolete t32_nanosleep */
/* syscall: "minherit" ret: "int" args: "void *" "size_t" "int" */
#define	SYS_minherit	250

				/* 251 is obsolete rfork */
/* syscall: "poll" ret: "int" args: "struct pollfd *" "u_int" "int" */
#define	SYS_poll	252

/* syscall: "issetugid" ret: "int" args: */
#define	SYS_issetugid	253

/* syscall: "lchown" ret: "int" args: "const char *" "uid_t" "gid_t" */
#define	SYS_lchown	254

/* syscall: "getsid" ret: "pid_t" args: "pid_t" */
#define	SYS_getsid	255

/* syscall: "msync" ret: "int" args: "void *" "size_t" "int" */
#define	SYS_msync	256

				/* 257 is obsolete semctl35 */
				/* 258 is obsolete shmctl35 */
				/* 259 is obsolete msgctl35 */
/* syscall: "pipe" ret: "int" args: "int *" */
#define	SYS_pipe	263

/* syscall: "fhopen" ret: "int" args: "const fhandle_t *" "int" */
#define	SYS_fhopen	264

/* syscall: "preadv" ret: "ssize_t" args: "int" "const struct iovec *" "int" "int" "off_t" */
#define	SYS_preadv	267

/* syscall: "pwritev" ret: "ssize_t" args: "int" "const struct iovec *" "int" "int" "off_t" */
#define	SYS_pwritev	268

/* syscall: "kqueue" ret: "int" args: */
#define	SYS_kqueue	269

				/* 270 is obsolete t32_kevent */
/* syscall: "mlockall" ret: "int" args: "int" */
#define	SYS_mlockall	271

/* syscall: "munlockall" ret: "int" args: */
#define	SYS_munlockall	272

/* syscall: "getresuid" ret: "int" args: "uid_t *" "uid_t *" "uid_t *" */
#define	SYS_getresuid	281

/* syscall: "setresuid" ret: "int" args: "uid_t" "uid_t" "uid_t" */
#define	SYS_setresuid	282

/* syscall: "getresgid" ret: "int" args: "gid_t *" "gid_t *" "gid_t *" */
#define	SYS_getresgid	283

/* syscall: "setresgid" ret: "int" args: "gid_t" "gid_t" "gid_t" */
#define	SYS_setresgid	284

				/* 285 is obsolete sys_omquery */
/* syscall: "mquery" ret: "void *" args: "void *" "size_t" "int" "int" "int" "long" "off_t" */
#define	SYS_mquery	286

/* syscall: "closefrom" ret: "int" args: "int" */
#define	SYS_closefrom	287

/* syscall: "sigaltstack" ret: "int" args: "const struct sigaltstack *" "struct sigaltstack *" */
#define	SYS_sigaltstack	288

/* syscall: "shmget" ret: "int" args: "key_t" "size_t" "int" */
#define	SYS_shmget	289

/* syscall: "semop" ret: "int" args: "int" "struct sembuf *" "size_t" */
#define	SYS_semop	290

				/* 291 is obsolete t32_stat */
				/* 292 is obsolete t32_fstat */
				/* 293 is obsolete t32_lstat */
/* syscall: "fhstat" ret: "int" args: "const fhandle_t *" "struct stat *" */
#define	SYS_fhstat	294

/* syscall: "__semctl" ret: "int" args: "int" "int" "int" "union semun *" */
#define	SYS___semctl	295

/* syscall: "shmctl" ret: "int" args: "int" "int" "struct shmid_ds *" */
#define	SYS_shmctl	296

/* syscall: "msgctl" ret: "int" args: "int" "int" "struct msqid_ds *" */
#define	SYS_msgctl	297

/* syscall: "sched_yield" ret: "int" args: */
#define	SYS_sched_yield	298

/* syscall: "getthrid" ret: "pid_t" args: */
#define	SYS_getthrid	299

				/* 300 is obsolete t32___thrsleep */
/* syscall: "__thrwakeup" ret: "int" args: "const volatile void *" "int" */
#define	SYS___thrwakeup	301

/* syscall: "__threxit" ret: "void" args: "pid_t *" */
#define	SYS___threxit	302

/* syscall: "__thrsigdivert" ret: "int" args: "sigset_t" "siginfo_t *" "const struct timespec *" */
#define	SYS___thrsigdivert	303

/* syscall: "__getcwd" ret: "int" args: "char *" "size_t" */
#define	SYS___getcwd	304

/* syscall: "adjfreq" ret: "int" args: "const int64_t *" "int64_t *" */
#define	SYS_adjfreq	305

				/* 306 is obsolete getfsstat53 */
				/* 307 is obsolete statfs53 */
				/* 308 is obsolete fstatfs53 */
				/* 309 is obsolete fhstatfs53 */
/* syscall: "setrtable" ret: "int" args: "int" */
#define	SYS_setrtable	310

/* syscall: "getrtable" ret: "int" args: */
#define	SYS_getrtable	311

				/* 312 is obsolete t32_getdirentries */
/* syscall: "faccessat" ret: "int" args: "int" "const char *" "int" "int" */
#define	SYS_faccessat	313

/* syscall: "fchmodat" ret: "int" args: "int" "const char *" "mode_t" "int" */
#define	SYS_fchmodat	314

/* syscall: "fchownat" ret: "int" args: "int" "const char *" "uid_t" "gid_t" "int" */
#define	SYS_fchownat	315

				/* 316 is obsolete t32_fstatat */
/* syscall: "linkat" ret: "int" args: "int" "const char *" "int" "const char *" "int" */
#define	SYS_linkat	317

/* syscall: "mkdirat" ret: "int" args: "int" "const char *" "mode_t" */
#define	SYS_mkdirat	318

/* syscall: "mkfifoat" ret: "int" args: "int" "const char *" "mode_t" */
#define	SYS_mkfifoat	319

/* syscall: "mknodat" ret: "int" args: "int" "const char *" "mode_t" "dev_t" */
#define	SYS_mknodat	320

/* syscall: "openat" ret: "int" args: "int" "const char *" "int" "..." */
#define	SYS_openat	321

/* syscall: "readlinkat" ret: "ssize_t" args: "int" "const char *" "char *" "size_t" */
#define	SYS_readlinkat	322

/* syscall: "renameat" ret: "int" args: "int" "const char *" "int" "const char *" */
#define	SYS_renameat	323

/* syscall: "symlinkat" ret: "int" args: "const char *" "int" "const char *" */
#define	SYS_symlinkat	324

/* syscall: "unlinkat" ret: "int" args: "int" "const char *" "int" */
#define	SYS_unlinkat	325

				/* 326 is obsolete t32_utimensat */
				/* 327 is obsolete t32_futimens */
				/* 328 is obsolete __tfork51 */
/* syscall: "__set_tcb" ret: "void" args: "void *" */
#define	SYS___set_tcb	329

/* syscall: "__get_tcb" ret: "void *" args: */
#define	SYS___get_tcb	330

#define	SYS_MAXSYSCALL	331
