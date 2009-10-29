
/*  A Bison parser, made from grammar.y
 by  GNU Bison version 1.25
  */

#define YYBISON 1  /* Identify Bison output.  */

#define yyparse pcap_parse
#define yylex pcap_lex
#define yyerror pcap_error
#define yylval pcap_lval
#define yychar pcap_char
#define yydebug pcap_debug
#define yynerrs pcap_nerrs
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

#line 1 "grammar.y"

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <stdio.h>

#include "pcap-int.h"

#include "gencode.h"
#include <pcap-namedb.h>

#include "gnuc.h"
#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#define QSET(q, p, d, a) (q).proto = (p),\
			 (q).dir = (d),\
			 (q).addr = (a)

int n_errors = 0;

static struct qual qerr = { Q_UNDEF, Q_UNDEF, Q_UNDEF, Q_UNDEF };

static void
yyerror(char *msg)
{
	++n_errors;
	bpf_error("%s", msg);
	/* NOTREACHED */
}

#ifndef YYBISON
int yyparse(void);

int
pcap_parse()
{
	return (yyparse());
}
#endif


#line 82 "grammar.y"
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
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		151
#define	YYFLAG		-32768
#define	YYNTBASE	61

#define YYTRANSLATE(x) ((unsigned)(x) <= 300 ? yytranslate[x] : 87)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    45,     2,     2,     2,     2,    47,     2,    54,
    53,    50,    48,     2,    49,     2,    51,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    60,     2,    57,
    56,    55,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
    58,     2,    59,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,    46,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    52
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     3,     5,     6,     8,    12,    16,    20,    24,    26,
    28,    30,    32,    36,    38,    42,    46,    48,    50,    53,
    55,    57,    59,    63,    67,    69,    71,    73,    76,    80,
    83,    86,    89,    92,    95,    99,   101,   105,   109,   111,
   113,   114,   116,   118,   122,   126,   130,   134,   136,   138,
   140,   142,   144,   146,   148,   150,   152,   154,   156,   158,
   160,   162,   164,   166,   168,   170,   172,   175,   178,   181,
   184,   189,   191,   193,   195,   197,   199,   201,   203,   205,
   207,   209,   214,   221,   225,   229,   233,   237,   241,   245,
   249,   253,   256,   260,   262,   264,   266,   268,   270,   272,
   274
};

