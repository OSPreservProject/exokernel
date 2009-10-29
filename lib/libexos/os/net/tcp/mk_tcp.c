
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

#include <dpf/old-dpf.h>
#include <netinet/in.h>
#include <exos/debug.h>

static struct dpf_frozen_ir df_connected[] = {
        /*      op              len     r0      r1      imm     */
        {       DPF_EQI,        2,      1,      0,      0x8}, /* 0 */
        {       DPF_BITS16I,    0,      0,      0,      0xc}, /* 1 */
        {       DPF_EQI,        2,      1,      0,      0x6}, /* 2 */
        {       DPF_BITS8I,     0,      0,      0,      0x17}, /* 3 */
	{       DPF_EQI,        2,      1,      0,      0x50d}, /* 10 */
        {       DPF_BITS16I,    0,      0,      0,      0x24}, /* 11 */
        {       DPF_EQI,        2,      1,      0,      0x1a12}, /* 4 */
        {       DPF_BITS16I,    0,      0,      0,      0x1a}, /* 5 */
        {       DPF_EQI,        2,      1,      0,      0x900}, /* 6 */
        {       DPF_BITS16I,    0,      0,      0,      0x1c}, /* 7 */
        {       DPF_EQI,        2,      1,      0,      0x9019}, /* 8 */
        {       DPF_BITS16I,    0,      0,      0,      0x22}, /* 9 */
};


static struct dpf_frozen_ir df_listen[] = {
        /*      op              len     r0      r1      imm     */
        {       DPF_EQI,        2,      1,      0,      0x8}, /* 0 */
        {       DPF_BITS16I,    0,      0,      0,      0xc}, /* 1 */
        {       DPF_EQI,        2,      1,      0,      0x6}, /* 2 */
        {       DPF_BITS8I,     0,      0,      0,      0x17}, /* 3 */
        {       DPF_EQI,        2,      1,      0,      0x50d}, /* 4 */
        {       DPF_BITS16I,    0,      0,      0,      0x24}, /* 5 */
};


/*
 * Get filter; if dst_port is zero, we take the filter that accepts packets
 * from any destination.
 */
void *mk_tcp(int *sz, int src_port, int dst_port, short *src_ip) 
{
    DPRINTF(2, ("mk_tcp: my port %d dst port %d\n", ntohs(dst_port), 
		ntohs(src_port)));

    if (src_port == 0) {

	*sz = sizeof df_listen / sizeof df_listen[0];

	df_listen[4].imm = dst_port;

	return (void *)df_listen;
    } else  {

	*sz = sizeof df_connected / sizeof df_connected[0];

	df_connected[6].imm = src_ip[0] & 0xFFFF;
	df_connected[8].imm = src_ip[1] & 0xFFFF;

	df_connected[10].imm = src_port;
	df_connected[4].imm = dst_port;

	return (void *)df_connected;
    }
}
