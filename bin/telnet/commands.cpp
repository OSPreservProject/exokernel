# 1 "commands.c"
 
 

 





































static char rcsid[] = "$OpenBSD: commands.c,v 1.9 1996/12/22 03:26:08 tholo Exp $";




# 1 "/usr/include/sys/param.h" 1 3
 
 

 



















































# 1 "/usr/include/sys/types.h" 1 3
 
 

 










































 
# 1 "/usr/include/machine/types.h" 1 3
 

 





































# 1 "/usr/include/sys/cdefs.h" 1 3
 
 

 








































 







# 1 "/usr/include/machine/cdefs.h" 1 3
 

 


































# 53 "/usr/include/sys/cdefs.h" 2 3










 






















# 110 "/usr/include/sys/cdefs.h" 3


 






















 






# 41 "/usr/include/machine/types.h" 2 3



typedef struct _physadr {
	int r[1];
} *physadr;

typedef struct label_t {
	int val[6];
} label_t;


typedef	unsigned long	vm_offset_t;
typedef	unsigned long	vm_size_t;

 




typedef	signed  char		   int8_t;
typedef	unsigned char		 u_int8_t;
typedef	short			  int16_t;
typedef	unsigned short		u_int16_t;
typedef	int			  int32_t;
typedef	unsigned int		u_int32_t;
 
typedef	long long		  int64_t;
 
typedef	unsigned long long	u_int64_t;

typedef int32_t			register_t;


# 48 "/usr/include/sys/types.h" 2 3


# 1 "/usr/include/machine/ansi.h" 1 3
 
 

 





































 

















 


















# 50 "/usr/include/sys/types.h" 2 3

# 1 "/usr/include/machine/endian.h" 1 3
 
 

 






































 







 











typedef u_int32_t in_addr_t;
typedef u_int16_t in_port_t;

 
u_int32_t	htonl  (u_int32_t)  ;
u_int16_t	htons  (u_int16_t)  ;
u_int32_t	ntohl  (u_int32_t)  ;
u_int16_t	ntohs  (u_int16_t)  ;
 



























# 116 "/usr/include/machine/endian.h" 3















 










# 51 "/usr/include/sys/types.h" 2 3



typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;

typedef unsigned char	unchar;		 
typedef	unsigned short	ushort;		 
typedef	unsigned int	uint;		 
typedef unsigned long	ulong;		 


typedef	u_int64_t	u_quad_t;	 
typedef	int64_t		quad_t;
typedef	quad_t *	qaddr_t;

typedef	char *		caddr_t;	 
typedef	int32_t		daddr_t;	 
typedef	int32_t		dev_t;		 
typedef	u_int32_t	fixpt_t;	 
typedef	u_int32_t	gid_t;		 
typedef	u_int32_t	ino_t;		 
typedef	long		key_t;		 
typedef	u_int16_t	mode_t;		 
typedef	u_int16_t	nlink_t;	 
typedef	quad_t		off_t;		 
typedef	int32_t		pid_t;		 
typedef quad_t		rlim_t;		 
typedef	int32_t		segsz_t;	 
typedef	int32_t		swblk_t;	 
typedef	u_int32_t	uid_t;		 

 







 
off_t	 lseek  (int, off_t, int)  ;
int	 ftruncate  (int, off_t)  ;
int	 truncate  (const char *, off_t)  ;
 




 






typedef	unsigned long 	clock_t;




typedef	unsigned int 	size_t;




typedef	int 	ssize_t;




typedef	long 	time_t;




typedef	int 	clockid_t;




typedef	int 	timer_t;






 









typedef int32_t	fd_mask;






typedef	struct fd_set {
	fd_mask	fds_bits[((( 256  ) + ((  (sizeof(fd_mask) * 8 )  ) - 1)) / (  (sizeof(fd_mask) * 8 )  )) ];
} fd_set;












# 187 "/usr/include/sys/types.h" 3




# 56 "/usr/include/sys/param.h" 2 3



 






# 1 "/usr/include/sys/syslimits.h" 1 3
 
 

 























































# 66 "/usr/include/sys/param.h" 2 3













 









 
# 1 "/usr/include/sys/signal.h" 1 3
 
 

 











































# 1 "/usr/include/machine/signal.h" 1 3
 

 





































typedef int sig_atomic_t;


 


# 1 "/usr/include/machine/trap.h" 1 3
 

 





































 

























 

# 47 "/usr/include/machine/signal.h" 2 3


 






struct	sigcontext {
	int	sc_gs;
	int	sc_fs;
	int	sc_es;
	int	sc_ds;
	int	sc_edi;
	int	sc_esi;
	int	sc_ebp;
	int	sc_ebx;
	int	sc_edx;
	int	sc_ecx;
	int	sc_eax;
	 
	int	sc_eip;
	int	sc_cs;
	int	sc_eflags;
	int	sc_esp;
	int	sc_ss;

	int	sc_onstack;		 
	int	sc_mask;		 

	int	sc_trapno;		 
	int	sc_err;
};








# 48 "/usr/include/sys/signal.h" 2 3





















































 








typedef unsigned int sigset_t;

 


struct	sigaction {
	void	(*sa_handler)  (int)  ;  
	sigset_t sa_mask;		 
	int	sa_flags;		 
};












 










typedef	void (*sig_t)  (int)  ;	 

 


struct	sigaltstack {
	char	*ss_sp;			 
	int	ss_size;		 
	int	ss_flags;		 
};





 



struct	sigvec {
	void	(*sv_handler)  (int)  ;  
	int	sv_mask;		 
	int	sv_flags;		 
};





 


struct	sigstack {
	char	*ss_sp;			 
	int	ss_onstack;		 
};

 







# 1 "/usr/include/sys/siginfo.h" 1 3
 

 































 
union sigval {
	int	sival_int;	 
	void	*sival_ptr;	 
};
 
 





 







 





































 






 










# 128 "/usr/include/sys/siginfo.h" 3





# 1 "/usr/include/sys/time.h" 1 3
 
 

 







































 



struct timeval {
	long	tv_sec;		 
	long	tv_usec;	 
};

 


struct timespec {
	time_t	tv_sec;		 
	long	tv_nsec;	 
};










struct timezone {
	int	tz_minuteswest;	 
	int	tz_dsttime;	 
};








 







# 98 "/usr/include/sys/time.h" 3

# 107 "/usr/include/sys/time.h" 3

 







# 124 "/usr/include/sys/time.h" 3

# 133 "/usr/include/sys/time.h" 3

 







struct	itimerval {
	struct	timeval it_interval;	 
	struct	timeval it_value;	 
};

 


struct clockinfo {
	int	hz;		 
	int	tick;		 
	int	tickadj;	 
	int	stathz;		 
	int	profhz;		 
};













# 1 "/usr/include/time.h" 1 3
 

 


































































struct tm {
	int	tm_sec;		 
	int	tm_min;		 
	int	tm_hour;	 
	int	tm_mday;	 
	int	tm_mon;		 
	int	tm_year;	 
	int	tm_wday;	 
	int	tm_yday;	 
	int	tm_isdst;	 
	long	tm_gmtoff;	 
	char	*tm_zone;	 
};



 
char *asctime  (const struct tm *)  ;
clock_t clock  (void)  ;
char *ctime  (const time_t *)  ;
double difftime  (time_t, time_t)  ;
struct tm *gmtime  (const time_t *)  ;
struct tm *localtime  (const time_t *)  ;
time_t mktime  (struct tm *)  ;
size_t strftime  (char *, size_t, const char *, const struct tm *)  ;
time_t time  (time_t *)  ;



extern char *tzname[2];
void tzset  (void)  ;



char *timezone  (int, int)  ;
void tzsetwall  (void)  ;

 


# 170 "/usr/include/sys/time.h" 2 3





 
int	adjtime  (const struct timeval *, struct timeval *)  ;
int	futimes  (int, const struct timeval *)  ;
int	getitimer  (int, struct itimerval *)  ;
int	gettimeofday  (struct timeval *, struct timezone *)  ;
int	nanosleep  (struct timespec *, struct timespec *)  ;
int	setitimer  (int, const struct itimerval *, struct itimerval *)  ;
int	settimeofday  (const struct timeval *, const struct timezone *)  ;
int	utimes  (const char *, const struct timeval *)  ;
 





# 133 "/usr/include/sys/siginfo.h" 2 3


typedef struct {
	int	si_signo;			 
	int	si_code;			 
	int	si_errno;			 
	union {
		int	_pad[((128  / sizeof (int)) - 3) ];		 
		struct {			 
			pid_t	_pid;		 
			union {
				struct {
					uid_t	_uid;
					union sigval	_value;
				} _kill;
				struct {
					clock_t	_utime;
					int	_status;
					clock_t	_stime;
				} _cld;
			} _pdata;
		} _proc;
		struct {	 
			caddr_t	_addr;		 
			int	_trapno;	 
		} _fault;
# 174 "/usr/include/sys/siginfo.h" 3

	} _data;
} siginfo_t;


























# 188 "/usr/include/sys/signal.h" 2 3





 



 
void	(*signal  (int, void (*)  (int)  )  )  (int)  ;
 

# 90 "/usr/include/sys/param.h" 2 3


 
# 1 "/usr/include/machine/param.h" 1 3
 
 

 





































 

















 



























 





 



















 

 




 



 



 



 







 










# 93 "/usr/include/sys/param.h" 2 3

# 1 "/usr/include/machine/limits.h" 1 3
 

 

















































































# 94 "/usr/include/sys/param.h" 2 3


 


























 






















				 



 











 











 





 






 





 

















 













 














# 47 "commands.c" 2




# 1 "/usr/include/sys/file.h" 1 3
 
 

 


































# 1 "/usr/include/sys/fcntl.h" 1 3
 
 

 










































 









 






 





 




























 


# 111 "/usr/include/sys/fcntl.h" 3


 













 



 













 


 









 



struct flock {
	off_t	l_start;	 
	off_t	l_len;		 
	pid_t	l_pid;		 
	short	l_type;		 
	short	l_whence;	 
};



 










 
int	open  (const char *, int, ...)  ;
int	creat  (const char *, mode_t)  ;
int	fcntl  (int, int, ...)  ;

int	flock  (int, int)  ;

 



# 39 "/usr/include/sys/file.h" 2 3

# 1 "/usr/include/sys/unistd.h" 1 3
 
 

 





































 







 
				 

				 

				 


 





 





 





 










 




























 



# 40 "/usr/include/sys/file.h" 2 3


# 82 "/usr/include/sys/file.h" 3

# 51 "commands.c" 2




# 1 "/usr/include/sys/socket.h" 1 3
 
 

 





































 



 








 













 











 


struct	linger {
	int	l_onoff;		 
	int	l_linger;		 
};

 




 






































 



struct sockaddr {
	u_char	sa_len;			 
	u_char	sa_family;		 
	char	sa_data[14];		 
};

 



struct sockproto {
	u_short	sp_family;		 
	u_short	sp_protocol;		 
};

 





































 










# 231 "/usr/include/sys/socket.h" 3

 



















 




 



struct msghdr {
	caddr_t	msg_name;		 
	u_int	msg_namelen;		 
	struct	iovec *msg_iov;		 
	u_int	msg_iovlen;		 
	caddr_t	msg_control;		 
	u_int	msg_controllen;		 
	int	msg_flags;		 
};










 





struct cmsghdr {
	u_int	cmsg_len;		 
	int	cmsg_level;		 
	int	cmsg_type;		 
 
};

 


 








 


 


struct osockaddr {
	u_short	sa_family;		 
	char	sa_data[14];		 
};

 


struct omsghdr {
	caddr_t	msg_name;		 
	int	msg_namelen;		 
	struct	iovec *msg_iov;		 
	int	msg_iovlen;		 
	caddr_t	msg_accrights;		 
	int	msg_accrightslen;
};





 
int	accept  (int, struct sockaddr *, int *)  ;
int	bind  (int, const struct sockaddr *, int)  ;
int	connect  (int, const struct sockaddr *, int)  ;
int	getpeername  (int, struct sockaddr *, int *)  ;
int	getsockname  (int, struct sockaddr *, int *)  ;
int	getsockopt  (int, int, int, void *, int *)  ;
int	listen  (int, int)  ;
ssize_t	recv  (int, void *, size_t, int)  ;
ssize_t	recvfrom  (int, void *, size_t, int, struct sockaddr *, int *)  ;
ssize_t	recvmsg  (int, struct msghdr *, int)  ;
ssize_t	send  (int, const void *, size_t, int)  ;
ssize_t	sendto  (int, const void *,
	    size_t, int, const struct sockaddr *, int)  ;
ssize_t	sendmsg  (int, const struct msghdr *, int)  ;
int	setsockopt  (int, int, int, const void *, int)  ;
int	shutdown  (int, int)  ;
int	socket  (int, int, int)  ;
int	socketpair  (int, int, int, int *)  ;
 









# 55 "commands.c" 2

# 1 "/usr/include/netinet/in.h" 1 3
 
 

 


































 







 






















 



































 








 





 


struct in_addr {
	u_int32_t s_addr;
};

 




































 


























 


struct sockaddr_in {
	u_int8_t  sin_len;
	u_int8_t  sin_family;
	u_int16_t sin_port;
	struct	  in_addr sin_addr;
	int8_t	  sin_zero[8];
};

 






