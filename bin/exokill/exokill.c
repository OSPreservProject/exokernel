
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

#include <exos/cap.h>
#include <exos/process.h>
#include <stdio.h>
#include <stdlib.h>
#include <xok/sys_ucall.h>

#include <exos/conf.h>
#include <signal.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
  int r, pid;
  u_int envid;
  char *prog;
  int reap_flag = 0;
  prog = argv[0];

  argv++;
  argc--;
  

  if (argc >= 1 && !strcmp(*argv,"-r")) {
    reap_flag = 1;
    argc--; 
    argv++;
  }

  if (argc > 1 || !(pid = atoi(*argv))) {
    printf("Usage: %s [-r] <pid>\n",prog);
    return -1;
  }
  envid = pid2envid(pid);
  if (envid == 0) {
    printf("pid %d not registered\n",pid);
    return -1;
  }
#ifdef PROCD
  {
    extern int proc_pid_exit(pid_t pid, int);
    proc_pid_exit(pid, (W_EXITCODE(0,SIGKILL)));
  }

  if (reap_flag) {
    extern int proc_pid_reap(pid_t pid);
    printf("reaping pid: %d\n",pid);
    proc_pid_reap(pid);
  }

#endif

  if (r = sys_env_free(CAP_ROOT, envid))
    printf("Not killed\n");
  else
    printf("Killed\n");

  return r;
}
