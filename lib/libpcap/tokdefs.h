typedef union {
	int i;
	bpf_u_int32 h;
	u_char *e;
	char *s;
	struct stmt *stmt;
	struct arth *a;
	struct {
		struct qual q;
		struct block *b;
	} blk;
	struct block *rblk;
} YYSTYPE;
#define	DST	258
#define	SRC	259
#define	HOST	260
#define	GATEWAY	261
#define	NET	262
#define	MASK	263
#define	PORT	264
#define	LESS	265
#define	GREATER	266
#define	PROTO	267
#define	BYTE	268
#define	ARP	269
#define	RARP	270
#define	IP	271
#define	TCP	272
#define	UDP	273
#define	ICMP	274
#define	IGMP	275
#define	IGRP	276
#define	ATALK	277
#define	DECNET	278
#define	LAT	279
#define	SCA	280
#define	MOPRC	281
#define	MOPDL	282
#define	TK_BROADCAST	283
#define	TK_MULTICAST	284
#define	NUM	285
#define	INBOUND	286
#define	OUTBOUND	287
#define	LINK	288
#define	GEQ	289
#define	LEQ	290
#define	NEQ	291
#define	ID	292
#define	EID	293
#define	HID	294
#define	LSH	295
#define	RSH	296
#define	LEN	297
#define	OR	298
#define	AND	299
#define	UMINUS	300


extern YYSTYPE pcap_lval;
