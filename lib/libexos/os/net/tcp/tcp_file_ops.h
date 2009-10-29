
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


struct file_ops tcp_file_ops = {
    NULL,			/* open */
    NULL,			/* lseek */
    tcpsocket_read,		/* read */
    tcpsocket_write,		/* write */
    tcpsocket_select,		/* select */
    tcpsocket_select_pred,	/* select_pred */
    tcpsocket_ioctl,		/* ioctl */
    NULL,			/* close0 */
    tcpsocket_close,		/* close */
    NULL,			/* lookup */
    NULL,			/* link */
    NULL,			/* symlink */
    NULL,			/* unlink */
    NULL,			/* mkdir */
    NULL,			/* rmdir */
    NULL,			/* mknod */
    NULL,			/* rename */
    NULL,			/* readlink */
    NULL,			/* follow_link */
    NULL,			/* truncate */
    NULL,			/* dup */
    NULL,			/* release */
    NULL,			/* acquire */
    tcpsocket_bind,		/* bind */
    tcpsocket_connect,		/* connect */
    NULL,			/* filepair */
    tcpsocket_accept,		/* accept */
    tcpsocket_getname,		/* getname */
    tcpsocket_listen,		/* listen */
    tcpsocket_send,	        /* send */
    tcpsocket_recv,		/* recv */
    NULL,			/* sendto */
    NULL,		        /* recvfrom */
    tcpsocket_shutdown,		/* shutdown */
    tcpsocket_setsockopt,	/* setsockopt */
    tcpsocket_getsockopt,	/* getsockopt */
    tcpsocket_fcntl,		/* fcntl */
    NULL,			/* mount */
    NULL,			/* unmount */
    NULL,			/* chmod */
    NULL,			/* chown */
    NULL,			/* stat */
    NULL,			/* readdir */
    NULL,			/* utimes */
    NULL,			/* bmap */
    NULL,			/* fsync */
    NULL			/* exithandler */
};
