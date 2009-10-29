
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

#if 0

NAME
     vhangup - virtually "hangup" the current controlling  termi-
     nal

SYNOPSIS
     void vhangup(void);

DESCRIPTION
     vhangup() is used by  the  initialization  process  init(1M)
     (among  others) to arrange that users are given "clean" ter-
     minals at login, by revoking access of the  previous  users
     processes  to  the  terminal.   To  effect  this,  vhangup()
     searches the system tables for references to the controlling
     terminal  of  the  invoking process, revoking access permis-
     sions on each  instance  of  the  terminal  that  it  finds.
     Further  attempts  to  access  the  terminal by the affected
     processes will yield I/O errors (EBADF or EIO).  Finally,  a
     SIGHUP  (hangup  signal) is sent to the process group of the
     controlling terminal.

SEE ALSO
     init(1M)

BUGS
     Access to the controlling terminal using /dev/tty  is  still
     possible.

     This call should be replaced by an automatic mechanism  that
     takes place on process exit.


#endif

void 
vhangup(void)
{}