struct ip_opts {
	struct in_addr	ip_dst;		 
	int8_t		ip_opts[40];	 
};

 
















	 






 
















 






 


struct ip_mreq {
	struct	in_addr imr_multiaddr;	 
	struct	in_addr imr_interface;	 
};

 







 








# 312 "/usr/include/netinet/in.h" 3

 

















# 343 "/usr/include/netinet/in.h" 3


# 356 "/usr/include/netinet/in.h" 3


# 56 "commands.c" 2





# 1 "/usr/include/signal.h" 1 3
 
 

 












































extern const  char * const  sys_signame[32 ];
extern const  char * const  sys_siglist[32 ];


 
int	raise  (int)  ;

int	kill  (pid_t, int)  ;
int	sigaction  (int, const struct sigaction *, struct sigaction *)  ;
int	sigaddset  (sigset_t *, int)  ;
int	sigdelset  (sigset_t *, int)  ;
int	sigemptyset  (sigset_t *)  ;
int	sigfillset  (sigset_t *)  ;
int	sigismember  (const sigset_t *, int)  ;
int	sigpending  (sigset_t *)  ;
int	sigprocmask  (int, const sigset_t *, sigset_t *)  ;
int	sigsuspend  (const sigset_t *)  ;


extern __inline int sigaddset(sigset_t *set, int signo) {
	extern int errno;

	if (signo <= 0 || signo >= 32 ) {
		errno = 22;			 
		return -1;
	}
	*set |= (1 << ((signo)-1));		 
	return (0);
}

extern __inline int sigdelset(sigset_t *set, int signo) {
	extern int errno;

	if (signo <= 0 || signo >= 32 ) {
		errno = 22;			 
		return -1;
	}
	*set &= ~(1 << ((signo)-1));		 
	return (0);
}

extern __inline int sigismember(const sigset_t *set, int signo) {
	extern int errno;

	if (signo <= 0 || signo >= 32 ) {
		errno = 22;			 
		return -1;
	}
	return ((*set & (1 << ((signo)-1))) != 0);
}


 




int	killpg  (pid_t, int)  ;
int	sigblock  (int)  ;
int	siginterrupt  (int, int)  ;
int	sigpause  (int)  ;
int	sigreturn  (struct sigcontext *)  ;
int	sigsetmask  (int)  ;
int	sigstack  (const struct sigstack *, struct sigstack *)  ;
int	sigaltstack  (const struct sigaltstack *, struct sigaltstack *)  ;
int	sigvec  (int, struct sigvec *, struct sigvec *)  ;
void	psignal  (unsigned int, const char *)  ;


 


# 61 "commands.c" 2

# 1 "/usr/include/netdb.h" 1 3
 

 






















































 







# 1 "/usr/include/sys/param.h" 1 3
 
 

 






















































 






# 1 "/usr/include/sys/syslimits.h" 1 3
 
 

 























































# 66 "/usr/include/sys/param.h" 2 3













 









 


 
# 1 "/usr/include/machine/param.h" 1 3
 
 

 





































 

















 



























 





 



















 

 




 



 



 



 







 










# 93 "/usr/include/sys/param.h" 2 3

# 1 "/usr/include/machine/limits.h" 1 3
 

 

















































































# 94 "/usr/include/sys/param.h" 2 3


 


























 






















				 



 











 











 





 






 





 

















 













 














# 66 "/usr/include/netdb.h" 2 3












extern int h_errno;

 




struct	hostent {
	char	*h_name;	 
	char	**h_aliases;	 
	int	h_addrtype;	 
	int	h_length;	 
	char	**h_addr_list;	 

};

 



struct	netent {
	char		*n_name;	 
	char		**n_aliases;	 
	int		n_addrtype;	 
	in_addr_t	n_net;		 
};

struct	servent {
	char	*s_name;	 
	char	**s_aliases;	 
	int	s_port;		 
	char	*s_proto;	 
};

struct	protoent {
	char	*p_name;	 
	char	**p_aliases;	 
	int	p_proto;	 
};

 












 
void		endhostent  (void)  ;
void		endnetent  (void)  ;
void		endprotoent  (void)  ;
void		endservent  (void)  ;
struct hostent	*gethostbyaddr  (const char *, int, int)  ;
struct hostent	*gethostbyname  (const char *)  ;
struct hostent	*gethostbyname2  (const char *, int)  ;
struct hostent	*gethostent  (void)  ;
struct netent	*getnetbyaddr  (in_addr_t, int)  ;
struct netent	*getnetbyname  (const char *)  ;
struct netent	*getnetent  (void)  ;
struct protoent	*getprotobyname  (const char *)  ;
struct protoent	*getprotobynumber  (int)  ;
struct protoent	*getprotoent  (void)  ;
struct servent	*getservbyname  (const char *, const char *)  ;
struct servent	*getservbyport  (int, const char *)  ;
struct servent	*getservent  (void)  ;
void		herror  (const char *)  ;
const char	*hstrerror  (int)  ;
void		sethostent  (int)  ;
 
void		setnetent  (int)  ;
void		setprotoent  (int)  ;
void		setservent  (int)  ;
 

 
# 170 "/usr/include/netdb.h" 3



# 62 "commands.c" 2

# 1 "/usr/include/ctype.h" 1 3
 

 




















































extern const char	*_ctype_;
extern const short	*_tolower_tab_;
extern const short	*_toupper_tab_;

 
extern int	isalnum  (int)  ;
extern int	isalpha  (int)  ;
extern int	iscntrl  (int)  ;
extern int	isdigit  (int)  ;
extern int	isgraph  (int)  ;
extern int	islower  (int)  ;
extern int	isprint  (int)  ;
extern int	ispunct  (int)  ;
extern int	isspace  (int)  ;
extern int	isupper  (int)  ;
extern int	isxdigit  (int)  ;
extern int	tolower  (int)  ;
extern int	toupper  (int)  ;


extern int	isblank  (int)  ;
extern int	isascii  (int)  ;
extern int	toascii  (int)  ;
extern int	_tolower  (int)  ;
extern int	_toupper  (int)  ;

 


























# 63 "commands.c" 2

# 1 "/usr/include/pwd.h" 1 3
 
 

 









































































struct passwd {
	char	*pw_name;		 
	char	*pw_passwd;		 
	int	pw_uid;			 
	int	pw_gid;			 
	time_t	pw_change;		 
	char	*pw_class;		 
	char	*pw_gecos;		 
	char	*pw_dir;		 
	char	*pw_shell;		 
	time_t	pw_expire;		 
};



 
struct passwd	*getpwuid  (uid_t)  ;
struct passwd	*getpwnam  (const char *)  ;

struct passwd	*getpwent  (void)  ;

int		 setpassent  (int)  ;
char		*user_from_uid  (uid_t, int)  ;

void		 setpwent  (void)  ;
void		 endpwent  (void)  ;

 


# 64 "commands.c" 2

# 1 "/usr/include/varargs.h" 1 3
 

 










































# 1 "/usr/include/machine/stdarg.h" 1 3
 

 







































typedef char * 	va_list;













# 46 "/usr/include/varargs.h" 2 3
















# 65 "commands.c" 2

# 1 "/usr/include/errno.h" 1 3
 
 

 








































extern int errno;			 




extern int sys_nerr;
extern char *sys_errlist[];














					 


























 



 






 













 

















 






 





 
























# 66 "commands.c" 2


# 1 "/usr/include/arpa/telnet.h" 1 3
 

 





































 

































extern char *telcmds[];








 












































# 147 "/usr/include/arpa/telnet.h" 3


 











 















 







































 
















extern char *slc_names[];



























 



 






 





















extern char *authtype_names[];





 


















# 330 "/usr/include/arpa/telnet.h" 3

extern char *encrypt_names[];
extern char *enctype_names[];










# 68 "commands.c" 2




# 1 "general.h" 1
 
 

 


































 









# 72 "commands.c" 2


# 1 "ring.h" 1
 
 

 





































 









typedef struct {
    unsigned char	*consume,	 
			*supply,	 
			*bottom,	 
			*top,		 
			*mark;		 
    int		size;		 
    u_long	consumetime,	 
		supplytime;
} Ring;

 

 
extern int
	ring_init  (Ring *ring, unsigned char *buffer, int count)  ;

 
extern void
	ring_supply_data  (Ring *ring, unsigned char *buffer, int count)  ;





 
extern void
	ring_supplied  (Ring *ring, int count)  ,
	ring_consumed  (Ring *ring, int count)  ;

 
extern int
	ring_empty_count  (Ring *ring)  ,
	ring_empty_consecutive  (Ring *ring)  ,
	ring_full_count  (Ring *ring)  ,
	ring_full_consecutive  (Ring *ring)  ;


extern void
    ring_clear_mark(),
    ring_mark();
# 74 "commands.c" 2


# 1 "externs.h" 1
 
 

 






































 












# 1 "/usr/include/stdio.h" 1 3
 
 

 
























































 





typedef off_t fpos_t;









 





 
struct __sbuf {
	unsigned char *_base;
	int	_size;
};

 























typedef	struct __sFILE {
	unsigned char *_p;	 
	int	_r;		 
	int	_w;		 
	short	_flags;		 
	short	_file;		 
	struct	__sbuf _bf;	 
	int	_lbfsize;	 

	 
	void	*_cookie;	 
	int	(*_close)  (void *)  ;
	int	(*_read)   (void *, char *, int)  ;
	fpos_t	(*_seek)   (void *, fpos_t, int)  ;
	int	(*_write)  (void *, const char *, int)  ;

	 
	struct	__sbuf _ub;	 
	unsigned char *_up;	 
	int	_ur;		 

	 
	unsigned char _ubuf[3];	 
	unsigned char _nbuf[1];	 

	 
	struct	__sbuf _lb;	 

	 
	int	_blksize;	 
	fpos_t	_offset;	 
} FILE;

 
extern FILE __sF[];
 





	 











 















 







 




















 


 
void	 clearerr  (FILE *)  ;
int	 fclose  (FILE *)  ;
int	 feof  (FILE *)  ;
int	 ferror  (FILE *)  ;
int	 fflush  (FILE *)  ;
int	 fgetc  (FILE *)  ;
int	 fgetpos  (FILE *, fpos_t *)  ;
char	*fgets  (char *, int, FILE *)  ;
FILE	*fopen  (const char *, const char *)  ;
int	 fprintf  (FILE *, const char *, ...)  ;
int	 fputc  (int, FILE *)  ;
int	 fputs  (const char *, FILE *)  ;
size_t	 fread  (void *, size_t, size_t, FILE *)  ;
FILE	*freopen  (const char *, const char *, FILE *)  ;
int	 fscanf  (FILE *, const char *, ...)  ;
int	 fseek  (FILE *, long, int)  ;
int	 fsetpos  (FILE *, const fpos_t *)  ;
long	 ftell  (FILE *)  ;
size_t	 fwrite  (const void *, size_t, size_t, FILE *)  ;
int	 getc  (FILE *)  ;
int	 getchar  (void)  ;
char	*gets  (char *)  ;






void	 perror  (const char *)  ;
int	 printf  (const char *, ...)  ;
int	 putc  (int, FILE *)  ;
int	 putchar  (int)  ;
int	 puts  (const char *)  ;
int	 remove  (const char *)  ;
int	 rename   (const char *, const char *)  ;
void	 rewind  (FILE *)  ;
int	 scanf  (const char *, ...)  ;
void	 setbuf  (FILE *, char *)  ;
int	 setvbuf  (FILE *, char *, int, size_t)  ;
int	 sprintf  (char *, const char *, ...)  ;
int	 sscanf  (const char *, const char *, ...)  ;
FILE	*tmpfile  (void)  ;
char	*tmpnam  (char *)  ;
int	 ungetc  (int, FILE *)  ;
int	 vfprintf  (FILE *, const char *, char * )  ;
int	 vprintf  (const char *, char * )  ;
int	 vsprintf  (char *, const char *, char * )  ;
 

 






 
char	*ctermid  (char *)  ;
char	*cuserid  (char *)  ;
FILE	*fdopen  (int, const char *)  ;
int	 fileno  (FILE *)  ;
 


 



 
char	*fgetln  (FILE *, size_t *)  ;
int	 fpurge  (FILE *)  ;
int	 getw  (FILE *)  ;
int	 pclose  (FILE *)  ;
FILE	*popen  (const char *, const char *)  ;
int	 putw  (int, FILE *)  ;
void	 setbuffer  (FILE *, char *, int)  ;
int	 setlinebuf  (FILE *)  ;
char	*tempnam  (const char *, const char *)  ;
int	 snprintf  (char *, size_t, const char *, ...)  
		__attribute__((format (printf, 3, 4)));
int	 vsnprintf  (char *, size_t, const char *, char * )  
		__attribute__((format (printf, 3, 0)));
int	 vscanf  (const char *, char * )  
		__attribute__((format (scanf, 1, 0)));
int	 vsscanf  (const char *, const char *, char * )  
		__attribute__((format (scanf, 2, 0)));
 

 






 


 
FILE	*funopen  (const void *,
		int (*)(void *, char *, int),
		int (*)(void *, const char *, int),
		fpos_t (*)(void *, fpos_t, int),
		int (*)(void *))  ;
 




 


 
int	__srget  (FILE *)  ;
int	__svfscanf  (FILE *, const char *, char * )  ;
int	__swbuf  (int, FILE *)  ;
 

 





