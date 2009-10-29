
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

#define INTERNAL_TTY_TYPE    20
#define INTERNAL_STDOUT_TYPE 21

#define CHECKCONSOLE(name,filp)			\
  if (!strcmp(name,"console")) {		\
      /* see fd/console.c */			\
        filp->f_mode = S_IFCHR & 0644;		\
        filp->f_dev = DEV_DEVICE;		\
        filp->f_ino = CONSOLE_TYPE;		\
	filp->f_pos = 0;			\
	filp->f_flags = flags;			\
	filp_refcount_init(filp);		\
	filp_refcount_inc(filp);		\
	filp->f_owner = __current->uid;		\
	filp->op_type = CONSOLE_TYPE;		\
	return 0;				\
    }

#define CHECKDUMB(name,filp)     		\
   if (!strcmp(name,"dumb")) {   		\
       /* see fd/console.c */			\
	 filp->f_mode = S_IFCHR & 0644;		\
         filp->f_dev = DEV_DEVICE;		\
         filp->f_ino = DUMB_TYPE;		\
	 filp->f_pos = 0;			\
	 filp->f_flags = flags;			\
	 filp_refcount_init(filp);		\
	 filp_refcount_inc(filp);		\
	 filp->f_owner = __current->uid;	\
	 filp->op_type = DUMB_TYPE;		\
	return 0;				\
    }

#define CHECKNULL(name,filp)			\
    if (!strcmp(name,"null")) {			\
      /* see fd/null.c */			\
      filp->f_pos = 0;				\
      filp->f_dev = DEV_DEVICE;			\
      filp->f_ino = NULL_TYPE;			\
      filp->f_mode = S_IFCHR & 0644;		\
      filp_refcount_init(filp);			\
      filp_refcount_inc(filp);			\
      filp->f_owner = __current->uid;		\
      filp->op_type = NULL_TYPE;		\
      filp->f_flags = 0;			\
      return 0;					\
    }

#ifdef DEVTTY
#define CHECKTTY(name,filp)			\
    if (!strcmp(name,"tty")) {			\
      /* see fd/null.c */			\
      filp->f_pos = 0;				\
      filp->f_dev = DEV_DEVICE;			\
      filp->f_ino = INTERNAL_TTY_TYPE;		\
      filp->f_mode = S_IFCHR & 0666;		\
      filp_refcount_init(filp);			\
      filp_refcount_inc(filp);			\
      filp->f_owner = __current->uid;		\
      filp->op_type = NULL_TYPE;		\
      filp->f_flags = 0;			\
      return 0;					\
    }
#else
#define CHECKTTY(name,filp){ name = name ; filp = filp; }
#endif


#define CHECKSTDOUT(name,filp)			\
    if (!strcmp(name,"stdout")) {		\
      /* see fd/null.c */			\
      filp->f_pos = 0;				\
      filp->f_dev = DEV_DEVICE;		\
      filp->f_ino = INTERNAL_STDOUT_TYPE;	\
      filp->f_mode = S_IFCHR & 0666;		\
      filp_refcount_init(filp);			\
      filp_refcount_inc(filp);			\
      filp->f_owner = __current->uid;		\
      filp->op_type = NULL_TYPE;		\
      filp->f_flags = 0;			\
      return 0;					\
    }

#define CHECKDEVS(name,filp) \
  CHECKCONSOLE(name,filp); 			\
  CHECKDUMB(name,filp);				\
  CHECKNULL(name,filp);				\
  CHECKTTY(name,filp);				\
  CHECKSTDOUT(name,filp);

/* pty/pty.c major and minor. */
#define PTY_MAJOR_MASTER 3
#define PTY_MAJOR_SLAVE  2


/* npty/npty.c major and minor. */
#define NPTY_MAJOR_MASTER 1
#define NPTY_MAJOR_SLAVE  0
