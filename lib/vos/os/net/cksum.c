
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


#include <vos/net/cksum.h>

/* 
 * Compute Internet Checksum for "count" bytes beginning at location "addr".
 * Taken from RFC 1071.
 * 
 * Leave these rolled, the unrolled versions are later.
 *
 */

long
inet_sum(uint16 *addr, uint16 count, long start) {
    register long sum = start;

    /*  This is the inner loop */
    while( count > 1 )  {
	sum += * (unsigned short *) addr++;
	count -= 2;
    }
    
    /*  Add left-over byte, if any */
    if(count > 0)
	sum += * (unsigned char *) addr;

    return sum;
}


uint16 
inet_cksum(uint16 *addr, uint16 count, long start) {
    register long sum = start;

    /*  This is the inner loop */
    while( count > 1 )  {
	sum += * (unsigned short *) addr++;
	count -= 2;
    }
    
    /*  Add left-over byte, if any */
    if(count > 0)
	sum += * (unsigned char *) addr;
    /*  Fold 32-bit sum to 16 bits */
    while (sum>>16)
	sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}

/************************************************************************/

uint inet_checksum(unsigned short *addr, int count, uint start, int last)
{
    register long sum = start;
    /* An unrolled loop */
    while ( count > 15 ) {
       sum += * (unsigned short *) &addr[0];
       sum += * (unsigned short *) &addr[1];
       sum += * (unsigned short *) &addr[2];
       sum += * (unsigned short *) &addr[3];
       sum += * (unsigned short *) &addr[4];
       sum += * (unsigned short *) &addr[5];
       sum += * (unsigned short *) &addr[6];
       sum += * (unsigned short *) &addr[7];
       addr += 8;
       count -= 16;
    }
    /*  This is the inner loop */
    while( count > 1 )  {
        sum += * (unsigned short *) addr++;
        count -= 2;
    }
    /*  Add left-over byte, if any */
    if(count > 0)
        sum += * (unsigned char *) addr;
    if (last) {
       /*  Fold 32-bit sum to 16 bits */
       while (sum>>16) {
          sum = (sum & 0xffff) + (sum >> 16);
       }
       sum = ~sum & 0xffff;
       if (sum == 0) {
          sum = 0xffff;
       }
       return (sum);
    }
    return sum;
}

/************************************************************************/