static __inline int __sputc(int _c, FILE *_p) {
	if (--_p->_w >= 0 || (_p->_w >= _p->_lbfsize && (char)_c != '\n'))
		return (*_p->_p++ = _c);
	else
		return (__swbuf(_c, _p));
}
# 357 "/usr/include/stdio.h" 3

















 











# 56 "externs.h" 2

# 1 "/usr/include/stdlib.h" 1 3
 

 
















































typedef	int 	wchar_t;



typedef struct {
	int quot;		 
	int rem;		 
} div_t;

typedef struct {
	long quot;		 
	long rem;		 
} ldiv_t;


typedef struct {
	quad_t quot;		 
	quad_t rem;		 
} qdiv_t;
















 
  void	 abort  (void)  ;
int	 abs  (int)  ;
int	 atexit  (void (*)(void))  ;
double	 atof  (const char *)  ;
int	 atoi  (const char *)  ;
long	 atol  (const char *)  ;
void	*bsearch  (const void *, const void *, size_t,
	    size_t, int (*)(const void *, const void *))  ;
void	*calloc  (size_t, size_t)  ;
div_t	 div  (int, int)  ;
  void	 exit  (int)  ;
void	 free  (void *)  ;
char	*getenv  (const char *)  ;
long	 labs  (long)  ;
ldiv_t	 ldiv  (long, long)  ;
void	*malloc  (size_t)  ;
void	 qsort  (void *, size_t, size_t,
	    int (*)(const void *, const void *))  ;
int	 rand  (void)  ;
void	*realloc  (void *, size_t)  ;
void	 srand  (unsigned)  ;
double	 strtod  (const char *, char **)  ;
long	 strtol  (const char *, char **, int)  ;
unsigned long
	 strtoul  (const char *, char **, int)  ;
int	 system  (const char *)  ;

 
int	 mblen  (const char *, size_t)  ;
size_t	 mbstowcs  (wchar_t *, const char *, size_t)  ;
int	 wctomb  (char *, wchar_t)  ;
int	 mbtowc  (wchar_t *, const char *, size_t)  ;
size_t	 wcstombs  (char *, const wchar_t *, size_t)  ;





void  *alloca  (size_t)  ; 


char	*getbsize  (int *, long *)  ;
char	*cgetcap  (char *, char *, int)  ;
int	 cgetclose  (void)  ;
int	 cgetent  (char **, char **, char *)  ;
int	 cgetfirst  (char **, char **)  ;
int	 cgetmatch  (char *, char *)  ;
int	 cgetnext  (char **, char **)  ;
int	 cgetnum  (char *, char *, long *)  ;
int	 cgetset  (char *)  ;
int	 cgetstr  (char *, char *, char **)  ;
int	 cgetustr  (char *, char *, char **)  ;

int	 daemon  (int, int)  ;
char	*devname  (int, int)  ;
int	 getloadavg  (double [], int)  ;

long	 a64l  (const char *)  ;
char	*l64a  (long)  ;

void	 cfree  (void *)  ;

int	 getopt  (int, char * const *, const char *)  ;
extern	 char *optarg;			 
extern	 int opterr;
extern	 int optind;
extern	 int optopt;
extern	 int optreset;
int	 getsubopt  (char **, char * const *, char **)  ;
extern	 char *suboptarg;		 

int	 heapsort  (void *, size_t, size_t,
	    int (*)(const void *, const void *))  ;
int	 mergesort  (void *, size_t, size_t,
	    int (*)(const void *, const void *))  ;
int	 radixsort  (const unsigned char **, int, const unsigned char *,
	    unsigned)  ;
int	 sradixsort  (const unsigned char **, int, const unsigned char *,
	    unsigned)  ;

char	*initstate  (unsigned, char *, int)  ;
long	 random  (void)  ;
char	*realpath  (const char *, char *)  ;
char	*setstate  (char *)  ;
void	 srandom  (unsigned)  ;

int	 putenv  (const char *)  ;
int	 setenv  (const char *, const char *, int)  ;
void	 unsetenv  (const char *)  ;
void	 setproctitle  (const char *, ...)  ;

quad_t	 qabs  (quad_t)  ;
qdiv_t	 qdiv  (quad_t, quad_t)  ;
quad_t	 strtoq  (const char *, char **, int)  ;
u_quad_t strtouq  (const char *, char **, int)  ;

double	 drand48  (void)  ;
double	 erand48  (unsigned short[3])  ;
long	 jrand48  (unsigned short[3])  ;
void	 lcong48  (unsigned short[7])  ;
long	 lrand48  (void)  ;
long	 mrand48  (void)  ;
long	 nrand48  (unsigned short[3])  ;
unsigned short *seed48  (unsigned short[3])  ;
void	 srand48  (long)  ;

u_int32_t arc4random  (void)  ;
void	arc4random_stir  (void)  ;
void	arc4random_addrandom  (u_char *, int)  ;


 


# 57 "externs.h" 2

# 1 "/usr/include/setjmp.h" 1 3
 

 










































# 1 "/usr/include/machine/setjmp.h" 1 3
 

 




# 46 "/usr/include/setjmp.h" 2 3



typedef long sigjmp_buf[10  + 1];


typedef long jmp_buf[10 ];



 
int	setjmp  (jmp_buf)  ;
void	longjmp  (jmp_buf, int)  ;


int	sigsetjmp  (sigjmp_buf, int)  ;
void	siglongjmp  (sigjmp_buf, int)  ;



int	_setjmp  (jmp_buf)  ;
void	_longjmp  (jmp_buf, int)  ;
void	longjmperror  (void)  ;

 


# 58 "externs.h" 2





# 1 "/usr/include/sys/ioctl.h" 1 3
 
 

 










































# 1 "/usr/include/sys/ttycom.h" 1 3
 
 

 










































# 1 "/usr/include/sys/ioccom.h" 1 3
 
 

 





































 










				 

				 

				 

				 

				 







 



# 47 "/usr/include/sys/ttycom.h" 2 3


 




 



struct winsize {
	unsigned short	ws_row;		 
	unsigned short	ws_col;		 
	unsigned short	ws_xpixel;	 
	unsigned short	ws_ypixel;	 
};












						 


						 

						 






						 






						 





































 










# 47 "/usr/include/sys/ioctl.h" 2 3


 




struct ttysize {
	unsigned short	ts_lines;
	unsigned short	ts_cols;
	unsigned short	ts_xxx;
	unsigned short	ts_yyy;
};





# 1 "/usr/include/sys/dkio.h" 1 3
 
 

 





































 


		 





 












# 65 "/usr/include/sys/ioctl.h" 2 3

# 1 "/usr/include/sys/filio.h" 1 3
 
 

 












































 









# 66 "/usr/include/sys/ioctl.h" 2 3

# 1 "/usr/include/sys/sockio.h" 1 3
 
 

 







































 











































# 67 "/usr/include/sys/ioctl.h" 2 3






 
int	ioctl  (int, unsigned long, ...)  ;
 



 










# 63 "externs.h" 2












# 1 "/usr/include/sys/termios.h" 1 3
 
 

 





































 



















 
















 









 



















 













 

























 


































typedef unsigned int	tcflag_t;
typedef unsigned char	cc_t;
typedef unsigned int	speed_t;

struct termios {
	tcflag_t	c_iflag;	 
	tcflag_t	c_oflag;	 
	tcflag_t	c_cflag;	 
	tcflag_t	c_lflag;	 
	cc_t		c_cc[20 ];	 
	int		c_ispeed;	 
	int		c_ospeed;	 
};

 









 










































 
speed_t	cfgetispeed  (const struct termios *)  ;
speed_t	cfgetospeed  (const struct termios *)  ;
int	cfsetispeed  (struct termios *, speed_t)  ;
int	cfsetospeed  (struct termios *, speed_t)  ;
int	tcgetattr  (int, struct termios *)  ;
int	tcsetattr  (int, int, const struct termios *)  ;
int	tcdrain  (int)  ;
int	tcflow  (int, int)  ;
int	tcflush  (int, int)  ;
int	tcsendbreak  (int, int)  ;


void	cfmakeraw  (struct termios *)  ;
int	cfsetspeed  (struct termios *, speed_t)  ;

 





 







 





# 1 "/usr/include/sys/ttydefaults.h" 1 3
 
 

 







































 





 








 





















 




 


 










# 287 "/usr/include/sys/termios.h" 2 3


# 75 "externs.h" 2














# 1 "/usr/include/string.h" 1 3
 

 

















































 
void	*memchr  (const void *, int, size_t)  ;
int	 memcmp  (const void *, const void *, size_t)  ;
void	*memcpy  (void *, const void *, size_t)  ;
void	*memmove  (void *, const void *, size_t)  ;
void	*memset  (void *, int, size_t)  ;
char	*strcat  (char *, const char *)  ;
char	*strchr  (const char *, int)  ;
int	 strcmp  (const char *, const char *)  ;
int	 strcoll  (const char *, const char *)  ;
char	*strcpy  (char *, const char *)  ;
size_t	 strcspn  (const char *, const char *)  ;
char	*strerror  (int)  ;
size_t	 strlen  (const char *)  ;
char	*strncat  (char *, const char *, size_t)  ;
int	 strncmp  (const char *, const char *, size_t)  ;
char	*strncpy  (char *, const char *, size_t)  ;
char	*strpbrk  (const char *, const char *)  ;
char	*strrchr  (const char *, int)  ;
size_t	 strspn  (const char *, const char *)  ;
char	*strstr  (const char *, const char *)  ;
char	*strtok  (char *, const char *)  ;
size_t	 strxfrm  (char *, const char *, size_t)  ;

 

int	 bcmp  (const void *, const void *, size_t)  ;
void	 bcopy  (const void *, void *, size_t)  ;
void	 bzero  (void *, size_t)  ;
int	 ffs  (int)  ;
char	*index  (const char *, int)  ;
void	*memccpy  (void *, const void *, int, size_t)  ;
char	*rindex  (const char *, int)  ;
int	 strcasecmp  (const char *, const char *)  ;
char	*strdup  (const char *)  ;
void	 strmode  (int, char *)  ;
int	 strncasecmp  (const char *, const char *, size_t)  ;
char	*strsep  (char **, const char *)  ;
char	*strsignal  (int)  ;
void	 swab  (const void *, void *, size_t)  ;

 


# 89 "externs.h" 2





# 103 "externs.h"





extern int errno;		 





extern int
    autologin,		 
    skiprc,		 
    eight,		 
    flushout,		 
    connected,		 
    globalmode,		 
    In3270,		 
    telnetport,		 
    localflow,		 
    restartany,		 
    localchars,		 
    donelclchars,	 
    showoptions,
    net,		 
    tin,		 
    tout,		 
    crlf,		 
    autoflush,		 
    autosynch,		 
    SYNCHing,		 
    donebinarytoggle,	 
    dontlecho,		 
    crmod,
    netdata,		 
    prettydump,		 





    termdata,		 

    debug,		 
    clienteof;		 

extern cc_t escape;	 
extern cc_t rlogin;	 

extern cc_t echoc;	 


extern char
    *prompt;		 

extern char
    doopt[],
    dont[],
    will[],
    wont[],
    options[],		 
    *hostname;		 

 








 























 





























extern FILE
    *NetTrace;		 
extern unsigned char
    NetTraceFile[];	 
extern void
    SetNetTrace  (char *)  ;	 

extern jmp_buf
    peerdied,
    toplevel;		 

extern void
    command  (int, char *, int)  ,
    Dump  (int, unsigned char *, int)  ,
    init_3270  (void)  ,
    printoption  (char *, int, int)  ,
    printsub  (int, unsigned char *, int)  ,
    sendnaws  (void)  ,
    setconnmode  (int)  ,
    setcommandmode  (void)  ,
    setneturg  (void)  ,
    sys_telnet_init  (void)  ,
    telnet  (char *)  ,
    tel_enter_binary  (int)  ,
    TerminalFlushOutput  (void)  ,
    TerminalNewMode  (int)  ,
    TerminalRestoreState  (void)  ,
    TerminalSaveState  (void)  ,
    tninit  (void)  ,
    upcase  (char *)  ,
    willoption  (int)  ,
    wontoption  (int)  ;

extern void
    send_do  (int, int)  ,
    send_dont  (int, int)  ,
    send_will  (int, int)  ,
    send_wont  (int, int)  ;

extern void
    lm_will  (unsigned char *, int)  ,
    lm_wont  (unsigned char *, int)  ,
    lm_do  (unsigned char *, int)  ,
    lm_dont  (unsigned char *, int)  ,
    lm_mode  (unsigned char *, int, int)  ;

extern void
    slc_init  (void)  ,
    slcstate  (void)  ,
    slc_mode_export  (void)  ,
    slc_mode_import  (int)  ,
    slc_import  (int)  ,
    slc_export  (void)  ,
    slc  (unsigned char *, int)  ,
    slc_check  (void)  ,
    slc_start_reply  (void)  ,
    slc_add_reply  (int, int, int)  ,
    slc_end_reply  (void)  ;
extern int
    slc_update  (void)  ;

extern void
    env_opt  (unsigned char *, int)  ,
    env_opt_start  (void)  ,
    env_opt_start_info  (void)  ,
    env_opt_add  (unsigned char *)  ,
    env_opt_end  (int)  ;

