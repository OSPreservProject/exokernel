
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


#include <stdio.h>

#include "xio_tcp_stats.h"

/*                                                                              
 * Print out statistics.
 */                                                                             
void xio_tcp_statprint (tcp_stat *stats)                                       
{
   int i;  
   int r;

   printf("Tcp receive statistics:\n");
   printf(" # segments received                  %7d\n", stats->nseg);
   printf(" # incomplete segments                %7d\n", stats->nincomplete);
   printf(" # acceptable segments                %7d\n", stats->naccept);
   printf(" # unacceptable segments              %7d\n", stats->nunaccept);
   printf(" # acks received                      %7d\n", stats->nack);
   printf(" # acks predicted                     %7d\n", stats->nackprd);     
   printf(" # data predicted                     %7d\n", stats->ndataprd);
   printf(" # syns received                      %7d\n", stats->nsynrcv);
   printf(" # fins received                      %7d\n", stats->nfinrcv);     
   printf(" # resets received                    %7d\n", stats->nrstrcv);     
   printf(" # segments with data                 %7d\n", stats->ndatarcv);
   printf(" # segments in order                  %7d\n", stats->norder);
   printf(" # segments buffered                  %7d\n", stats->nbuf);
   printf(" # segments with data but no usr buf  %7d\n", stats->noutspace);
   printf(" # segments out of order              %7d\n", stats->noutorder);
   printf(" # window probes received             %7d\n", stats->nprobercv);
   printf(" # segments with bad ip cksum         %7d\n", stats->nbadipcksum);
   printf(" # segments with bad tcp cksum        %7d\n", stats->nbadtcpcksum);
   printf(" # segments dropped on receive        %7d\n", stats->ndroprcv);    
   r = 1;
   for (i = 0; i < NLOG2; i++) {
      printf("%6d", r);
      r = r << 1;
   }
   printf("\n");
   for (i = 0; i < NLOG2; i++) {
      printf("%6d", stats->rcvdata[i]);
   }
   printf("\n");

   printf("Tcp send statistics:\n");
   printf(" # acks sent                          %7d\n", stats->nacksnd);
   printf(" # volunteerly acks                   %7d\n", stats->nvack);     
   printf(" # acks sent zero windows             %7d\n", stats->nzerowndsnt);
   printf(" # acks delayed                       %7d\n", stats->ndelayack);
   printf(" # fin segments sent                  %7d\n", stats->nfinsnd);     
   printf(" # syn segments sent                  %7d\n", stats->nsynsnd);     
   printf(" # reset segments sent                %7d\n", stats->nrstsnd);
   printf(" # segments sent with data            %7d\n", stats->ndata);
   printf(" # window probes send                 %7d\n", stats->nprobesnd);
   printf(" # ether retransmissions              %7d\n", stats->nethretrans);
   printf(" # retransmissions                    %7d\n", stats->nretrans);
   printf(" # segments dropped on send           %7d\n", stats->ndropsnd);
   r = 1;
   for (i = 0; i < NLOG2; i++) {
      printf("%6d", r);
      r = r << 1;
   }
   printf("\n");
   for (i = 0; i < NLOG2; i++) {
      printf("%6d", stats->snddata[i]);
   }
   printf("\n");

   printf("Tcp call statistics:\n");
   printf(" # tcp_open calls                     %7d\n", stats->nopen);
   printf(" # tcp_listen calls                   %7d\n", stats->nlisten);
   printf(" # tcp_accept calls                   %7d\n", stats->naccepts);
   printf(" # tcp_read calls                     %7d\n", stats->nread);
   printf(" # tcp_write calls                    %7d\n", stats->nwrite);     
   printf(" # tcp_close calls                    %7d\n", stats->nclose);
}

