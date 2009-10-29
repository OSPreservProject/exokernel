
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

#include <fd/proc.h>
#include <stdlib.h>
#include <string.h>

#include <xok/sysinfo.h>

//#define PRINTFDSTAT		/* prints fd stats */

#define printf(format,args...) fprintf(stderr,format,## args);

#ifdef FDSTAT

#define SUM(module,name) total += fd_ ## module.name ## _v
#define TOTAL(module) 								\
  total = 0;								\
  APPLYM(module,SUM);							

#define PRTOTAL \
  printf(" total cycles %12qd ",total);					\
  printf(" time (usecs): %qd\n\n",total / ((quad_t) __sysinfo.si_mhz))


#define PRINTENTRY(module,str) \
     TOTAL(module); if(total) {printf("%s\n",str);APPLYM(module,PRNAME); PRTOTAL;}

extern char *__progname;
void pr_fd_stat(void) {
  pctrval total;
  int i;

  STOP(misc,proctime);

  printf("### Statistics for: \"%s\"###\n",__progname);
  printf("FD\n");
  TOTAL(fd);
  APPLYM(fd,PRNAME);
  PRTOTAL;

  printf("FTABLE\n");
  TOTAL(ftable);
  APPLYM(ftable,PRNAME);
  PRTOTAL;

  TOTAL(namei);
  if (total) {
    printf("NAMEI\n");
    APPLYM(namei,PRNAME);
    PRTOTAL;
  }

  PRINTENTRY(nfs_lookup,"NFS_LOOKUP");
  PRINTENTRY(nfsc,"NFSC");
  PRINTENTRY(misc,"MISC");

  for (i = 0 ; i < NR_OPS; i++) {
    TOTAL(fd_op[i]);
    if (total > 0) {
      printf("FD_OP %-10s (%d)\n",filp_op_type2name(i),i);
      APPLYM(fd_op[i],PRNAME);
      PRTOTAL;
    }
  }
  PRNAME(ftable,proctime);

  {
    pctrval id;
    id = __cpuid();
    printf("CPU ID: %qd, hastsc %d hsdp5 %d hasp6 %d cpufamily: %qd\n",
	   id, __hastsc(id), __hasp5ctr(id), __hasp6ctr(id),__cpufamily());
	   
    if ( __hasp6ctr(id) ) {
      printf("counter0 %qd counter1 %qd\n",rdpmc(0),rdpmc(1));

    }
  }
}

#else

void pr_fd_stat(void) {}

#endif