extern unsigned char
    *env_default  (int, int)  ,
    *env_getvalue  (unsigned char *)  ;

extern int
    get_status  (void)  ,
    dosynch  (void)  ;

extern cc_t
    *tcval  (int)  ;

# 347 "externs.h"


extern struct	termios  new_tc;













































































# 444 "externs.h"




 

extern Ring
    netoring,
    netiring,
    ttyoring,
    ttyiring;

 
# 478 "externs.h"

# 76 "commands.c" 2

# 1 "defines.h" 1
 
 

 






















































 





# 77 "commands.c" 2

# 1 "types.h" 1
 
 

 


































typedef struct {
    char *modedescriptions;
    char modetype;
} Modelist;

extern Modelist modelist[];

typedef struct {
    int
	system,			 
	echotoggle,		 
	modenegotiated,		 
	didnetreceive,		 
	gotDM;			 
} Clocks;

extern Clocks clocks;
# 78 "commands.c" 2



# 1 "/usr/include/netinet/in_systm.h" 1 3
 
 

 


































 




 







typedef u_int16_t n_short;		 
typedef u_int32_t n_long;		 

typedef u_int32_t n_time;		 




# 81 "commands.c" 2





# 1 "/usr/include/netinet/ip.h" 1 3
 
 

 


































 





 


struct ip {

	u_int8_t  ip_hl:4,		 
		  ip_v:4;		 





	u_int8_t  ip_tos;		 
	u_int16_t ip_len;		 
	u_int16_t ip_id;		 
	u_int16_t ip_off;		 




	u_int8_t  ip_ttl;		 
	u_int8_t  ip_p;			 
	u_int16_t ip_sum;		 
	struct	  in_addr ip_src, ip_dst;  
};



 





 

 











 





















 







 


struct	ip_timestamp {
	u_int8_t ipt_code;		 
	u_int8_t ipt_len;		 
	u_int8_t ipt_ptr;		 

	u_int8_t ipt_flg:4,		 
		 ipt_oflw:4;		 





	union ipt_timestamp {
		 n_time	ipt_time[1];
		 struct	ipt_ta {
			struct in_addr ipt_addr;
			n_time ipt_time;
		 } ipt_ta[1];
	} ipt_timestamp;
};

 




 








 








# 86 "commands.c" 2








int tos = -1;


char	*hostname;
static char _hostname[256 ];

extern char *getenv();

extern int isprefix();
extern char **genget();
extern int Ambiguous();

static call();

typedef struct {
	char	*name;		 
	char	*help;		 
	int	(*handler)();	 
	int	needconnect;	 
} Command;

static char line[256];
static char saveline[256];
static int margc;
static char *margv[20];


# 1 "/usr/include/sys/wait.h" 1 3
 
 

 





































 




 

























 












 

 







 





union wait {
	int	w_status;		 
	 


	struct {

		unsigned int	w_Termsig:7,	 
				w_Coredump:1,	 
				w_Retcode:8,	 
				w_Filler:16;	 







	} w_T;
	 




	struct {

		unsigned int	w_Stopval:8,	 
				w_Stopsig:8,	 
				w_Filler:16;	 






	} w_S;
};













 
struct rusage;	 

pid_t	wait  (int *)  ;
pid_t	waitpid  (pid_t, int *, int)  ;

pid_t	wait3  (int *, int, struct rusage *)  ;
pid_t	wait4  (pid_t, int *, int, struct rusage *)  ;

 



# 121 "commands.c" 2


    int
skey_calc(argc, argv)
	int argc;
	char **argv;
{
	int status;

	if(argc != 3) {
		printf("%s sequence challenge\n", argv[0]);
		return;
	}

	switch(fork()) {
	case 0:
		execv("/usr/bin/skey" , argv);
		exit (1);
	case -1:
		perror("fork");
		break;
	default:
		(void) wait(&status);
		if ((((*(int *)&(   status   ))  & 0177)  == 0) )
			return (((*(int *)&(  status  ))  >> 8) );
		return (0);
	}
}

   	


    static void
makeargv()
{
    register char *cp, *cp2, c;
    register char **argp = margv;

    margc = 0;
    cp = line;
    if (*cp == '!') {		 
	strcpy(saveline, line);	 
	*argp++ = "!";		 
	margc++;
	cp++;
    }
    while (c = *cp) {
	register int inquote = 0;
	while (((_ctype_ + 1)[ c ] & 0x08 ) )
	    c = *++cp;
	if (c == '\0')
	    break;
	*argp++ = cp;
	margc += 1;
	for (cp2 = cp; c != '\0'; c = *++cp) {
	    if (inquote) {
		if (c == inquote) {
		    inquote = 0;
		    continue;
		}
	    } else {
		if (c == '\\') {
		    if ((c = *++cp) == '\0')
			break;
		} else if (c == '"') {
		    inquote = '"';
		    continue;
		} else if (c == '\'') {
		    inquote = '\'';
		    continue;
		} else if (((_ctype_ + 1)[ c ] & 0x08 ) )
		    break;
	    }
	    *cp2++ = c;
	}
	*cp2 = '\0';
	if (c == '\0')
	    break;
	cp++;
    }
    *argp++ = 0;
}

 





	static
special(s)
	register char *s;
{
	register char c;
	char b;

	switch (*s) {
	case '^':
		b = *++s;
		if (b == '?') {
		    c = b | 0x40;		 
		} else {
		    c = b & 0x1f;
		}
		break;
	default:
		c = *s;
		break;
	}
	return c;
}

 



	static char *
control(c)
	register cc_t c;
{
	static char buf[5];
	 






	register unsigned int uic = (unsigned int)c;

	if (uic == 0x7f)
		return ("^?");
	if (c == (cc_t)(0377) ) {
		return "off";
	}
	if (uic >= 0x80) {
		buf[0] = '\\';
		buf[1] = ((c>>6)&07) + '0';
		buf[2] = ((c>>3)&07) + '0';
		buf[3] = (c&07) + '0';
		buf[4] = 0;
	} else if (uic >= 0x20) {
		buf[0] = c;
		buf[1] = 0;
	} else {
		buf[0] = '^';
		buf[1] = '@'+c;
		buf[2] = 0;
	}
	return (buf);
}



 





struct sendlist {
    char	*name;		 
    char	*help;		 
    int		needconnect;	 
    int		narg;		 
    int		(*handler)();	 
    int		nbyte;		 
    int		what;		 
};


static int
	send_esc  (void)  ,
	send_help  (void)  ,
	send_docmd  (char *)  ,
	send_dontcmd  (char *)  ,
	send_willcmd  (char *)  ,
	send_wontcmd  (char *)  ;

static struct sendlist Sendlist[] = {
    { "ao",	"Send Telnet Abort output",		1, 0, 0, 2, 245  },
    { "ayt",	"Send Telnet 'Are You There'",		1, 0, 0, 2, 246  },
    { "brk",	"Send Telnet Break",			1, 0, 0, 2, 243  },
    { "break",	0,					1, 0, 0, 2, 243  },
    { "ec",	"Send Telnet Erase Character",		1, 0, 0, 2, 247  },
    { "el",	"Send Telnet Erase Line",		1, 0, 0, 2, 248  },
    { "escape",	"Send current escape character",	1, 0, send_esc, 1, 0 },
    { "ga",	"Send Telnet 'Go Ahead' sequence",	1, 0, 0, 2, 249  },
    { "ip",	"Send Telnet Interrupt Process",	1, 0, 0, 2, 244  },
    { "intp",	0,					1, 0, 0, 2, 244  },
    { "interrupt", 0,					1, 0, 0, 2, 244  },
    { "intr",	0,					1, 0, 0, 2, 244  },
    { "nop",	"Send Telnet 'No operation'",		1, 0, 0, 2, 241  },
    { "eor",	"Send Telnet 'End of Record'",		1, 0, 0, 2, 239  },
    { "abort",	"Send Telnet 'Abort Process'",		1, 0, 0, 2, 238  },
    { "susp",	"Send Telnet 'Suspend Process'",	1, 0, 0, 2, 237  },
    { "eof",	"Send Telnet End of File Character",	1, 0, 0, 2, 236  },
    { "synch",	"Perform Telnet 'Synch operation'",	1, 0, dosynch, 2, 0 },
    { "getstatus", "Send request for STATUS",		1, 0, get_status, 6, 0 },
    { "?",	"Display send options",			0, 0, send_help, 0, 0 },
    { "help",	0,					0, 0, send_help, 0, 0 },
    { "do",	0,					0, 1, send_docmd, 3, 0 },
    { "dont",	0,					0, 1, send_dontcmd, 3, 0 },
    { "will",	0,					0, 1, send_willcmd, 3, 0 },
    { "wont",	0,					0, 1, send_wontcmd, 3, 0 },
    { 0 }
};




    static int
sendcmd(argc, argv)
    int  argc;
    char **argv;
{
    int count;		 
    int i;
    int question = 0;	 
    struct sendlist *s;	 
    int success = 0;
    int needconnect = 0;

    if (argc < 2) {
	printf("need at least one argument for 'send' command\n");
	printf("'send ?' for help\n");
	return 0;
    }
     





    count = 0;
    for (i = 1; i < argc; i++) {
	s = ((struct sendlist *) genget( argv[i] , (char **) Sendlist, sizeof(struct sendlist))) ;
	if (s == 0) {
	    printf("Unknown send argument '%s'\n'send ?' for help.\n",
			argv[i]);
	    return 0;
	} else if (Ambiguous(s)) {
	    printf("Ambiguous send argument '%s'\n'send ?' for help.\n",
			argv[i]);
	    return 0;
	}
	if (i + s->narg >= argc) {
	    fprintf((&__sF[2]) ,
	    "Need %d argument%s to 'send %s' command.  'send %s ?' for help.\n",
		s->narg, s->narg == 1 ? "" : "s", s->name, s->name);
	    return 0;
	}
	count += s->nbyte;
	if (s->handler == send_help) {
	    send_help();
	    return 0;
	}

	i += s->narg;
	needconnect += s->needconnect;
    }
    if (!connected && needconnect) {
	printf("?Need to be connected first.\n");
	printf("'send ?' for help\n");
	return 0;
    }
     
    if ((ring_empty_count(&netoring))  < count) {
	printf("There is not enough room in the buffer TO the network\n");
	printf("to process your request.  Nothing will be done.\n");
	printf("('send synch' will throw away most data in the network\n");
	printf("buffer, if this might help.)\n");
	return 0;
    }
     
    count = 0;
    for (i = 1; i < argc; i++) {
	if ((s = ((struct sendlist *) genget( argv[i] , (char **) Sendlist, sizeof(struct sendlist))) ) == 0) {
	    fprintf((&__sF[2]) , "Telnet 'send' error - argument disappeared!\n");
	    (void) quit();
	     
	}
	if (s->handler) {
	    count++;
	    success += (*s->handler)((s->narg > 0) ? argv[i+1] : 0,
				  (s->narg > 1) ? argv[i+2] : 0);
	    i += s->narg;
	} else {
	    { { *netoring.supply =   255   ; ring_supplied(&netoring, 1); } ; { *netoring.supply =    s->what  ; ring_supplied(&netoring, 1); } ; } ;
	    printoption("SENT", 255 , s->what);
	}
    }
    return (count == success);
}

    static int
send_esc()
{
    { *netoring.supply =  escape ; ring_supplied(&netoring, 1); } ;
    return 1;
}

    static int
send_docmd(name)
    char *name;
{
    return(send_tncmd(send_do, "do", name));
}

    static int
send_dontcmd(name)
    char *name;
{
    return(send_tncmd(send_dont, "dont", name));
}
    static int
send_willcmd(name)
    char *name;
{
    return(send_tncmd(send_will, "will", name));
}
    static int
send_wontcmd(name)
    char *name;
{
    return(send_tncmd(send_wont, "wont", name));
}

    int
send_tncmd(func, cmd, name)
    void	(*func)();
    char	*cmd, *name;
{
    char **cpp;
    extern char *telopts[];
    register int val = 0;

    if (isprefix(name, "?")) {
	register int col, len;

	printf("Usage: send %s <value|option>\n", cmd);
	printf("\"value\" must be from 0 to 255\n");
	printf("Valid options are:\n\t");

	col = 8;
	for (cpp = telopts; *cpp; cpp++) {
	    len = strlen(*cpp) + 3;
	    if (col + len > 65) {
		printf("\n\t");
		col = 8;
	    }
	    printf(" \"%s\"", *cpp);
	    col += len;
	}
	printf("\n");
	return 0;
    }
    cpp = (char **)genget(name, telopts, sizeof(char *));
    if (Ambiguous(cpp)) {
	fprintf((&__sF[2]) ,"'%s': ambiguous argument ('send %s ?' for help).\n",
					name, cmd);
	return 0;
    }
    if (cpp) {
	val = cpp - telopts;
    } else {
	register char *cp = name;

	while (*cp >= '0' && *cp <= '9') {
	    val *= 10;
	    val += *cp - '0';
	    cp++;
	}
	if (*cp != 0) {
	    fprintf((&__sF[2]) , "'%s': unknown argument ('send %s ?' for help).\n",
					name, cmd);
	    return 0;
	} else if (val < 0 || val > 255) {
	    fprintf((&__sF[2]) , "'%s': bad value ('send %s ?' for help).\n",
					name, cmd);
	    return 0;
	}
    }
    if (!connected) {
	printf("?Need to be connected first.\n");
	return 0;
    }
    (*func)(val, 1);
    return 1;
}

    static int