static const short yyrhs[] = {    62,
    63,     0,    62,     0,     0,    72,     0,    63,    64,    72,
     0,    63,    64,    66,     0,    63,    65,    72,     0,    63,
    65,    66,     0,    44,     0,    43,     0,    67,     0,    86,
     0,    69,    70,    53,     0,    37,     0,    39,    51,    30,
     0,    39,     8,    39,     0,    39,     0,    38,     0,    68,
    66,     0,    45,     0,    54,     0,    67,     0,    71,    64,
    66,     0,    71,    65,    66,     0,    86,     0,    70,     0,
    74,     0,    68,    72,     0,    75,    76,    77,     0,    75,
    76,     0,    75,    77,     0,    75,    12,     0,    75,    78,
     0,    73,    66,     0,    69,    63,    53,     0,    79,     0,
    83,    81,    83,     0,    83,    82,    83,     0,    80,     0,
    79,     0,     0,     4,     0,     3,     0,     4,    43,     3,
     0,     3,    43,     4,     0,     4,    44,     3,     0,     3,
    44,     4,     0,     5,     0,     7,     0,     9,     0,     6,
     0,    33,     0,    16,     0,    14,     0,    15,     0,    17,
     0,    18,     0,    19,     0,    20,     0,    21,     0,    22,
     0,    23,     0,    24,     0,    25,     0,    27,     0,    26,
     0,    75,    28,     0,    75,    29,     0,    10,    30,     0,
    11,    30,     0,    13,    30,    85,    30,     0,    31,     0,
    32,     0,    55,     0,    34,     0,    56,     0,    35,     0,
    57,     0,    36,     0,    86,     0,    84,     0,    79,    58,
    83,    59,     0,    79,    58,    83,    60,    30,    59,     0,
    83,    48,    83,     0,    83,    49,    83,     0,    83,    50,
    83,     0,    83,    51,    83,     0,    83,    47,    83,     0,
    83,    46,    83,     0,    83,    40,    83,     0,    83,    41,
    83,     0,    49,    83,     0,    69,    84,    53,     0,    42,
     0,    47,     0,    46,     0,    57,     0,    55,     0,    56,
     0,    30,     0,    69,    86,    53,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   130,   134,   136,   138,   139,   140,   141,   142,   144,   146,
   148,   149,   151,   153,   154,   156,   158,   170,   171,   173,
   175,   177,   178,   179,   181,   183,   185,   186,   188,   189,
   190,   191,   192,   194,   195,   196,   197,   199,   201,   204,
   205,   208,   209,   210,   211,   212,   213,   216,   217,   218,
   221,   223,   224,   225,   226,   227,   228,   229,   230,   231,
   232,   233,   234,   235,   236,   237,   239,   240,   241,   242,
   243,   244,   245,   247,   248,   249,   251,   252,   253,   255,
   256,   258,   259,   260,   261,   262,   263,   264,   265,   266,
   267,   268,   269,   270,   272,   273,   274,   275,   276,   278,
   279
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","DST","SRC",
"HOST","GATEWAY","NET","MASK","PORT","LESS","GREATER","PROTO","BYTE","ARP","RARP",
"IP","TCP","UDP","ICMP","IGMP","IGRP","ATALK","DECNET","LAT","SCA","MOPRC","MOPDL",
"TK_BROADCAST","TK_MULTICAST","NUM","INBOUND","OUTBOUND","LINK","GEQ","LEQ",
"NEQ","ID","EID","HID","LSH","RSH","LEN","OR","AND","'!'","'|'","'&'","'+'",
"'-'","'*'","'/'","UMINUS","')'","'('","'>'","'='","'<'","'['","']'","':'","prog",
"null","expr","and","or","id","nid","not","paren","pid","qid","term","head",
"rterm","pqual","dqual","aqual","ndaqual","pname","other","relop","irelop","arth",
"narth","byteop","pnum", NULL
};
#endif

static const short yyr1[] = {     0,
    61,    61,    62,    63,    63,    63,    63,    63,    64,    65,
    66,    66,    66,    67,    67,    67,    67,    67,    67,    68,
    69,    70,    70,    70,    71,    71,    72,    72,    73,    73,
    73,    73,    73,    74,    74,    74,    74,    74,    74,    75,
    75,    76,    76,    76,    76,    76,    76,    77,    77,    77,
    78,    79,    79,    79,    79,    79,    79,    79,    79,    79,
    79,    79,    79,    79,    79,    79,    80,    80,    80,    80,
    80,    80,    80,    81,    81,    81,    82,    82,    82,    83,
    83,    84,    84,    84,    84,    84,    84,    84,    84,    84,
    84,    84,    84,    84,    85,    85,    85,    85,    85,    86,
    86
};

static const short yyr2[] = {     0,
     2,     1,     0,     1,     3,     3,     3,     3,     1,     1,
     1,     1,     3,     1,     3,     3,     1,     1,     2,     1,
     1,     1,     3,     3,     1,     1,     1,     2,     3,     2,
     2,     2,     2,     2,     3,     1,     3,     3,     1,     1,
     0,     1,     1,     3,     3,     3,     3,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     2,     2,     2,     2,
     4,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     4,     6,     3,     3,     3,     3,     3,     3,     3,
     3,     2,     3,     1,     1,     1,     1,     1,     1,     1,
     3
};

static const short yydefact[] = {     3,
    41,     0,     0,     0,    54,    55,    53,    56,    57,    58,
    59,    60,    61,    62,    63,    64,    66,    65,   100,    72,
    73,    52,    94,    20,     0,    21,     1,    41,    41,     4,
     0,    27,     0,    40,    39,     0,    81,    80,    69,    70,
     0,     0,     0,    92,    10,     9,    41,    41,    28,     0,
    81,    80,    14,    18,    17,    34,    11,     0,     0,    12,
    43,    42,    48,    51,    49,    50,    32,    67,    68,    30,
    31,    33,     0,    75,    77,    79,     0,     0,     0,     0,
     0,     0,     0,     0,    74,    76,    78,     0,     0,    96,
    95,    98,    99,    97,     0,     0,     6,    41,    41,     5,
    80,     8,     7,    35,    93,   101,     0,     0,    19,    22,
     0,    26,     0,    25,     0,     0,     0,     0,    29,     0,
    90,    91,    89,    88,    84,    85,    86,    87,    37,    38,
    71,    80,    16,    15,     0,    13,     0,     0,    45,    47,
    44,    46,    82,     0,    23,    24,     0,    83,     0,     0,
     0
};

