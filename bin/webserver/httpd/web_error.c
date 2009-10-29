
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
#include <assert.h>

#include "web_general.h"
#include "web_config.h"
#include "web_error.h"


char *web_error_badreq = "<HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\nYour client sent a query that this server could not understand.<P>\nReason: Malformed request.<P>\n</BODY>\n";

char *web_error_unknownreq = "<HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\nYour client sent a query that this server could not understand.<P>\nReason: Invalid or unsupported method.<P>\n</BODY>\n";

char *web_error_toomanymimes = "<HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\nYour client sent a query that this server could not understand.<P>\nReason: Too many MIME (or other) headers.<P>\n</BODY>\n";

char *web_error_notfound = "<HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\nThe requested URL %s was not found on this server.<P>\n</BODY>\n";

char *web_error_poordirname = "<HEAD><TITLE>Document moved</TITLE></HEAD>\n<BODY><H1>Document moved</H1>\nThis document has moved <A HREF=\"http://%s/%s/\"> here</A>.<P>\n</BODY>\n";

char *web_error_movedtemp = "<HEAD><TITLE>Document moved</TITLE></HEAD>\n<BODY><H1>Document moved</H1>\nThis document has moved <A HREF=\"%s\"> here</A>.<P>\n</BODY>\n";

char *web_error_movedperm = "<HEAD><TITLE>Document moved</TITLE></HEAD>\n<BODY><H1>Document moved</H1>\nThis document has moved <A HREF=\"%s\"> here</A>.<P>\n</BODY>\n";


typedef struct {
   char *msg;
   int  len;
} errormsg_t;

errormsg_t errormsgs[(WEBREQ_ERROR_MAX+1)];


void web_error_printmsgs()
{
   int i;
   for (i=0; i<=WEBREQ_ERROR_MAX; i++) {
      printf ("error %d: len %d\n", i, errormsgs[i].len);
   }
}


void web_error_initmsgs ()
{
   errormsgs[WEBREQ_ERROR_BADREQ].msg = web_error_badreq;
   errormsgs[WEBREQ_ERROR_BADREQ].len = strlen(web_error_badreq);

   errormsgs[WEBREQ_ERROR_UNKNOWNREQ].msg = web_error_unknownreq;
   errormsgs[WEBREQ_ERROR_UNKNOWNREQ].len = strlen(web_error_unknownreq);

   errormsgs[WEBREQ_ERROR_TOOMANYMIMES].msg = web_error_toomanymimes;
   errormsgs[WEBREQ_ERROR_TOOMANYMIMES].len = strlen(web_error_toomanymimes);

       /* reduce len by 2 for the %s */
   errormsgs[WEBREQ_ERROR_NOTFOUND].msg = web_error_notfound;
   errormsgs[WEBREQ_ERROR_NOTFOUND].len = strlen(web_error_notfound) - 2;

       /* reduce len by 4 for the 2 %s's */
   errormsgs[WEBREQ_ERROR_POORDIRNAME].msg = web_error_poordirname;
   errormsgs[WEBREQ_ERROR_POORDIRNAME].len = strlen(web_error_poordirname) - 4;

       /* reduce len by 2 for the %s */
   errormsgs[WEBREQ_ERROR_MOVEDTEMP].msg = web_error_movedtemp;
   errormsgs[WEBREQ_ERROR_MOVEDTEMP].len = strlen(web_error_movedtemp) - 2;

       /* reduce len by 2 for the %s */
   errormsgs[WEBREQ_ERROR_MOVEDPERM].msg = web_error_movedperm;
   errormsgs[WEBREQ_ERROR_MOVEDPERM].len = strlen(web_error_movedperm) - 2;
/*
   web_error_printmsgs();
*/
}


int web_error_header (char *buf, int buflen, int errorval)
{
   int len;

   switch (errorval) {
      case WEBREQ_ERROR_BADREQ:
      case WEBREQ_ERROR_UNKNOWNREQ:
      case WEBREQ_ERROR_TOOMANYMIMES:
                           len = 25;
                           if (buflen > len) {
                              sprintf (buf, "HTTP/1.0 400 Bad Request%c", LF);
                           }
                           break;
      case WEBREQ_ERROR_NOTFOUND:
                           len = 23;
                           if (buflen > len) {
                              sprintf (buf, "HTTP/1.0 404 Not Found%c", LF);
                           }
                           break;
      case WEBREQ_ERROR_POORDIRNAME:
      case WEBREQ_ERROR_MOVEDTEMP:
                           len = 19;
                           if (buflen > len) {
                              sprintf (buf, "HTTP/1.0 302 Found%c", LF);
                           }
                           break;
      case WEBREQ_ERROR_MOVEDPERM:
                           len = 19;
                           if (buflen > len) {
                              sprintf (buf, "HTTP/1.0 301 Moved%c", LF);
                           }
                           break;
      default:
               printf ("Unknown error condition: %d\n", errorval);
               exit (0);
   }

   assert ((len == strlen (buf)) || (buflen <= len));
   return (len);
}


int web_error_messagelen (int errorval, char *docname)
{
   int len;

   if ((errorval < WEBREQ_ERROR_MIN) || (errorval >= WEBREQ_ERROR_MAX)) {
      printf ("Unknown error condition: %d\n", errorval);
      exit (0);
   }

   len = errormsgs[errorval].len;
   if (errorval == WEBREQ_ERROR_POORDIRNAME) {
      if (docname[0] == '/') {
         docname++;
      }
      len += ServerNameLen;
   }
   if (errorval >= WEBREQ_ERROR_1stINCLUDE) {
      len += strlen (docname);
   }

   return (len);
}


int web_error_message (char *buf, int buflen, int errorval, char *docname)
{
   int len;

   if ((errorval < WEBREQ_ERROR_MIN) || (errorval >= WEBREQ_ERROR_MAX)) {
      printf ("Unknown error condition: %d\n", errorval);
      exit (0);
   }

   len = errormsgs[errorval].len;
   if (errorval == WEBREQ_ERROR_POORDIRNAME) {
      if (docname[0] == '/') {
         docname++;
      }
      len += ServerNameLen;
   }
   if (errorval >= WEBREQ_ERROR_1stINCLUDE) {
      len += strlen (docname);
   }
   if (buflen > len) {
      if (errorval == WEBREQ_ERROR_POORDIRNAME) {
         sprintf (buf, errormsgs[errorval].msg, ServerName, docname);
      } else if (errorval >= WEBREQ_ERROR_1stINCLUDE) {
         sprintf (buf, errormsgs[errorval].msg, docname);
      } else {
         sprintf (buf, errormsgs[errorval].msg);
      }
      assert (len == strlen(buf));
   }
   return (len);
}