send_help()
{
    struct sendlist *s;	 
    for (s = Sendlist; s->name; s++) {
	if (s->help)
	    printf("%-15s %s\n", s->name, s->help);
    }
    return(0);
}

 




    static int
lclchars()
{
    donelclchars = 1;
    return 1;
}

    static int
togdebug()
{

    if (net > 0 &&
	(SetSockOpt(net, 0xffff , 0x0001 , debug)) < 0) {
	    perror("setsockopt (SO_DEBUG)");
    }







    return 1;
}


    static int
togcrlf()
{
    if (crlf) {
	printf("Will send carriage returns as telnet <CR><LF>.\n");
    } else {
	printf("Will send carriage returns as telnet <CR><NUL>.\n");
    }
    return 1;
}

int binmode;

    static int
togbinary(val)
    int val;
{
    donebinarytoggle = 1;

    if (val >= 0) {
	binmode = val;
    } else {
	if ((options[ 0  ]& 0x02 )  &&
				(options[ 0  ]& 0x08 ) ) {
	    binmode = 1;
	} else if ((! (options[  0   ]& 0x02 ) )  &&
				(! (options[  0   ]& 0x08 ) ) ) {
	    binmode = 0;
	}
	val = binmode ? 0 : 1;
    }

    if (val == 1) {
	if ((options[ 0  ]& 0x02 )  &&
					(options[ 0  ]& 0x08 ) ) {
	    printf("Already operating in binary mode with remote host.\n");
	} else {
	    printf("Negotiating binary mode with remote host.\n");
	    tel_enter_binary(3);
	}
    } else {
	if ((! (options[  0   ]& 0x02 ) )  &&
					(! (options[  0   ]& 0x08 ) ) ) {
	    printf("Already in network ascii mode with remote host.\n");
	} else {
	    printf("Negotiating network ascii mode with remote host.\n");
	    tel_leave_binary(3);
	}
    }
    return 1;
}

    static int
togrbinary(val)
    int val;
{
    donebinarytoggle = 1;

    if (val == -1)
	val = (options[ 0  ]& 0x08 )  ? 0 : 1;

    if (val == 1) {
	if ((options[ 0  ]& 0x08 ) ) {
	    printf("Already receiving in binary mode.\n");
	} else {
	    printf("Negotiating binary mode on input.\n");
	    tel_enter_binary(1);
	}
    } else {
	if ((! (options[  0   ]& 0x08 ) ) ) {
	    printf("Already receiving in network ascii mode.\n");
	} else {
	    printf("Negotiating network ascii mode on input.\n");
	    tel_leave_binary(1);
	}
    }
    return 1;
}

    static int
togxbinary(val)
    int val;
{
    donebinarytoggle = 1;

    if (val == -1)
	val = (options[ 0  ]& 0x02 )  ? 0 : 1;

    if (val == 1) {
	if ((options[ 0  ]& 0x02 ) ) {
	    printf("Already transmitting in binary mode.\n");
	} else {
	    printf("Negotiating binary mode on output.\n");
	    tel_enter_binary(2);
	}
    } else {
	if ((! (options[  0   ]& 0x02 ) ) ) {
	    printf("Already transmitting in network ascii mode.\n");
	} else {
	    printf("Negotiating network ascii mode on output.\n");
	    tel_leave_binary(2);
	}
    }
    return 1;
}


static int togglehelp  (void)  ;




struct togglelist {
    char	*name;		 
    char	*help;		 
    int		(*handler)();	 
    int		*variable;
    char	*actionexplanation;
    int		needconnect;	 
};

static struct togglelist Togglelist[] = {
    { "autoflush",
	"flushing of output when sending interrupt characters",
	    0,
		&autoflush,
		    "flush output when sending interrupt characters", 0 },
    { "autosynch",
	"automatic sending of interrupt characters in urgent mode",
	    0,
		&autosynch,
		    "send interrupt characters in urgent mode", 0 },
# 697 "commands.c"

    { "skiprc",
	"don't read ~/.telnetrc file",
	    0,
		&skiprc,
		    "skip reading of ~/.telnetrc file", 0 },
    { "binary",
	"sending and receiving of binary data",
	    togbinary,
		0,
		    0, 1 },
    { "inbinary",
	"receiving of binary data",
	    togrbinary,
		0,
		    0, 1 },
    { "outbinary",
	"sending of binary data",
	    togxbinary,
		0,
		    0, 1 },
    { "crlf",
	"sending carriage returns as telnet <CR><LF>",
	    togcrlf,
		&crlf,
		    0, 0 },
    { "crmod",
	"mapping of received carriage returns",
	    0,
		&crmod,
		    "map carriage return on output", 0 },
    { "localchars",
	"local recognition of certain control characters",
	    lclchars,
		&localchars,
		    "recognize certain control characters", 0 },
    { " ", "", 0, 0 },		 
# 745 "commands.c"

    { "debug",
	"debugging",
	    togdebug,
		&debug,
		    "turn on socket level debugging", 0 },
    { "netdata",
	"printing of hexadecimal network data (debugging)",
	    0,
		&netdata,
		    "print hexadecimal representation of network traffic", 0 },
    { "prettydump",
	"output of \"netdata\" to user readable format (debugging)",
	    0,
		&prettydump,
		    "print user readable output for \"netdata\"", 0 },
    { "options",
	"viewing of options processing (debugging)",
	    0,
		&showoptions,
		    "show option processing", 0 },

    { "termdata",
	"(debugging) toggle printing of hexadecimal terminal data",
	    0,
		&termdata,
		    "print hexadecimal representation of terminal traffic", 0 },

    { "?",
	0,
	    togglehelp, 0 },
    { "help",
	0,
	    togglehelp, 0 },
    { 0 }
};

    static int
togglehelp()
{
    struct togglelist *c;

    for (c = Togglelist; c->name; c++) {
	if (c->help) {
	    if (*c->help)
		printf("%-15s toggle %s\n", c->name, c->help);
	    else
		printf("\n");
	}
    }
    printf("\n");
    printf("%-15s %s\n", "?", "display help information");
    return 0;
}

    static void
settogglehelp(set)
    int set;
{
    struct togglelist *c;

    for (c = Togglelist; c->name; c++) {
	if (c->help) {
	    if (*c->help)
		printf("%-15s %s %s\n", c->name, set ? "enable" : "disable",
						c->help);
	    else
		printf("\n");
	}
    }
}




    static int
toggle(argc, argv)
    int  argc;
    char *argv[];
{
    int retval = 1;
    char *name;
    struct togglelist *c;

    if (argc < 2) {
	fprintf((&__sF[2]) ,
	    "Need an argument to 'toggle' command.  'toggle ?' for help.\n");
	return 0;
    }
    argc--;
    argv++;
    while (argc--) {
	name = *argv++;
	c = (struct togglelist *) genget( name , (char **) Togglelist, sizeof(struct togglelist)) ;
	if (Ambiguous(c)) {
	    fprintf((&__sF[2]) , "'%s': ambiguous argument ('toggle ?' for help).\n",
					name);
	    return 0;
	} else if (c == 0) {
	    fprintf((&__sF[2]) , "'%s': unknown argument ('toggle ?' for help).\n",
					name);
	    return 0;
	} else if (!connected && c->needconnect) {
	    printf("?Need to be connected first.\n");
	    printf("'send ?' for help\n");
	    return 0;
	} else {
	    if (c->variable) {
		*c->variable = !*c->variable;		 
		if (c->actionexplanation) {
		    printf("%s %s.\n", *c->variable? "Will" : "Won't",
							c->actionexplanation);
		}
	    }
	    if (c->handler) {
		retval &= (*c->handler)(-1);
	    }
	}
    }
    return retval;
}

 




struct termios  new_tc = { 0 };


struct setlist {
    char *name;				 
    char *help;				 
    void (*handler)();
    cc_t *charp;			 
};

static struct setlist Setlist[] = {

    { "echo", 	"character to toggle local echoing on/off", 0, &echoc },

    { "escape",	"character to escape back to telnet command mode", 0, &escape },
    { "rlogin", "rlogin escape character", 0, &rlogin },
    { "tracefile", "file to write trace information to", SetNetTrace, (cc_t *)NetTraceFile},
    { " ", "" },
    { " ", "The following need 'localchars' to be toggled true", 0, 0 },
    { "flushoutput", "character to cause an Abort Output", 0, & new_tc.c_cc[15 ]   },
    { "interrupt", "character to cause an Interrupt Process", 0, & new_tc.c_cc[8 ]   },
    { "quit",	"character to cause an Abort process", 0, & new_tc.c_cc[9 ]   },
    { "eof",	"character to cause an EOF ", 0, & new_tc.c_cc[0 ]   },
    { " ", "" },
    { " ", "The following are for local editing in linemode", 0, 0 },
    { "erase",	"character to use to erase a character", 0, & new_tc.c_cc[3 ]   },
    { "kill",	"character to use to erase a line", 0, & new_tc.c_cc[5 ]   },
    { "lnext",	"character to use for literal next", 0, & new_tc.c_cc[14 ]   },
    { "susp",	"character to cause a Suspend Process", 0, & new_tc.c_cc[10 ]   },
    { "reprint", "character to use for line reprint", 0, & new_tc.c_cc[6 ]   },
    { "worderase", "character to use to erase a word", 0, & new_tc.c_cc[4 ]   },
    { "start",	"character to use for XON", 0, & new_tc.c_cc[12 ]   },
    { "stop",	"character to use for XOFF", 0, & new_tc.c_cc[13 ]   },
    { "forw1",	"alternate end of line character", 0, & new_tc.c_cc[1 ]   },
    { "forw2",	"alternate end of line character", 0, & new_tc.c_cc[1 ]   },
    { "ayt",	"alternate AYT character", 0, & new_tc.c_cc[18 ]   },
    { 0 }
};

# 938 "commands.c"


    static struct setlist *
getset(name)
    char *name;
{
    return (struct setlist *)
		genget(name, (char **) Setlist, sizeof(struct setlist));
}

    void
set_escape_char(s)
    char *s;
{
	if (rlogin != (0377) ) {
		rlogin = (s && *s) ? special(s) : (0377) ;
		printf("Telnet rlogin escape character is '%s'.\n",
					control(rlogin));
	} else {
		escape = (s && *s) ? special(s) : (0377) ;
		printf("Telnet escape character is '%s'.\n", control(escape));
	}
}

    static int
setcmd(argc, argv)
    int  argc;
    char *argv[];
{
    int value;
    struct setlist *ct;
    struct togglelist *c;

    if (argc < 2 || argc > 3) {
	printf("Format is 'set Name Value'\n'set ?' for help.\n");
	return 0;
    }
    if ((argc == 2) && (isprefix(argv[1], "?") || isprefix(argv[1], "help"))) {
	for (ct = Setlist; ct->name; ct++)
	    printf("%-15s %s\n", ct->name, ct->help);
	printf("\n");
	settogglehelp(1);
	printf("%-15s %s\n", "?", "display help information");
	return 0;
    }

    ct = getset(argv[1]);
    if (ct == 0) {
	c = (struct togglelist *) genget( argv[1] , (char **) Togglelist, sizeof(struct togglelist)) ;
	if (c == 0) {
	    fprintf((&__sF[2]) , "'%s': unknown argument ('set ?' for help).\n",
			argv[1]);
	    return 0;
	} else if (Ambiguous(c)) {
	    fprintf((&__sF[2]) , "'%s': ambiguous argument ('set ?' for help).\n",
			argv[1]);
	    return 0;
	} else if (!connected && c->needconnect) {
	    printf("?Need to be connected first.\n");
	    printf("'send ?' for help\n");
	    return 0;
	}

	if (c->variable) {
	    if ((argc == 2) || (strcmp("on", argv[2]) == 0))
		*c->variable = 1;
	    else if (strcmp("off", argv[2]) == 0)
		*c->variable = 0;
	    else {
		printf("Format is 'set togglename [on|off]'\n'set ?' for help.\n");
		return 0;
	    }
	    if (c->actionexplanation) {
		printf("%s %s.\n", *c->variable? "Will" : "Won't",
							c->actionexplanation);
	    }
	}
	if (c->handler)
	    (*c->handler)(1);
    } else if (argc != 3) {
	printf("Format is 'set Name Value'\n'set ?' for help.\n");
	return 0;
    } else if (Ambiguous(ct)) {
	fprintf((&__sF[2]) , "'%s': ambiguous argument ('set ?' for help).\n",
			argv[1]);
	return 0;
    } else if (ct->handler) {
	(*ct->handler)(argv[2]);
	printf("%s set to \"%s\".\n", ct->name, (char *)ct->charp);
    } else {
	if (strcmp("off", argv[2])) {
	    value = special(argv[2]);
	} else {
	    value = (0377) ;
	}
	*(ct->charp) = (cc_t)value;
	printf("%s character is '%s'.\n", ct->name, control(*(ct->charp)));
    }
    slc_check();
    return 1;
}

    static int