static const short yydefgoto[] = {   149,
     1,    50,    47,    48,   109,    57,    58,    42,   112,   113,
    30,    31,    32,    33,    70,    71,    72,    43,    35,    88,
    89,    36,    37,    95,    38
};

static const short yypact[] = {-32768,
   131,   -10,    19,    30,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,   256,-32768,   -29,   215,   215,-32768,
   213,-32768,    87,    31,-32768,   266,-32768,-32768,-32768,-32768,
   287,   256,   -50,-32768,-32768,-32768,   173,   173,-32768,   -34,
   -24,     9,-32768,-32768,    -6,-32768,-32768,   213,   213,-32768,
    23,    34,-32768,-32768,-32768,-32768,-32768,-32768,-32768,    54,
-32768,-32768,   256,-32768,-32768,-32768,   256,   256,   256,   256,
   256,   256,   256,   256,-32768,-32768,-32768,   256,   256,-32768,
-32768,-32768,-32768,-32768,    38,   244,-32768,   173,   173,-32768,
    12,-32768,-32768,-32768,-32768,-32768,    -3,    52,-32768,-32768,
   -17,    11,   -29,     9,    48,    79,    83,    84,-32768,   119,
    -8,    -8,   278,   290,    29,    29,-32768,-32768,   244,   244,
-32768,    -5,-32768,-32768,     9,-32768,   213,   213,-32768,-32768,
-32768,-32768,-32768,    65,-32768,-32768,    41,-32768,   101,   102,
-32768
};

static const short yypgoto[] = {-32768,
-32768,   105,    -4,     0,   -30,   -55,     6,    -1,-32768,-32768,
   -22,-32768,-32768,-32768,-32768,    33,-32768,    22,-32768,-32768,
-32768,    46,   -18,-32768,   -26
};


#define	YYLAST		344


static const short yytable[] = {    29,
    56,   107,    52,   110,    60,    49,    28,    73,    45,    46,
    51,   -12,    19,    45,    46,    52,    97,   102,   104,    39,
   101,   101,    34,    51,   100,   103,    29,    29,   105,    59,
   -36,    60,   114,    28,    28,   133,    26,   -25,   -25,    81,
    82,    83,    84,   110,   108,    99,    99,   106,    40,    34,
    34,   139,    98,    98,   -12,   -12,    59,   111,    63,    41,
    65,   106,    66,   136,   -12,   115,   116,   131,    34,    34,
    44,   101,   132,   -36,   -36,    49,   117,   118,    83,    84,
    51,   134,   140,   -36,   135,   141,   142,    96,    73,    61,
    62,    63,    64,    65,   147,    66,    99,    29,    67,   148,
   150,   151,   119,    98,    98,    27,   145,   146,   137,   111,
    60,    60,   138,     0,    68,    69,     0,     0,   120,    34,
    34,     0,   121,   122,   123,   124,   125,   126,   127,   128,
    -2,     0,     0,   129,   130,    59,    59,     0,     0,     0,
     2,     3,     0,     4,     5,     6,     7,     8,     9,    10,
    11,    12,    13,    14,    15,    16,    17,    18,    77,    78,
    19,    20,    21,    22,    79,    80,    81,    82,    83,    84,
     0,     0,    23,     0,     0,    24,     0,   143,   144,    25,
     0,     0,     2,     3,    26,     4,     5,     6,     7,     8,
     9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
     0,     0,    19,    20,    21,    22,     0,     0,     0,    53,
    54,    55,     0,     0,    23,     0,     0,    24,     0,     0,
     0,    25,     0,     0,     2,     3,    26,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
    17,    18,    19,     0,    19,    20,    21,    22,     0,    53,
    54,    55,     0,     0,     0,     0,    23,    24,     0,    24,
     0,     0,     0,    25,     0,     0,    26,     0,    26,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    77,    78,    19,     0,     0,    22,    79,
    80,    81,    82,    83,    84,     0,     0,    23,     0,    74,
    75,    76,     0,     0,    25,    77,    78,     0,     0,    26,
     0,    79,    80,    81,    82,    83,    84,    77,    78,     0,
    85,    86,    87,     0,    80,    81,    82,    83,    84,    77,
    78,     0,    90,    91,     0,     0,     0,    81,    82,    83,
    84,    92,    93,    94
};

