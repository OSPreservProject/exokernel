
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

#include "stat.h"

#define NUM_GRAPHS 3
#define MAX_RANGE 22
#define GRAPH_HEIGHT "3.5"
#define GRAPH_WIDTH "5.25"

extern void webdemo_main (int argc, char **argv);

void main () {
  int argcnt = 7;
  char *argvector[10];

  argvector[0] = "webdemo";
  argvector[1] = "mola2";
  argvector[2] = "file0k.html";
  argvector[3] = "cone2";
  argvector[4] = "80";
  argvector[5] = "3";
  argvector[6] = "2000";

  init_stats ("zwolle", 49998, 0);
  init_graphs (NUM_GRAPHS, "0 Bytes.100 Bytes.10 KB", "Cheetah/xok",
	       MAX_RANGE, GRAPH_HEIGHT, GRAPH_WIDTH);

  init2_stats ("zwolle", 49999, 0);
  init2_graphs (NUM_GRAPHS, "0 Bytes.100 Bytes.10 KB", "NCSA/OpenBSD",
	       MAX_RANGE, GRAPH_HEIGHT, GRAPH_WIDTH);

  stats_enable (1);
  stats_barrier ("barrier1d");
  webdemo_main (argcnt, argvector);
  stats_done (1);

  argvector[2] = "file100.html";

  stats_enable (2);
  stats_barrier ("barrier1d");
  webdemo_main (argcnt, argvector);
  stats_done (2);

  argvector[2] = "file10k.html";
  argvector[6] = "1000";

  stats_enable (3);
  stats_barrier ("barrier1d");
  webdemo_main (argcnt, argvector);
  stats_done (3);

  argvector[1] = "isca2";
  argvector[2] = "file0k.html";
  argvector[3] = "cone2";
  argvector[4] = "81";
  argvector[6] = "2000";

  stats2_enable (1);
  stats2_barrier ("barrier1d");
  webdemo_main (argcnt, argvector);
  stats2_done (1);

  argvector[2] = "file100.html";

  stats2_enable (2);
  stats2_barrier ("barrier1d");
  webdemo_main (argcnt, argvector);
  stats2_done (2);

  argvector[2] = "file10k.html";
  argvector[6] = "1000";

  stats2_enable (3);
  stats2_barrier ("barrier1d");
  webdemo_main (argcnt, argvector);
  stats2_done (3);
}

