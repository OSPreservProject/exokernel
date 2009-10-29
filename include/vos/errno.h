
/*
 * Copyright 1999 MIT
 */


#ifndef __VOS_ERRNO_H__
#define __VOS_ERRNO_H__

#ifndef __ASSEMBLER__

#include <errno.h>

extern int errno;
#define RETERR(r)  { errno = r; return -1; }

#endif /* __ASSEMBLER__ */


#define V_NOPERM	EPERM		/* 1  not permitted */
#define V_NOENT		ENOENT		/* 2  no such file or dir */
#define V_NOPROC	ESRCH		/* 3  no such process or env */
#define V_NOENV		ESRCH		/* 3  no such environment */
#define V_INTR		EINTR		/* 4  interrupted */
#define V_IO		EIO		/* 5  generic IO error */
#define V_NXIO		ENXIO		/* 6  device not configured */
#define V_ARG2BIG	E2BIG		/* 7  argument list too big */
#define V_NOEXEC	ENOEXEC		/* 8  cannot exec */
#define V_BADFD		EBADF		/* 9  descriptor corrupted */
#define V_NOCHILD	ECHILD		/* 10 no child processes */
#define V_DEADLK	EDEADLK		/* 11 dead lock avoided */
#define V_NOMEM		ENOMEM		/* 12 no memory */
#define V_ACCESS	EACCESS		/* 13 permission denied */
#define V_FAULT		EFAULT		/* 14 bad address */
#define V_NOTBLK	ENOTBLK		/* 15 block device required */
#define V_BUSY		EBUSY		/* 16 device busy */
#define V_EXIST		EEXIST		/* 17 file exists */
#define V_XDEV		EXDEV		/* 18 cross dev link */
#define V_NODEV		ENODEV		/* 19 not supported by dev */
#define V_NOTDIR	ENOTDIR		/* 20 not directory */
#define V_ISDIR		EISDIR		/* 21 is a directory */
#define V_INVALID	EINVAL		/* 22 invalid argument */
#define V_NFILE		ENFILE		/* 23 too many open files */
#define V_MFILE		EMFILE		/* 24 too many open files */
#define V_NOTTY		ENOTTY		/* 25 bad ioctl for dev */
#define V_TXTBSY	ETXTBSY		/* 26 text file busy */
#define V_FBIG		EFBIG		/* 27 file too big */
#define V_NOSPC		ENOSPC		/* 28 no space on dev */
#define V_SPIPE		ESPIPE		/* 29 illegal seek */
#define V_ROFS		EROFS		/* 30 read-only fs */
#define V_MLINK		EMLINK		/* 31 too many links */
#define V_PIPE		EPIPE		/* 32 broken pipe */
#define V_DOM		EDOM		/* 33 num arg out of domain */
#define V_RANGE		ERANGE		/* 34 result too large */
#define V_AGAIN		EAGAIN		/* 35 try again */
#define V_WOULDBLOCK	35		/* 35 would block */
#define V_INPROGRESS	EINPROGRESS	/* 36 operation in progress */
#define V_ALREADY	EALREADY	/* 37 operation started */
#define V_NOTSOCK	ENOTSOCK	/* 38 not socket */
#define V_DESTADDRREQ 	EDESTADDRREQ 	/* 39 dest addr required */
#define V_MSGSIZE 	EMSGSIZE 	/* 40 message too long */
#define V_PROTOTYPE 	EPROTOTYPE 	/* 41 wrong protocol 4 socket */
#define V_NOPROTOOPT 	ENOPROTOOPT 	/* 42 protocol not available */
#define V_BADPROTOCOL 	EPROTONOSUPPORT /* 43 protocol not supported */
#define V_BADSOCKET 	ESOCKTNOSUPPORT /* 44 bad socket type */
#define V_OPNOTSUPP 	EOPNOTSUPP 	/* 45 operation not supported */
#define V_PFNOSUPPORT 	EPFNOSUPPORT 	/* 46 protocol family not supported */
#define V_ADDRINUSE 	EADDRINUSE 	/* 48 address already in use */
#define V_ADDRNOTAVAIL 	EADDRNOTAVAIL 	/* 49 can't assign requested address */
#define V_NETDOWN 	ENETDOWN 	/* 50 network is down */
#define V_NETUNREACH 	ENETUNREACH 	/* 51 network is unreachable */
#define V_NETRESET 	ENETRESET 	/* 52 dropped connection on reset */
#define V_CONNABORTED 	ECONNABORTED 	/* 53 connection abort */
#define V_CONNRESET 	ECONNRESET 	/* 54 connection reset by peer */
#define V_NOBUFS 	ENOBUFS 	/* 55 no buffer space available */
#define V_ISCONN 	EISCONN 	/* 56 socket is already connected */
#define V_NOTCONN 	ENOTCONN 	/* 57 socket is not connected */
#define V_SHUTDOWN 	ESHUTDOWN 	/* 58 socket already shutdown */
#define V_TOOMANYREFS 	ETOOMANYREFS 	/* 59 too many references */
#define V_TIMEDOUT 	ETIMEDOUT 	/* 60 operation timed out */
#define V_CONNREFUSED 	ECONNREFUSED 	/* 61 connection refused */
#define V_LOOP 		ELOOP 		/* 62 too many symbolic links */
#define V_NAMETOOLONG 	ENAMETOOLONG 	/* 63 file name too long */
#define V_HOSTDOWN 	EHOSTDOWN 	/* 64 host is down */
#define V_HOSTUNREACH 	EHOSTUNREACH 	/* 65 no route to host */
#define V_NOTEMPTY 	ENOTEMPTY 	/* 66 directory not empty */
#define V_PROCLIM 	EPROCLIM 	/* 67 too many processes */
#define V_USERS 	EUSERS 		/* 68 too many users */
#define V_DQUOT 	EDQUOT 		/* 69 disc quota exceeded */
#define V_STALE 	ESTALE 		/* 70 stale NFS file handle */
#define V_REMOTE 	EREMOTE 	/* 71 too much remote in path */
#define V_BADRPC 	EBADRPC 	/* 72 RPC struct is bad */
#define V_RPCMISMATCH 	ERPCMISMATCH 	/* 73 RPC version wrong */
#define V_PROGUNAVAIL 	EPROGUNAVAIL 	/* 74 RPC prog. not avail */
#define V_PROGMISMATCH 	EPROGMISMATCH 	/* 75 program version wrong */
#define V_PROCUNAVAIL 	EPROCUNAVAIL 	/* 76 bad procedure for program */
#define V_NOLCK 	ENOLCK 		/* 77 no locks available */
#define V_NOSYS 	ENOSYS 		/* 78 function not implemented */
#define V_FTYPE 	EFTYPE 		/* 79 bad file type or format */


#define V_UNAVAIL	100		/* service/call unavailable */
#define V_CANTMAP	101		/* can't map memory */
#define V_WAKENUP	102		/* waken up */
#define V_NOFORK  	105		/* fork failed */
#define V_WK_BADSZ	106		/* bad size */
#define V_WK_BADREG	107		/* bad registered terms */
#define V_WK_DLFAILED   108		/* download failed */
#define V_WK_BADCONS   	109		/* bad constructor */
#define V_WK_MTERMS	110		/* too many terms */
#define V_WK_TNOTFOUND	111		/* term not found */
#define V_TOOMANYIPCS	112		/* too many outstanding ipcs */
#define V_SBUF_TOOBIG	113		/* sbuf size over NBPG */
#define V_TOOMANYDPFS	114		/* too many filters for port */
#define V_IPCMSG_FULL	115		/* ipc message ring full */


#endif /* __VOS_ERRNO_H__ */

