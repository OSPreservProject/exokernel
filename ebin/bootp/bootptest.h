/* bootptest.h */
/*
 * Hacks for sharing print-bootp.c between tcpdump and bootptest.
 */
#define ESRC(p) (p)
#define EDST(p) (p)
#if 0
#ifndef	USE_BFUNCS
/* Use mem/str functions */
/* There are no overlapped copies, so memcpy is OK. */
#define bcopy(a,b,c)    memcpy(b,a,c)
#define bzero(p,l)      memset(p,0,l)
#define bcmp(a,b,c)     memcmp(a,b,c)
#endif
#endif
extern int vflag; /* verbose flag */

/* global pointers to beginning and end of current packet (during printing) */
extern unsigned char *packetp;
extern unsigned char *snapend;

#ifdef	__STDC__
#define P(args) args
#else
#define P(args) ()
#endif

extern char *ipaddr_string P((struct in_addr *));
extern int printfn P((u_char *s, u_char *ep));

#undef P