unsetcmd(argc, argv)
    int  argc;
    char *argv[];
{
    struct setlist *ct;
    struct togglelist *c;
    register char *name;

    if (argc < 2) {
	fprintf((&__sF[2]) ,
	    "Need an argument to 'unset' command.  'unset ?' for help.\n");
	return 0;
    }
    if (isprefix(argv[1], "?") || isprefix(argv[1], "help")) {
	for (ct = Setlist; ct->name; ct++)
	    printf("%-15s %s\n", ct->name, ct->help);
	printf("\n");
	settogglehelp(0);
	printf("%-15s %s\n", "?", "display help information");
	return 0;
    }

    argc--;
    argv++;
    while (argc--) {
	name = *argv++;
	ct = getset(name);
	if (ct == 0) {
	    c = (struct togglelist *) genget( name , (char **) Togglelist, sizeof(struct togglelist)) ;
	    if (c == 0) {
		fprintf((&__sF[2]) , "'%s': unknown argument ('unset ?' for help).\n",
			name);
		return 0;
	    } else if (Ambiguous(c)) {
		fprintf((&__sF[2]) , "'%s': ambiguous argument ('unset ?' for help).\n",
			name);
		return 0;
	    }
	    if (c->variable) {
		*c->variable = 0;
		if (c->actionexplanation) {
		    printf("%s %s.\n", *c->variable? "Will" : "Won't",
							c->actionexplanation);
		}
	    }
	    if (c->handler)
		(*c->handler)(0);
	} else if (Ambiguous(ct)) {
	    fprintf((&__sF[2]) , "'%s': ambiguous argument ('unset ?' for help).\n",
			name);
	    return 0;
	} else if (ct->handler) {
	    (*ct->handler)(0);
	    printf("%s reset to \"%s\".\n", ct->name, (char *)ct->charp);
	} else {
	    *(ct->charp) = (0377) ;
	    printf("%s character is '%s'.\n", ct->name, control(*(ct->charp)));
	}
    }
    return 1;
}

 




extern int kludgelinemode;

    static int
dokludgemode()
{
    kludgelinemode = 1;
    send_wont(34 , 1);
    send_dont(3 , 1);
    send_dont(1 , 1);
}


    static int
dolinemode()
{

    if (kludgelinemode)
	send_dont(3 , 1);

    send_will(34 , 1);
    send_dont(1 , 1);
    return 1;
}

    static int
docharmode()
{

    if (kludgelinemode)
	send_do(3 , 1);
    else

    send_wont(34 , 1);
    send_do(1 , 1);
    return 1;
}

    static int
dolmmode(bit, on)
    int bit, on;
{
    unsigned char c;
    extern int linemode;

    if ((! (options[  34   ]& 0x02 ) ) ) {
	printf("?Need to have LINEMODE option enabled first.\n");
	printf("'mode ?' for help.\n");
	return 0;
    }

    if (on)
	c = (linemode | bit);
    else
	c = (linemode & ~bit);
    lm_mode(&c, 1, 1);
    return 1;
}

    int
setmode(bit)
{
    return dolmmode(bit, 1);
}

    int
clearmode(bit)
{
    return dolmmode(bit, 0);
}

struct modelist {
	char	*name;		 
	char	*help;		 
	int	(*handler)();	 
	int	needconnect;	 
	int	arg1;
};

extern int modehelp();

static struct modelist ModeList[] = {
    { "character", "Disable LINEMODE option",	docharmode, 1 },

    { "",	"(or disable obsolete line-by-line mode)", 0 },

    { "line",	"Enable LINEMODE option",	dolinemode, 1 },

    { "",	"(or enable obsolete line-by-line mode)", 0 },

    { "", "", 0 },
    { "",	"These require the LINEMODE option to be enabled", 0 },
    { "isig",	"Enable signal trapping",	setmode, 1, 0x02  },
    { "+isig",	0,				setmode, 1, 0x02  },
    { "-isig",	"Disable signal trapping",	clearmode, 1, 0x02  },
    { "edit",	"Enable character editing",	setmode, 1, 0x01  },
    { "+edit",	0,				setmode, 1, 0x01  },
    { "-edit",	"Disable character editing",	clearmode, 1, 0x01  },
    { "softtabs", "Enable tab expansion",	setmode, 1, 0x08  },
    { "+softtabs", 0,				setmode, 1, 0x08  },
    { "-softtabs", "Disable character editing",	clearmode, 1, 0x08  },
    { "litecho", "Enable literal character echo", setmode, 1, 0x10  },
    { "+litecho", 0,				setmode, 1, 0x10  },
    { "-litecho", "Disable literal character echo", clearmode, 1, 0x10  },
    { "help",	0,				modehelp, 0 },

    { "kludgeline", 0,				dokludgemode, 1 },

    { "", "", 0 },
    { "?",	"Print help information",	modehelp, 0 },
    { 0 },
};


    int
modehelp()
{
    struct modelist *mt;

    printf("format is:  'mode Mode', where 'Mode' is one of:\n\n");
    for (mt = ModeList; mt->name; mt++) {
	if (mt->help) {
	    if (*mt->help)
		printf("%-15s %s\n", mt->name, mt->help);
	    else
		printf("\n");
	}
    }
    return 0;
}




    static int
modecmd(argc, argv)
    int  argc;
    char *argv[];
{
    struct modelist *mt;

    if (argc != 2) {
	printf("'mode' command requires an argument\n");
	printf("'mode ?' for help.\n");
    } else if ((mt = (struct modelist *) genget( argv[1] , (char **) ModeList, sizeof(struct modelist)) ) == 0) {
	fprintf((&__sF[2]) , "Unknown mode '%s' ('mode ?' for help).\n", argv[1]);
    } else if (Ambiguous(mt)) {
	fprintf((&__sF[2]) , "Ambiguous mode '%s' ('mode ?' for help).\n", argv[1]);
    } else if (mt->needconnect && !connected) {
	printf("?Need to be connected first.\n");
	printf("'mode ?' for help.\n");
    } else if (mt->handler) {
	return (*mt->handler)(mt->arg1);
    }
    return 0;
}

 




    static int
display(argc, argv)
    int  argc;
    char *argv[];
{
    struct togglelist *tl;
    struct setlist *sl;

















    if (argc == 1) {
	for (tl = Togglelist; tl->name; tl++) {
	    if ( tl ->variable &&  tl ->actionexplanation) { if (* tl ->variable) { printf("will"); } else { printf("won't"); } printf(" %s.\n",  tl ->actionexplanation); } ;
	}
	printf("\n");
	for (sl = Setlist; sl->name; sl++) {
	    if ( sl ->name && * sl ->name != ' ') { if ( sl ->handler == 0) printf("%-15s [%s]\n",  sl ->name, control(* sl ->charp)); else printf("%-15s \"%s\"\n",  sl ->name, (char *) sl ->charp); } ;
	}
    } else {
	int i;

	for (i = 1; i < argc; i++) {
	    sl = getset(argv[i]);
	    tl = (struct togglelist *) genget( argv[i] , (char **) Togglelist, sizeof(struct togglelist)) ;
	    if (Ambiguous(sl) || Ambiguous(tl)) {
		printf("?Ambiguous argument '%s'.\n", argv[i]);
		return 0;
	    } else if (!sl && !tl) {
		printf("?Unknown argument '%s'.\n", argv[i]);
		return 0;
	    } else {
		if (tl) {
		    if ( tl ->variable &&  tl ->actionexplanation) { if (* tl ->variable) { printf("will"); } else { printf("won't"); } printf(" %s.\n",  tl ->actionexplanation); } ;
		}
		if (sl) {
		    if ( sl ->name && * sl ->name != ' ') { if ( sl ->handler == 0) printf("%-15s [%s]\n",  sl ->name, control(* sl ->charp)); else printf("%-15s \"%s\"\n",  sl ->name, (char *) sl ->charp); } ;
		}
	    }
	}
    }
 optionstatus();
    return 1;


}

 




 


	static int
setescape(argc, argv)
	int argc;
	char *argv[];
{
	register char *arg;
	char buf[50];

	printf(
	    "Deprecated usage - please use 'set escape%s%s' in the future.\n",
				(argc > 2)? " ":"", (argc > 2)? argv[1]: "");
	if (argc > 2)
		arg = argv[1];
	else {
		printf("new escape character: ");
		(void) fgets(buf, sizeof(buf), (&__sF[0]) );
		arg = buf;
	}
	if (arg[0] != '\0')
		escape = arg[0];
	if (!In3270) {
		printf("Escape character is '%s'.\n", control(escape));
	}
	(void) fflush((&__sF[1]) );
	return 1;
}

     
    static int
togcrmod()
{
    crmod = !crmod;
    printf("Deprecated usage - please use 'toggle crmod' in the future.\n");
    printf("%s map carriage return on output.\n", crmod ? "Will" : "Won't");
    (void) fflush((&__sF[1]) );
    return 1;
}

     
    int
suspend()
{

    setcommandmode();
    {
	long oldrows, oldcols, newrows, newcols, err;

	err = (TerminalWindowSize(&oldrows, &oldcols) == 0) ? 1 : 0;
	(void) kill(0, 18 );
	 




	if (TerminalWindowSize(&newrows, &newcols) && connected &&
	    (err || ((oldrows != newrows) || (oldcols != newcols)))) {
		sendnaws();
	}
    }
     
    TerminalSaveState();
    setconnmode(0);



    return 1;
}


     
    int
shell(argc, argv)
    int argc;
    char *argv[];
{
    long oldrows, oldcols, newrows, newcols, err;

    setcommandmode();

    err = (TerminalWindowSize(&oldrows, &oldcols) == 0) ? 1 : 0;
    switch(vfork()) {
    case -1:
	perror("Fork failed\n");
	break;

    case 0:
	{
	     


	    register char *shellp, *shellname;
	    extern char *strrchr();

	    shellp = getenv("SHELL");
	    if (shellp == 0 )
		shellp = "/bin/sh";
	    if ((shellname = strrchr(shellp, '/')) == 0)
		shellname = shellp;
	    else
		shellname++;
	    if (argc > 1)
		execl(shellp, shellname, "-c", &saveline[1], 0);
	    else
		execl(shellp, shellname, 0);
	    perror("Execl");
	    _exit(1);
	}
    default:
	    (void)wait((int *)0);	 

	    if (TerminalWindowSize(&newrows, &newcols) && connected &&
		(err || ((oldrows != newrows) || (oldcols != newcols)))) {
		    sendnaws();
	    }
	    break;
    }
    return 1;
}




     
    static
bye(argc, argv)
    int  argc;		 
    char *argv[];	 
{
    extern int resettermname;

    if (connected) {
	(void) shutdown(net, 2);
	printf("Connection closed.\n");
	(void) NetClose(net);
	connected = 0;
	resettermname = 1;



	 
	tninit();



    }
    if ((argc != 2) || (strcmp(argv[1], "fromquit") != 0)) {
	longjmp(toplevel, 1);
	 
    }
    return 1;			 
}

 
quit()
{
	(void) call(bye, "bye", "fromquit", 0);
	Exit(0);
	 
}

 
	int
logout()
{
	send_do(18 , 1);
	(void) netflush();
	return 1;
}


 



struct slclist {
	char	*name;
	char	*help;
	void	(*handler)();
	int	arg;
};

static void slc_help();

struct slclist SlcList[] = {
    { "export",	"Use local special character definitions",
						slc_mode_export,	0 },
    { "import",	"Use remote special character definitions",
						slc_mode_import,	1 },
    { "check",	"Verify remote special character definitions",
						slc_mode_import,	0 },
    { "help",	0,				slc_help,		0 },
    { "?",	"Print help information",	slc_help,		0 },
    { 0 },
};

    static void
slc_help()
{
    struct slclist *c;

    for (c = SlcList; c->name; c++) {
	if (c->help) {
	    if (*c->help)
		printf("%-15s %s\n", c->name, c->help);
	    else
		printf("\n");
	}
    }
}

    static struct slclist *
getslc(name)
    char *name;
{
    return (struct slclist *)
		genget(name, (char **) SlcList, sizeof(struct slclist));
}

    static
slccmd(argc, argv)
    int  argc;
    char *argv[];
{
    struct slclist *c;

    if (argc != 2) {
	fprintf((&__sF[2]) ,
	    "Need an argument to 'slc' command.  'slc ?' for help.\n");
	return 0;
    }
    c = getslc(argv[1]);
    if (c == 0) {
	fprintf((&__sF[2]) , "'%s': unknown argument ('slc ?' for help).\n",
    				argv[1]);
	return 0;
    }
    if (Ambiguous(c)) {
	fprintf((&__sF[2]) , "'%s': ambiguous argument ('slc ?' for help).\n",
    				argv[1]);
	return 0;
    }
    (*c->handler)(c->arg);
    slcstate();
    return 1;
}

 



struct envlist {
	char	*name;
	char	*help;
	void	(*handler)();
	int	narg;
};

extern struct env_lst *
	env_define  (unsigned char *, unsigned char *)  ;
extern void
	env_undefine  (unsigned char *)  ,
	env_export  (unsigned char *)  ,
	env_unexport  (unsigned char *)  ,
	env_send  (unsigned char *)  ,



	env_list  (void)  ;
static void
	env_help  (void)  ;

