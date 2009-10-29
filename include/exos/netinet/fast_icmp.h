/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */


#ifndef __FAST_ICMP__
#define __FAST_ICMP__

#include <sys/types.h>
#include <exos/netinet/fast_ip.h>

/* types for ICMP packets */

#define ICMP_ECHO_REPLY 0
#define ICMP_DST_UNREACHABLE 3
#define ICMP_SOURCE_QUENCE 4
#define ICMP_REDIRECT 5
#define ICMP_ECHO 8
#define ICMP_TYPE_TIME_EXCEEDED 11
#define ICMP_PARAMETER_PROBLEM 12
#define ICMP_TIME_STAMP 13
#define ICMP_TIME_STAMP_REPLY 14
#define ICMP_INFO_REQUEST 15
#define ICMP_INFO_REQUEST_REPLY 16

/* codes for spefic types of ICMP packets */

/* dest unreachable packets */
#define ICMP_CODE_PROTUNREACH 2
#define ICMP_CODE_PORTUNREACH 3

/* echo packets */
#define ICMP_CODE_ECHO 0

/* timestamp packets */
#define ICMP_CODE_TIMESTAMP 0

/* most icmp request types */
struct icmp_generic {
  uint8 type;			/* one of the ICMP_TYPE_*'s above */
  uint8 code;			/* one of the ICMP_CODE_*'s above */
  uint16 checksum;		/* 16 1's comp csum */
  uint32 unused;		/* should be zero */
  struct ip ip;			/* ip header from original packet */
  uint8 data[8];		/* first 64 bits from original packet */
};

/* parameter problem header */
struct icmp_param {
  uint8 type;			/* one of the ICMP_TYPE_*'s above */
  uint8 code;			/* one of the ICMP_CODE_*'s above */
  uint16 checksum;		/* 16 1's comp csum */
  uint8 pointer;		/* which octect was a problem */
  uint8 unused[3];		/* should be zero */
  struct ip ip;			/* ip header from original packet */
  uint8 data[8];		/* first 64 bits from original packet */
};

/* redirect header */
struct icmp_redirect {
  uint8 type;			/* one of the ICMP_TYPE_*'s above */
  uint8 code;			/* one of the ICMP_CODE_*'s above */
  uint16 checksum;		/* 16 1's comp csum */
  uint32 gateway;		/* address of gateway */
  struct ip ip;			/* ip header from original packet */
  uint8 data[8];		/* first 64 bits from original packet */
};


/* struct for things with sequence numbers */
struct icmp_sequenced {
  uint8 type;			/* one of the ICMP_TYPE_*'s above */
  uint8 code;			/* one of the ICMP_CODE_*'s above */
  uint16 checksum;		/* 16 1's comp csum */
  uint16 identifier;
  uint16 sequence;
};

/* timestamp header */
struct icmp_time {
  uint8 type;			/* one of the ICMP_TYPE_*'s above */
  uint8 code;			/* one of the ICMP_CODE_*'s above */
  uint16 checksum;		/* 16 1's comp csum */
  uint16 identifier;
  uint16 sequence;
  uint32 orig;			/* orignate timestamp */
  uint32 receive;		/* receive timestamp */
  uint32 transmit;		/* transmit timestamp */
};

/* different struct names for each type of packet */
#define icmp_unreach icmp_generic
#define icmp_exceeded icmp_generic
#define icmp_quence icmp_generic
#define icmp_redirect icmp_generic
#define icmp_info icmp_sequenced
#define icmp_echo icmp_sequenced

#endif /* __FAST_ICMP__ */

