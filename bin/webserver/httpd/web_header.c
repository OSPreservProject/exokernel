
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
#include <string.h>

#include <assert.h>

#if 1
#define DPRINTF(a,b)	if ((a) < 0) { printf b; }
#else
#define DPRINTF(a,b)
#endif

#include "web_general.h"
#include "web_config.h"
#include "web_header.h"


int web_header_goodstatus (char *buf, int buflen) {
   if (buflen > 30) {
      sprintf (buf, "HTTP/1.0 200 Document follows%c", LF);
      assert (30 == strlen(buf));
   }
   return (30);
}


char *http_date_format = "%a, %d %b %Y %T GMT";

int web_header_date (char *buf, int buflen)
{
   char datebuf[64];
   int len;

   sprintf (datebuf, "Sun, 10 Dec 1995 23:30:58 GMT");
   len = 7 + strlen(datebuf);
   if (buflen > len) {
      sprintf (buf, "Date: %s%c", datebuf, LF);
      assert (strlen(buf) == len);
   }
   return (len);
}


int web_header_server (char *buf, int buflen)
{
   if (buflen > 26) {
      sprintf (buf, "Server: Exo-Webserver/0.0%c", LF);
      assert (26 == strlen(buf));
   }
   return (26);
}


int web_header_contenttype (char *buf, int buflen, int type)
{
   char *mimetype;
   int len;

   if (type >= MimeTypeCount) {
      printf ("Unexpected MIME type encountered: %d\n", type);
      type = -1;
   }
   mimetype = (type == -1) ? DefaultMimeType : MimeTypes[type].type;
DPRINTF (2, ("mime type %d, mimetype %s\n", type, mimetype));
   len = 15 + strlen (mimetype);
   if (buflen > len) {
      sprintf (buf, "Content-type: %s%c", mimetype, LF);
      assert (len == strlen(buf));
   }
   return (len);
}


int web_header_contentencoding (char *buf, int buflen, int type)
{
   char *mimeencoding;
   int len = 0;

   if (type >= MimeEncodingCount) {
      printf ("Unexpected MIME type encountered: %d\n", type);
      return (0);
   }

   mimeencoding = MimeEncodings[type].type;
   len = 19 + strlen (mimeencoding);
   if (buflen > len) {
      sprintf (buf, "Content-encoding: %s%c", mimeencoding, LF);
      assert (len == strlen(buf));
   }
   return (len);
}


int web_header_contentlength (char *buf, int buflen, unsigned int filelen)
{
   char lenbuf[32];
   int len;

   sprintf (lenbuf, "%d", filelen);
   len = 17 + strlen(lenbuf);
   if (buflen > len) {
      sprintf (buf, "Content-length: %s%c", lenbuf, LF);
      assert (strlen(buf) == len);
   }
   return (len);
}


int web_header_lastmodified (char *buf, int buflen, time_t lastmod)
{
/*
   char datebuf[64];
   struct tm *tms;
   int len;

   tms = gmtime(&lastmod);
   strftime (datebuf, 64, http_date_format, tms);
   len = 16 + strlen(datebuf);
   if (buflen > len) {
      sprintf (buf, "Last-modified: %s%c", datebuf, LF);
      assert (strlen(buf) == len);
   }
   return (len);
*/
   return (0);
}


int web_header_location (char *buf, int buflen, char *docname, int partial)
{
   int len = (partial) ? 19 : 11;

   if (partial) {
      if (docname[0] == '/') {
         docname++;
      }
      len += ServerNameLen;
   }
   len += strlen (docname);
   if (buflen > len) {
      if (partial) {
         sprintf (buf, "Location: http://%s/%s%c", ServerName, docname, LF);
      } else {
         sprintf (buf, "Location: %s%c", docname, LF);
      }
      assert (strlen(buf) == len);
   }

   return (len);
}


int web_header_end (char *buf, int buflen)
{
   if (buflen > 1) {
      sprintf (buf, "%c", LF);
      assert (1 == strlen(buf));
   }
   return (1);
}