struct envlist EnvList[] = {
    { "define",	"Define an environment variable",
						(void (*)())env_define,	2 },
    { "undefine", "Undefine an environment variable",
						env_undefine,	1 },
    { "export",	"Mark an environment variable for automatic export",
						env_export,	1 },
    { "unexport", "Don't mark an environment variable for automatic export",
						env_unexport,	1 },
    { "send",	"Send an environment variable", env_send,	1 },
    { "list",	"List the current environment variables",
						env_list,	0 },




    { "help",	0,				env_help,		0 },
    { "?",	"Print help information",	env_help,		0 },
    { 0 },
};

    static void
env_help()
{
    struct envlist *c;

    for (c = EnvList; c->name; c++) {
	if (c->help) {
	    if (*c->help)
		printf("%-15s %s\n", c->name, c->help);
	    else
		printf("\n");
	}
    }
}

    static struct envlist *
getenvcmd(name)
    char *name;
{
    return (struct envlist *)
		genget(name, (char **) EnvList, sizeof(struct envlist));
}

env_cmd(argc, argv)
    int  argc;
    char *argv[];
{
    struct envlist *c;

    if (argc < 2) {
	fprintf((&__sF[2]) ,
	    "Need an argument to 'environ' command.  'environ ?' for help.\n");
	return 0;
    }
    c = getenvcmd(argv[1]);
    if (c == 0) {
	fprintf((&__sF[2]) , "'%s': unknown argument ('environ ?' for help).\n",
    				argv[1]);
	return 0;
    }
    if (Ambiguous(c)) {
	fprintf((&__sF[2]) , "'%s': ambiguous argument ('environ ?' for help).\n",
    				argv[1]);
	return 0;
    }
    if (c->narg + 2 != argc) {
	fprintf((&__sF[2]) ,
	    "Need %s%d argument%s to 'environ %s' command.  'environ ?' for help.\n",
		c->narg < argc + 2 ? "only " : "",
		c->narg, c->narg == 1 ? "" : "s", c->name);
	return 0;
    }
    (*c->handler)(argv[2], argv[3]);
    return 1;
}

struct env_lst {
	struct env_lst *next;	 
	struct env_lst *prev;	 
	unsigned char *var;	 
	unsigned char *value;	 
	int export;		 
	int welldefined;	 
};

struct env_lst envlisthead;

	struct env_lst *
env_find(var)
	unsigned char *var;
{
	register struct env_lst *ep;

	for (ep = envlisthead.next; ep; ep = ep->next) {
		if (strcmp((char *)ep->var, (char *)var) == 0)
			return(ep);
	}
	return(0 );
}

	void
env_init()
{
	extern char **environ;
	register char **epp, *cp;
	register struct env_lst *ep;
	extern char *strchr();

	for (epp = environ; *epp; epp++) {
		if (cp = strchr(*epp, '=')) {
			*cp = '\0';
			ep = env_define((unsigned char *)*epp,
					(unsigned char *)cp+1);
			ep->export = 0;
			*cp = '=';
		}
	}
	 




	if ((ep = env_find("DISPLAY"))
	    && ((*ep->value == ':')
		|| (strncmp((char *)ep->value, "unix:", 5) == 0))) {
		char hbuf[256+1];
		char *cp2 = strchr((char *)ep->value, ':');

		gethostname(hbuf, 256);
		hbuf[256] = '\0';
		cp = (char *)malloc(strlen(hbuf) + strlen(cp2) + 1);
		sprintf((char *)cp, "%s%s", hbuf, cp2);
		free(ep->value);
		ep->value = (unsigned char *)cp;
	}
	 




	if ((env_find("USER") == 0 ) && (ep = env_find("LOGNAME"))) {
		env_define((unsigned char *)"USER", ep->value);
		env_unexport((unsigned char *)"USER");
	}
	env_export((unsigned char *)"DISPLAY");
	env_export((unsigned char *)"PRINTER");
}

	struct env_lst *
env_define(var, value)
	unsigned char *var, *value;
{
	register struct env_lst *ep;

	if (ep = env_find(var)) {
		if (ep->var)
			free(ep->var);
		if (ep->value)
			free(ep->value);
	} else {
		ep = (struct env_lst *)malloc(sizeof(struct env_lst));
		ep->next = envlisthead.next;
		envlisthead.next = ep;
		ep->prev = &envlisthead;
		if (ep->next)
			ep->next->prev = ep;
	}
	ep->welldefined = opt_welldefined(var);
	ep->export = 1;
	ep->var = (unsigned char *)strdup((char *)var);
	ep->value = (unsigned char *)strdup((char *)value);
	return(ep);
}

	void
env_undefine(var)
	unsigned char *var;
{
	register struct env_lst *ep;

	if (ep = env_find(var)) {
		ep->prev->next = ep->next;
		if (ep->next)
			ep->next->prev = ep->prev;
		if (ep->var)
			free(ep->var);
		if (ep->value)
			free(ep->value);
		free(ep);
	}
}

	void
env_export(var)
	unsigned char *var;
{
	register struct env_lst *ep;

	if (ep = env_find(var))
		ep->export = 1;
}

	void
env_unexport(var)
	unsigned char *var;
{
	register struct env_lst *ep;

	if (ep = env_find(var))
		ep->export = 0;
}

	void
env_send(var)
	unsigned char *var;
{
	register struct env_lst *ep;

	if ((! (options[  39   ]& 0x01 ) ) 



		) {
		fprintf((&__sF[2]) ,
		    "Cannot send '%s': Telnet ENVIRON option not enabled\n",
									var);
		return;
	}
	ep = env_find(var);
	if (ep == 0) {
		fprintf((&__sF[2]) , "Cannot send '%s': variable not defined\n",
									var);
		return;
	}
	env_opt_start_info();
	env_opt_add(ep->var);
	env_opt_end(0);
}

	void
env_list()
{
	register struct env_lst *ep;

	for (ep = envlisthead.next; ep; ep = ep->next) {
		printf("%c %-20s %s\n", ep->export ? '*' : ' ',
					ep->var, ep->value);
	}
}

	unsigned char *
env_default(init, welldefined)
	int init;
{
	static struct env_lst *nep = 0 ;

	if (init) {
		nep = &envlisthead;
		return;
	}
	if (nep) {
		while (nep = nep->next) {
			if (nep->export && (nep->welldefined == welldefined))
				return(nep->var);
		}
	}
	return(0 );
}

	unsigned char *
env_getvalue(var)
	unsigned char *var;
{
	register struct env_lst *ep;

	if (ep = env_find(var))
		return(ep->value);
	return(0 );
}

# 1925 "commands.c"


# 2007 "commands.c"



# 2041 "commands.c"


 


     
    static
status(argc, argv)
    int	 argc;
    char *argv[];
{
    if (connected) {
	printf("Connected to %s.\n", hostname);
	if ((argc < 2) || strcmp(argv[1], "notmuch")) {
	    int mode = getconnmode();

	    if ((options[ 34  ]& 0x02 ) ) {
		printf("Operating with LINEMODE option\n");
		printf("%s line editing\n", (mode& 0x01 ) ? "Local" : "No");
		printf("%s catching of signals\n",
					(mode& 0x02 ) ? "Local" : "No");
		slcstate();

	    } else if (kludgelinemode && (! (options[  3   ]& 0x08 ) ) ) {
		printf("Operating in obsolete linemode\n");

	    } else {
		printf("Operating in single character mode\n");
		if (localchars)
		    printf("Catching signals locally\n");
	    }
	    printf("%s character echo\n", (mode& 0x0200 ) ? "Local" : "Remote");
	    if ((options[ 33  ]& 0x02 ) )
		printf("%s flow control\n", (mode& 0x0100 ) ? "Local" : "No");
	}
    } else {
	printf("No connection.\n");
    }

    printf("Escape character is '%s'.\n", control(escape));
    (void) fflush((&__sF[1]) );
# 2109 "commands.c"

    return 1;
}


 


ayt_status()
{
    (void) call(status, "status", "notmuch", 0);
}


unsigned long inet_addr();

    int
tn(argc, argv)
    int argc;
    char *argv[];
{
    register struct hostent *host = 0, *alias = 0;
    struct sockaddr_in sin;
    struct sockaddr_in ladr;
    struct servent *sp = 0;
    unsigned long temp;
    extern char *inet_ntoa();

    char *srp = 0, *strrchr();
    unsigned long sourceroute(), srlen;

    char *cmd, *hostp = 0, *portp = 0, *user = 0, *aliasp = 0;

     
    memset((char *)&sin, 0, sizeof(sin));

    if (connected) {
	printf("?Already connected to %s\n", hostname);
	seteuid(getuid());
	setuid(getuid());
	return 0;
    }
    if (argc < 2) {
	(void) strcpy(line, "open ");
	printf("(to) ");
	(void) fgets(&line[strlen(line)], sizeof(line) - strlen(line), (&__sF[0]) );
	makeargv();
	argc = margc;
	argv = margv;
    }
    cmd = *argv;
    --argc; ++argv;
    while (argc) {
	if (strcmp(*argv, "help") == 0 || isprefix(*argv, "?"))
	    goto usage;
	if (strcmp(*argv, "-l") == 0) {
	    --argc; ++argv;
	    if (argc == 0)
		goto usage;
	    user = *argv++;
	    --argc;
	    continue;
	}
	if (strcmp(*argv, "-b") == 0) {
	    --argc; ++argv;
	    if (argc == 0)
		goto usage;
	    aliasp = *argv++;
	    --argc;
	    continue;
	}
	if (strcmp(*argv, "-a") == 0) {
	    --argc; ++argv;
	    autologin = 1;
	    continue;
	}
	if (hostp == 0) {
	    hostp = *argv++;
	    --argc;
	    continue;
	}
	if (portp == 0) {
	    portp = *argv++;
	    --argc;
	    continue;
	}
    usage:
	printf("usage: %s [-l user] [-a] host-name [port]\n", cmd);
	seteuid(getuid());
	setuid(getuid());
	return 0;
    }
    if (hostp == 0)
	goto usage;


    if (hostp[0] == '@' || hostp[0] == '!') {
	if ((hostname = strrchr(hostp, ':')) == 0 )
	    hostname = strrchr(hostp, '@');
	hostname++;
	srp = 0;
	temp = sourceroute(hostp, &srp, &srlen);
	if (temp == 0) {
	    herror(srp);
	    seteuid(getuid());
	    setuid(getuid());
	    return 0;
	} else if (temp == -1) {
	    printf("Bad source route option: %s\n", hostp);
	    seteuid(getuid());
	    setuid(getuid());
	    return 0;
	} else {
	    sin.sin_addr.s_addr = temp;
	    sin.sin_family = 2 ;
	}
    } else {

	temp = inet_addr(hostp);
	if (temp != ((u_int32_t)( 0xffffffff ))  ) {
	    sin.sin_addr.s_addr = temp;
	    sin.sin_family = 2 ;
	    host = gethostbyaddr((char *)&temp, sizeof(temp), 2 );
	    if (host)
	        (void) strcpy(_hostname, host->h_name);
	    else
		(void) strcpy(_hostname, hostp);
	    hostname = _hostname;
	} else {
	    host = gethostbyname(hostp);
	    if (host) {
		sin.sin_family = host->h_addrtype;

		memmove((caddr_t)&sin.sin_addr,
				host->h_addr_list[0], host->h_length);



		strncpy(_hostname, host->h_name, sizeof(_hostname));
		_hostname[sizeof(_hostname)-1] = '\0';
		hostname = _hostname;
	    } else {
		herror(hostp);
		seteuid(getuid());
		setuid(getuid());
		return 0;
	    }
	}

    }

    if (portp) {
	if (*portp == '-') {
	    portp++;
	    telnetport = 1;
	} else
	    telnetport = 0;
	sin.sin_port = atoi(portp);
	if (sin.sin_port == 0) {
	    sp = getservbyname(portp, "tcp");
	    if (sp)
		sin.sin_port = sp->s_port;
	    else {
		printf("%s: bad port number\n", portp);
		seteuid(getuid());
		setuid(getuid());
		return 0;
	    }
	} else {



	    sin.sin_port = ({ register u_int16_t __x = (   sin.sin_port   ); __asm ("rorw $8, %w1" : "=r" (__x) : "0" (__x)); __x; })   ;
	}
    } else {
	if (sp == 0) {
	    sp = getservbyname("telnet", "tcp");
	    if (sp == 0) {
		fprintf((&__sF[2]) , "telnet: tcp/telnet: unknown service\n");
		seteuid(getuid());
		setuid(getuid());
		return 0;
	    }
	    sin.sin_port = sp->s_port;
	}
	telnetport = 1;
    }
    printf("Trying %s...\n", inet_ntoa(sin.sin_addr));
    do {
	net = socket(2 , 1 , 0);
	seteuid(getuid());
	setuid(getuid());
	if (net < 0) {
	    perror("telnet: socket");
	    return 0;
	}
	if (aliasp) {
	    memset ((caddr_t)&ladr, 0, sizeof (ladr));
	    temp = inet_addr(aliasp);
	    if (temp != ((u_int32_t)( 0xffffffff ))  ) {
	        ladr.sin_addr.s_addr = temp;
	        ladr.sin_family = 2 ;
	        alias = gethostbyaddr((char *)&temp, sizeof(temp), 2 );
	    } else {
	        alias = gethostbyname(aliasp);
	        if (alias) {
		    ladr.sin_family = alias->h_addrtype;

		    memmove((caddr_t)&ladr.sin_addr,
			alias->h_addr_list[0], alias->h_length);




	        } else {
		    herror(aliasp);
		    return 0;
	        }
	    }
            ladr.sin_port = ({ register u_int16_t __x = (   0   ); __asm ("rorw $8, %w1" : "=r" (__x) : "0" (__x)); __x; })   ;
  
            if (bind (net, (struct sockaddr *)&ladr, sizeof(ladr)) < 0) {
                perror(aliasp);; 
                (void) close(net);    
		return 0;
            }
        }
 
	if (srp && setsockopt(net, 0 , 1 , (char *)srp, srlen) < 0)
		perror("setsockopt (IP_OPTIONS)");


	{





	    if (tos < 0)
		tos = 0x10 ;	 
	    if (tos
		&& (setsockopt(net, 0 , 3 ,
		    (char *)&tos, sizeof(int)) < 0)
		&& (errno != 42 ))
		    perror("telnet: setsockopt (IP_TOS) (ignored)");
	}


	if (debug && SetSockOpt(net, 0xffff , 0x0001 , 1) < 0) {
		perror("setsockopt (SO_DEBUG)");
	}

	if (connect(net, (struct sockaddr *)&sin, sizeof (sin)) < 0) {

	    if (host && host->h_addr_list[1]) {
		int oerrno = errno;

		fprintf((&__sF[2]) , "telnet: connect to address %s: ",
						inet_ntoa(sin.sin_addr));
		errno = oerrno;
		perror((char *)0);
		host->h_addr_list++;
		memmove((caddr_t)&sin.sin_addr,
			host->h_addr_list[0], host->h_length);
		(void) NetClose(net);
		continue;
	    }

	    perror("telnet: Unable to connect to remote host");
	    return 0;
	}
	connected++;



    } while (connected == 0);
    cmdrc(hostp, hostname);
    if (autologin && user == 0 ) {
	struct passwd *pw;

	user = getenv("USER");
	if (user == 0  ||
	    (pw = getpwnam(user)) && pw->pw_uid != getuid()) {
		if (pw = getpwuid(getuid()))
			user = pw->pw_name;
		else
			user = 0 ;
	}
    }
    if (user) {
	env_define((unsigned char *)"USER", (unsigned char *)user);
	env_export((unsigned char *)"USER");
    }
    (void) call(status, "status", "notmuch", 0);
    if (setjmp(peerdied) == 0)
	telnet(user);
    (void) NetClose(net);
    ExitString("Connection closed by foreign host.\n",1);
     
}