static const short yycheck[] = {     1,
    31,     8,    29,    59,    31,    28,     1,    58,    43,    44,
    29,     0,    30,    43,    44,    42,    47,    48,    53,    30,
    47,    48,     1,    42,    47,    48,    28,    29,    53,    31,
     0,    58,    59,    28,    29,    39,    54,    43,    44,    48,
    49,    50,    51,    99,    51,    47,    48,    53,    30,    28,
    29,     4,    47,    48,    43,    44,    58,    59,     5,    30,
     7,    53,     9,    53,    53,    43,    44,    30,    47,    48,
    25,    98,    99,    43,    44,    98,    43,    44,    50,    51,
    99,    30,     4,    53,   111,     3,     3,    42,    58,     3,
     4,     5,     6,     7,    30,     9,    98,    99,    12,    59,
     0,     0,    70,    98,    99,     1,   137,   138,   113,   111,
   137,   138,   113,    -1,    28,    29,    -1,    -1,    73,    98,
    99,    -1,    77,    78,    79,    80,    81,    82,    83,    84,
     0,    -1,    -1,    88,    89,   137,   138,    -1,    -1,    -1,
    10,    11,    -1,    13,    14,    15,    16,    17,    18,    19,
    20,    21,    22,    23,    24,    25,    26,    27,    40,    41,
    30,    31,    32,    33,    46,    47,    48,    49,    50,    51,
    -1,    -1,    42,    -1,    -1,    45,    -1,    59,    60,    49,
    -1,    -1,    10,    11,    54,    13,    14,    15,    16,    17,
    18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
    -1,    -1,    30,    31,    32,    33,    -1,    -1,    -1,    37,
    38,    39,    -1,    -1,    42,    -1,    -1,    45,    -1,    -1,
    -1,    49,    -1,    -1,    10,    11,    54,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    30,    -1,    30,    31,    32,    33,    -1,    37,
    38,    39,    -1,    -1,    -1,    -1,    42,    45,    -1,    45,
    -1,    -1,    -1,    49,    -1,    -1,    54,    -1,    54,    14,
    15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
    25,    26,    27,    40,    41,    30,    -1,    -1,    33,    46,
    47,    48,    49,    50,    51,    -1,    -1,    42,    -1,    34,
    35,    36,    -1,    -1,    49,    40,    41,    -1,    -1,    54,
    -1,    46,    47,    48,    49,    50,    51,    40,    41,    -1,
    55,    56,    57,    -1,    47,    48,    49,    50,    51,    40,
    41,    -1,    46,    47,    -1,    -1,    -1,    48,    49,    50,
    51,    55,    56,    57
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/lib/bison.simple"

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

#ifndef alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi)
#include <alloca.h>
#else /* not sparc */
#if defined (MSDOS) && !defined (__TURBOC__)
#include <malloc.h>
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
#include <malloc.h>
 #pragma alloca
#else /* not MSDOS, __TURBOC__, or _AIX */
#ifdef __hpux
#ifdef __cplusplus
extern "C" {
void *alloca (unsigned int);
};
#else /* not __cplusplus */
void *alloca ();
#endif /* not __cplusplus */
#endif /* __hpux */
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc.  */
#endif /* not GNU C.  */
#endif /* alloca not defined.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	return(0)
#define YYABORT 	return(1)
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
int yyparse (void);
#endif

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 196 "/usr/lib/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
      yyss = (short *) alloca (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1, size * sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 1:
#line 131 "grammar.y"
{
	finish_parse(yyvsp[0].blk.b);
;
    break;}
case 3:
#line 136 "grammar.y"
{ yyval.blk.q = qerr; ;
    break;}
case 5:
#line 139 "grammar.y"
{ gen_and(yyvsp[-2].blk.b, yyvsp[0].blk.b); yyval.blk = yyvsp[0].blk; ;
    break;}
case 6:
#line 140 "grammar.y"
{ gen_and(yyvsp[-2].blk.b, yyvsp[0].blk.b); yyval.blk = yyvsp[0].blk; ;
    break;}
case 7:
#line 141 "grammar.y"
{ gen_or(yyvsp[-2].blk.b, yyvsp[0].blk.b); yyval.blk = yyvsp[0].blk; ;
    break;}
case 8:
#line 142 "grammar.y"
{ gen_or(yyvsp[-2].blk.b, yyvsp[0].blk.b); yyval.blk = yyvsp[0].blk; ;
    break;}
case 9:
#line 144 "grammar.y"
{ yyval.blk = yyvsp[-1].blk; ;
    break;}
case 10:
#line 146 "grammar.y"
{ yyval.blk = yyvsp[-1].blk; ;
    break;}
case 12:
#line 149 "grammar.y"
{ yyval.blk.b = gen_ncode(NULL, (bpf_u_int32)yyvsp[0].i,
						   yyval.blk.q = yyvsp[-1].blk.q); ;
    break;}
case 13:
#line 151 "grammar.y"
{ yyval.blk = yyvsp[-1].blk; ;
    break;}
case 14:
#line 153 "grammar.y"
{ yyval.blk.b = gen_scode(yyvsp[0].s, yyval.blk.q = yyvsp[-1].blk.q); ;
    break;}
case 15:
#line 154 "grammar.y"
{ yyval.blk.b = gen_mcode(yyvsp[-2].s, NULL, yyvsp[0].i,
				    yyval.blk.q = yyvsp[-3].blk.q); ;
    break;}
case 16:
#line 156 "grammar.y"
{ yyval.blk.b = gen_mcode(yyvsp[-2].s, yyvsp[0].s, 0,
				    yyval.blk.q = yyvsp[-3].blk.q); ;
    break;}
case 17:
#line 158 "grammar.y"
{
				  /* Decide how to parse HID based on proto */
				  yyval.blk.q = yyvsp[-1].blk.q;
				  switch (yyval.blk.q.proto) {
				  case Q_DECNET:
					yyval.blk.b = gen_ncode(yyvsp[0].s, 0, yyval.blk.q);
					break;
				  default:
					yyval.blk.b = gen_ncode(yyvsp[0].s, 0, yyval.blk.q);
					break;
				  }
				;
    break;}
case 18:
#line 170 "grammar.y"
{ yyval.blk.b = gen_ecode(yyvsp[0].e, yyval.blk.q = yyvsp[-1].blk.q); ;
    break;}
case 19:
#line 171 "grammar.y"
{ gen_not(yyvsp[0].blk.b); yyval.blk = yyvsp[0].blk; ;
    break;}
case 20:
#line 173 "grammar.y"
{ yyval.blk = yyvsp[-1].blk; ;
    break;}
case 21:
#line 175 "grammar.y"
{ yyval.blk = yyvsp[-1].blk; ;
    break;}
case 23:
#line 178 "grammar.y"
{ gen_and(yyvsp[-2].blk.b, yyvsp[0].blk.b); yyval.blk = yyvsp[0].blk; ;
    break;}
case 24:
#line 179 "grammar.y"
{ gen_or(yyvsp[-2].blk.b, yyvsp[0].blk.b); yyval.blk = yyvsp[0].blk; ;
    break;}
case 25:
#line 181 "grammar.y"
{ yyval.blk.b = gen_ncode(NULL, (bpf_u_int32)yyvsp[0].i,
						   yyval.blk.q = yyvsp[-1].blk.q); ;
    break;}
case 28:
#line 186 "grammar.y"
{ gen_not(yyvsp[0].blk.b); yyval.blk = yyvsp[0].blk; ;
    break;}
case 29:
#line 188 "grammar.y"
{ QSET(yyval.blk.q, yyvsp[-2].i, yyvsp[-1].i, yyvsp[0].i); ;
    break;}
case 30:
#line 189 "grammar.y"
{ QSET(yyval.blk.q, yyvsp[-1].i, yyvsp[0].i, Q_DEFAULT); ;
    break;}
case 31:
#line 190 "grammar.y"
{ QSET(yyval.blk.q, yyvsp[-1].i, Q_DEFAULT, yyvsp[0].i); ;
    break;}
case 32:
#line 191 "grammar.y"
{ QSET(yyval.blk.q, yyvsp[-1].i, Q_DEFAULT, Q_PROTO); ;
    break;}
case 33:
#line 192 "grammar.y"
{ QSET(yyval.blk.q, yyvsp[-1].i, Q_DEFAULT, yyvsp[0].i); ;
    break;}
case 34:
#line 194 "grammar.y"
{ yyval.blk = yyvsp[0].blk; ;
    break;}
case 35:
#line 195 "grammar.y"
{ yyval.blk.b = yyvsp[-1].blk.b; yyval.blk.q = yyvsp[-2].blk.q; ;
    break;}
case 36:
#line 196 "grammar.y"
{ yyval.blk.b = gen_proto_abbrev(yyvsp[0].i); yyval.blk.q = qerr; ;
    break;}
case 37:
#line 197 "grammar.y"
{ yyval.blk.b = gen_relation(yyvsp[-1].i, yyvsp[-2].a, yyvsp[0].a, 0);
				  yyval.blk.q = qerr; ;
    break;}
case 38:
#line 199 "grammar.y"
{ yyval.blk.b = gen_relation(yyvsp[-1].i, yyvsp[-2].a, yyvsp[0].a, 1);
				  yyval.blk.q = qerr; ;
    break;}
case 39:
#line 201 "grammar.y"
{ yyval.blk.b = yyvsp[0].rblk; yyval.blk.q = qerr; ;
    break;}
case 41:
#line 205 "grammar.y"
{ yyval.i = Q_DEFAULT; ;
    break;}
case 42:
#line 208 "grammar.y"
{ yyval.i = Q_SRC; ;
    break;}
case 43:
#line 209 "grammar.y"
{ yyval.i = Q_DST; ;
    break;}
case 44:
#line 210 "grammar.y"
{ yyval.i = Q_OR; ;
    break;}
case 45:
#line 211 "grammar.y"
{ yyval.i = Q_OR; ;
    break;}
case 46:
#line 212 "grammar.y"
{ yyval.i = Q_AND; ;
    break;}
case 47:
#line 213 "grammar.y"
{ yyval.i = Q_AND; ;
    break;}
case 48:
#line 216 "grammar.y"
{ yyval.i = Q_HOST; ;
    break;}
case 49:
#line 217 "grammar.y"
{ yyval.i = Q_NET; ;
    break;}
case 50:
#line 218 "grammar.y"
{ yyval.i = Q_PORT; ;
    break;}
case 51:
#line 221 "grammar.y"
{ yyval.i = Q_GATEWAY; ;
    break;}
case 52:
#line 223 "grammar.y"
{ yyval.i = Q_LINK; ;
    break;}
case 53:
#line 224 "grammar.y"
{ yyval.i = Q_IP; ;
    break;}
case 54:
#line 225 "grammar.y"
{ yyval.i = Q_ARP; ;
    break;}
case 55:
#line 226 "grammar.y"
{ yyval.i = Q_RARP; ;
    break;}
case 56:
#line 227 "grammar.y"
{ yyval.i = Q_TCP; ;
    break;}
case 57:
#line 228 "grammar.y"
{ yyval.i = Q_UDP; ;
    break;}
case 58:
#line 229 "grammar.y"
{ yyval.i = Q_ICMP; ;
    break;}
case 59:
#line 230 "grammar.y"
{ yyval.i = Q_IGMP; ;
    break;}
case 60:
#line 231 "grammar.y"
{ yyval.i = Q_IGRP; ;
    break;}
case 61:
#line 232 "grammar.y"
{ yyval.i = Q_ATALK; ;
    break;}
case 62:
#line 233 "grammar.y"
{ yyval.i = Q_DECNET; ;
    break;}
case 63:
#line 234 "grammar.y"
{ yyval.i = Q_LAT; ;
    break;}
case 64:
#line 235 "grammar.y"
{ yyval.i = Q_SCA; ;
    break;}
case 65:
#line 236 "grammar.y"
{ yyval.i = Q_MOPDL; ;
    break;}
case 66:
#line 237 "grammar.y"
{ yyval.i = Q_MOPRC; ;
    break;}
case 67:
#line 239 "grammar.y"
{ yyval.rblk = gen_broadcast(yyvsp[-1].i); ;
    break;}
case 68:
#line 240 "grammar.y"
{ yyval.rblk = gen_multicast(yyvsp[-1].i); ;
    break;}
case 69:
#line 241 "grammar.y"
{ yyval.rblk = gen_less(yyvsp[0].i); ;
    break;}
case 70:
#line 242 "grammar.y"
{ yyval.rblk = gen_greater(yyvsp[0].i); ;
    break;}
case 71:
#line 243 "grammar.y"
{ yyval.rblk = gen_byteop(yyvsp[-1].i, yyvsp[-2].i, yyvsp[0].i); ;
    break;}
case 72:
#line 244 "grammar.y"
{ yyval.rblk = gen_inbound(0); ;
    break;}
case 73:
#line 245 "grammar.y"
{ yyval.rblk = gen_inbound(1); ;
    break;}
case 74:
#line 247 "grammar.y"
{ yyval.i = BPF_JGT; ;
    break;}
case 75:
#line 248 "grammar.y"
{ yyval.i = BPF_JGE; ;
    break;}
case 76:
#line 249 "grammar.y"
{ yyval.i = BPF_JEQ; ;
    break;}
case 77:
#line 251 "grammar.y"
{ yyval.i = BPF_JGT; ;
    break;}
case 78:
#line 252 "grammar.y"
{ yyval.i = BPF_JGE; ;
    break;}
case 79:
#line 253 "grammar.y"
{ yyval.i = BPF_JEQ; ;
    break;}
case 80:
#line 255 "grammar.y"
{ yyval.a = gen_loadi(yyvsp[0].i); ;
    break;}
case 82:
#line 258 "grammar.y"
{ yyval.a = gen_load(yyvsp[-3].i, yyvsp[-1].a, 1); ;
    break;}
case 83:
#line 259 "grammar.y"
{ yyval.a = gen_load(yyvsp[-5].i, yyvsp[-3].a, yyvsp[-1].i); ;
    break;}
case 84:
#line 260 "grammar.y"
{ yyval.a = gen_arth(BPF_ADD, yyvsp[-2].a, yyvsp[0].a); ;
    break;}
case 85:
#line 261 "grammar.y"
{ yyval.a = gen_arth(BPF_SUB, yyvsp[-2].a, yyvsp[0].a); ;
    break;}
case 86:
#line 262 "grammar.y"
{ yyval.a = gen_arth(BPF_MUL, yyvsp[-2].a, yyvsp[0].a); ;
    break;}
case 87:
#line 263 "grammar.y"
{ yyval.a = gen_arth(BPF_DIV, yyvsp[-2].a, yyvsp[0].a); ;
    break;}
case 88:
#line 264 "grammar.y"
{ yyval.a = gen_arth(BPF_AND, yyvsp[-2].a, yyvsp[0].a); ;
    break;}
case 89:
#line 265 "grammar.y"
{ yyval.a = gen_arth(BPF_OR, yyvsp[-2].a, yyvsp[0].a); ;
    break;}
case 90:
#line 266 "grammar.y"
{ yyval.a = gen_arth(BPF_LSH, yyvsp[-2].a, yyvsp[0].a); ;
    break;}
case 91:
#line 267 "grammar.y"
{ yyval.a = gen_arth(BPF_RSH, yyvsp[-2].a, yyvsp[0].a); ;
    break;}
case 92:
#line 268 "grammar.y"
{ yyval.a = gen_neg(yyvsp[0].a); ;
    break;}
case 93:
#line 269 "grammar.y"
{ yyval.a = yyvsp[-1].a; ;
    break;}
case 94:
#line 270 "grammar.y"
{ yyval.a = gen_loadlen(); ;
    break;}
case 95:
#line 272 "grammar.y"
{ yyval.i = '&'; ;
    break;}
case 96:
#line 273 "grammar.y"
{ yyval.i = '|'; ;
    break;}
case 97:
#line 274 "grammar.y"
{ yyval.i = '<'; ;
    break;}
case 98:
#line 275 "grammar.y"
{ yyval.i = '>'; ;
    break;}
case 99:
#line 276 "grammar.y"
{ yyval.i = '='; ;
    break;}
case 101:
#line 279 "grammar.y"
{ yyval.i = yyvsp[-1].i; ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 498 "/usr/lib/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;
}
#line 281 "grammar.y"

