
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


#ifndef __WEB_ERROR_H__
#define __WEB_ERROR_H__

#define WEBREQ_ERROR_MIN		0
#define WEBREQ_ERROR_BADREQ		0
#define WEBREQ_ERROR_UNKNOWNREQ		1
#define WEBREQ_ERROR_TOOMANYMIMES	2
#define WEBREQ_ERROR_1stINCLUDE		3
#define WEBREQ_ERROR_NOTFOUND		3
#define WEBREQ_ERROR_POORDIRNAME	4
#define WEBREQ_ERROR_MOVEDTEMP		5
#define WEBREQ_ERROR_MOVEDPERM		6
#define WEBREQ_ERROR_MAX		6
/*
#define WEBREQ_ERROR_POORDIRNAME        1
#define WEBREQ_ERROR_UNKNOWNREQ         2
#define WEBREQ_ERROR_TOOMANYMIMES       3
#define WEBREQ_ERROR_MOVEDPERM          301
#define WEBREQ_ERROR_MOVEDTEMP          302
#define WEBREQ_ERROR_BADREQ             400
#define WEBREQ_ERROR_NOTFOUND           404
*/

/* web_error.c prototypes */

void web_error_initmsgs ();
int web_error_header (char *buf, int buflen, int errorval);
int web_error_message (char *buf, int buflen, int errorval, char *docname);
int web_error_messagelen (int errorval, char *docname);

#endif  /* __WEB_ERROR_H__ */