static char
	openhelp[] =	"connect to a site",
	closehelp[] =	"close current connection",
	logouthelp[] =	"forcibly logout remote user and close the connection",
	quithelp[] =	"exit telnet",
	statushelp[] =	"print status information",
	helphelp[] =	"print help information",
	sendhelp[] =	"transmit special characters ('send ?' for more)",
	sethelp[] = 	"set operating parameters ('set ?' for more)",
	unsethelp[] = 	"unset operating parameters ('unset ?' for more)",
	togglestring[] ="toggle operating parameters ('toggle ?' for more)",
	slchelp[] =	"change state of special charaters ('slc ?' for more)",
	displayhelp[] =	"display operating parameters",







	zhelp[] =	"suspend telnet",

	shellhelp[] =	"invoke a subshell",
	envhelp[] =	"change environment variables ('environ ?' for more)",
	modestring[] = "try to enter line or character mode ('mode ?' for more)";

static int	help();

static Command cmdtab[] = {
	{ "close",	closehelp,	bye,		1 },
	{ "logout",	logouthelp,	logout,		1 },
	{ "display",	displayhelp,	display,	0 },
	{ "mode",	modestring,	modecmd,	0 },
	{ "open",	openhelp,	tn,		0 },
	{ "quit",	quithelp,	quit,		0 },
	{ "send",	sendhelp,	sendcmd,	0 },
	{ "set",	sethelp,	setcmd,		0 },
	{ "unset",	unsethelp,	unsetcmd,	0 },
	{ "status",	statushelp,	status,		0 },
	{ "toggle",	togglestring,	toggle,		0 },
	{ "slc",	slchelp,	slccmd,		0 },







	{ "z",		zhelp,		suspend,	0 },




	{ "!",		shellhelp,	shell,		0 },

	{ "environ",	envhelp,	env_cmd,	0 },
	{ "?",		helphelp,	help,		0 },

	{ "skey",	0 ,		skey_calc,	0 },

	0
};

static char	crmodhelp[] =	"deprecated command -- use 'toggle crmod' instead";
static char	escapehelp[] =	"deprecated command -- use 'set escape' instead";

static Command cmdtab2[] = {
	{ "help",	0,		help,		0 },
	{ "escape",	escapehelp,	setescape,	0 },
	{ "crmod",	crmodhelp,	togcrmod,	0 },
	0
};


 



     
    static
call(__builtin_va_alist )
    long __builtin_va_alist; ...  
{
    va_list ap;
    typedef int (*intrtn_t)();
    intrtn_t routine;
    char *args[100];
    int argno = 0;

    (( ap ) = (va_list)&__builtin_va_alist) ;
    routine = ((*(  intrtn_t  *)(( ap ) += (((sizeof(   intrtn_t  ) + sizeof(long) - 1) / sizeof(long)) * sizeof(long)) , ( ap ) - (((sizeof(   intrtn_t  ) + sizeof(long) - 1) / sizeof(long)) * sizeof(long)) )) );
    while ((args[argno++] = (*(  char *  *)(( ap ) += (((sizeof(   char *  ) + sizeof(long) - 1) / sizeof(long)) * sizeof(long)) , ( ap ) - (((sizeof(   char *  ) + sizeof(long) - 1) / sizeof(long)) * sizeof(long)) )) ) != 0) {
	;
    }
    ((void)0) ;
    return (*routine)(argno-1, args);
}


    static Command *
getcmd(name)
    char *name;
{
    Command *cm;

    if (cm = (Command *) genget(name, (char **) cmdtab, sizeof(Command)))
	return cm;
    return (Command *) genget(name, (char **) cmdtab2, sizeof(Command));
}

    void
command(top, tbuf, cnt)
    int top;
    char *tbuf;
    int cnt;
{
    register Command *c;

    setcommandmode();
    if (!top) {
	__sputc(  '\n'  ,   (&__sF[1])  )  ;

    } else {
	(void) signal(2 , (void (*) (int)  )0 );
	(void) signal(3 , (void (*) (int)  )0 );

    }
    for (;;) {
	if (rlogin == (0377) )
		printf("%s> ", prompt);
	if (tbuf) {
	    register char *cp;
	    cp = line;
	    while (cnt > 0 && (*cp++ = *tbuf++) != '\n')
		cnt--;
	    tbuf = 0;
	    if (cp == line || *--cp != '\n' || cp == line)
		goto getline;
	    *cp = '\0';
	    if (rlogin == (0377) )
		printf("%s\n", line);
	} else {
	getline:
	    if (rlogin != (0377) )
		printf("%s> ", prompt);
	    if (fgets(line, sizeof(line), (&__sF[0]) ) == 0 ) {
		if ((((  (&__sF[0])   )->_flags & 0x0020 ) != 0)   || (((  (&__sF[0])   )->_flags & 0x0040 ) != 0)  ) {
		    (void) quit();
		     
		}
		break;
	    }
	}
	if (line[0] == 0)
	    break;
	makeargv();
	if (margv[0] == 0) {
	    break;
	}
	c = getcmd(margv[0]);
	if (Ambiguous(c)) {
	    printf("?Ambiguous command\n");
	    continue;
	}
	if (c == 0) {
	    printf("?Invalid command\n");
	    continue;
	}
	if (c->needconnect && !connected) {
	    printf("?Need to be connected first.\n");
	    continue;
	}
	if ((*c->handler)(margc, margv)) {
	    break;
	}
    }
    if (!top) {
	if (!connected) {
	    longjmp(toplevel, 1);
	     
	}





	setconnmode(0);

    }
}

 


	static
help(argc, argv)
	int argc;
	char *argv[];
{
	register Command *c;

	if (argc == 1) {
		printf("Commands may be abbreviated.  Commands are:\n\n");
		for (c = cmdtab; c->name; c++)
			if (c->help) {
				printf("%-*s\t%s\n", (sizeof ("connect")) , c->name,
								    c->help);
			}
		return 0;
	}
	while (--argc > 0) {
		register char *arg;
		arg = *++argv;
		c = getcmd(arg);
		if (Ambiguous(c))
			printf("?Ambiguous help command %s\n", arg);
		else if (c == (Command *)0)
			printf("?Invalid help command %s\n", arg);
		else
			printf("%s\n", c->help);
	}
	return 0;
}

static char *rcname = 0;
static char rcbuf[128];

cmdrc(m1, m2)
	char *m1, *m2;
{
    register Command *c;
    FILE *rcfile;
    int gotmachine = 0;
    int l1 = strlen(m1);
    int l2 = strlen(m2);
    char m1save[64];

    if (skiprc)
	return;

    strcpy(m1save, m1);
    m1 = m1save;

    if (rcname == 0) {
	rcname = getenv("HOME");
	if (rcname && (strlen(rcname) + 10) < sizeof(rcbuf))
	    strcpy(rcbuf, rcname);
	else
	    rcbuf[0] = '\0';
	strcat(rcbuf, "/.telnetrc");
	rcname = rcbuf;
    }

    if ((rcfile = fopen(rcname, "r")) == 0) {
	return;
    }

    for (;;) {
	if (fgets(line, sizeof(line), rcfile) == 0 )
	    break;
	if (line[0] == 0)
	    break;
	if (line[0] == '#')
	    continue;
	if (gotmachine) {
	    if (! ((_ctype_ + 1)[ line[0] ] & 0x08 ) )
		gotmachine = 0;
	}
	if (gotmachine == 0) {
	    if (((_ctype_ + 1)[ line[0] ] & 0x08 ) )
		continue;
	    if (strncasecmp(line, m1, l1) == 0)
		strncpy(line, &line[l1], sizeof(line) - l1);
	    else if (strncasecmp(line, m2, l2) == 0)
		strncpy(line, &line[l2], sizeof(line) - l2);
	    else if (strncasecmp(line, "DEFAULT", 7) == 0)
		strncpy(line, &line[7], sizeof(line) - 7);
	    else
		continue;
	    if (line[0] != ' ' && line[0] != '\t' && line[0] != '\n')
		continue;
	    gotmachine = 1;
	}
	makeargv();
	if (margv[0] == 0)
	    continue;
	c = getcmd(margv[0]);
	if (Ambiguous(c)) {
	    printf("?Ambiguous command: %s\n", margv[0]);
	    continue;
	}
	if (c == 0) {
	    printf("?Invalid command: %s\n", margv[0]);
	    continue;
	}
	 


	if (c->needconnect && !connected) {
	    printf("?Need to be connected first for %s.\n", margv[0]);
	    continue;
	}
	(*c->handler)(margc, margv);
    }
    fclose(rcfile);
}



 






































	unsigned long
sourceroute(arg, cpp, lenp)
	char	*arg;
	char	**cpp;
	int	*lenp;
{
	static char lsr[44];



	char *cp, *cp2, *lsrp, *lsrep;
	register int tmp;
	struct in_addr sin_addr;
	register struct hostent *host = 0;
	register char c;

	 



	if (cpp == 0  || lenp == 0 )
		return((unsigned long)-1);
	if (*cpp != 0  && *lenp < 7)
		return((unsigned long)-1);
	 



	if (*cpp) {
		lsrp = *cpp;
		lsrep = lsrp + *lenp;
	} else {
		*cpp = lsrp = lsr;
		lsrep = lsrp + 44;
	}

	cp = arg;

	 





	if (*cp == '!') {
		cp++;
		*lsrp++ = 137 ;
	} else
		*lsrp++ = 131 ;








	if (*cp != '@')
		return((unsigned long)-1);


	lsrp++;		 
	*lsrp++ = 4;


	cp++;

	sin_addr.s_addr = 0;

	for (c = 0;;) {
		if (c == ':')
			cp2 = 0;
		else for (cp2 = cp; c = *cp2; cp2++) {
			if (c == ',') {
				*cp2++ = '\0';
				if (*cp2 == '@')
					cp2++;
			} else if (c == '@') {
				*cp2++ = '\0';
			} else if (c == ':') {
				*cp2++ = '\0';
			} else
				continue;
			break;
		}
		if (!c)
			cp2 = 0;

		if ((tmp = inet_addr(cp)) != ((u_int32_t)( 0xffffffff ))  ) {
			sin_addr.s_addr = tmp;
		} else if (host = gethostbyname(cp)) {

			memmove((caddr_t)&sin_addr,
				host->h_addr_list[0], host->h_length);



		} else {
			*cpp = cp;
			return(0);
		}
		memmove(lsrp, (char *)&sin_addr, 4);
		lsrp += 4;
		if (cp2)
			cp = cp2;
		else
			break;
		 


		if (lsrp + 4 > lsrep)
			return((unsigned long)-1);
	}

	if ((*(*cpp+ 1 ) = lsrp - *cpp) <= 7) {
		*cpp = 0;
		*lenp = 0;
		return((unsigned long)-1);
	}
	*lsrp++ = 1 ;  
	*lenp = lsrp - *cpp;
# 2890 "commands.c"

	return(sin_addr.s_addr);
}

