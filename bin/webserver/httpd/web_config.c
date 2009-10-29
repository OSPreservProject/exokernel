
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
#include "web_general.h"
#include "web_config.h"
#include "web_alias.h"

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int ServerPort1 = 80;
int MaxConns = WEB_MAX_CONNS;

#define WEB_CONFIG_MAXSTRINGLEN	63
char ServerRoot[(WEB_CONFIG_MAXSTRINGLEN+1)];
char ErrorLog[(WEB_CONFIG_MAXSTRINGLEN+1)];
char TransferLog[(WEB_CONFIG_MAXSTRINGLEN+1)];
char AgentLog[(WEB_CONFIG_MAXSTRINGLEN+1)];
char RefererLog[(WEB_CONFIG_MAXSTRINGLEN+1)];
char ServerName[(WEB_CONFIG_MAXSTRINGLEN+1)];
char DocumentRoot[(WEB_CONFIG_MAXSTRINGLEN+1)];
char UserDir[(WEB_CONFIG_MAXSTRINGLEN+1)];
char DirectoryIndex[(WEB_CONFIG_MAXSTRINGLEN+1)];
char DefaultMimeType[(WEB_CONFIG_MAXSTRINGLEN+1)];
char MimeTypeFile[(WEB_CONFIG_MAXSTRINGLEN+1)];
char MimeEncodingFile[(WEB_CONFIG_MAXSTRINGLEN+1)];

int ServerNameLen = 0;
int DirectoryIndexLen = 0;

mime_t *MimeTypes = NULL;
int MimeTypeCount = 0;

mime_t *MimeEncodings = NULL;
int MimeEncodingCount = 0;

int mime_contenttype_html = -1;

static void web_config_mimetypes (char *filename, mime_t **types, int *typecount);


/* Function for reading an integer config value */

static int web_config_intparam (char *line, char *paramname, int *paramvalPtr)
{
   int len = strlen (paramname);
   int ret = 0;

   if (strncmp (line, paramname, len) == 0) {
      if (sscanf (&line[(len+1)], "%d", paramvalPtr) != 1) {
         printf ("Empty value supplied for %s\n", paramname);
         exit (0);
      }
      printf ("%s %d\n", paramname, *paramvalPtr);
      ret = 1;
   }
   return (ret);
}


/* Function for reading a string config value */

static int web_config_stringparam (char *line, char *paramname, char *paramval, int *paramlen)
{
   int len = strlen (paramname);
   int ret = 0;

   if (strncmp (line, paramname, len) == 0) {
      if (sscanf (&line[(len+1)], "%s", paramval) != 1) {
         printf ("Empty config value supplied for %s\n", paramname);
         exit (0);
      }
      if (strlen (paramval) > WEB_CONFIG_MAXSTRINGLEN) {
         printf ("Config value for %s is too long (must be less than %d characters)\n", paramname, WEB_CONFIG_MAXSTRINGLEN);
         exit (0);
      }
      *paramlen = strlen (paramval);
      printf ("%s %s\n", paramname, paramval);
      ret = 1;
   }
   return (ret);
}


/* initialize the config values with defaults.  Read overrides from the */
/* identified config file.                                              */

void web_config (char *filename)
{
   char line[256];
   char tmp[256];
   int found;
   int unused;
   int mainport = -1;
   int serverport = -1;
   FILE * configfile;

   if ((configfile = fopen (filename, "r")) == NULL) {
      printf ("Unable to open config file: %s\n", filename);
      exit (0);
   }

   ServerRoot[0] = (char) 0;
   ErrorLog[0] = (char) 0;
   TransferLog[0] = (char) 0;
   AgentLog[0] = (char) 0;
   RefererLog[0] = (char) 0;
   ServerName[0] = (char) 0;
   DocumentRoot[0] = '/';
   DocumentRoot[1] = (char) 0;
   UserDir[0] = (char) 0;
   DirectoryIndex[0] = (char) 0;
   DefaultMimeType[0] = (char) 0;
   MimeTypeFile[0] = (char) 0;
   MimeEncodingFile[0] = (char) 0;

   while (fgets (line, 255, configfile)) {
      if ((line[0] == '#') || (line[0] == '\n')) {
         continue;
      }
      found = 0;
      serverport = -1;
      found |= web_config_intparam (line, "ServerPort", &serverport);
      if (serverport != -1) {
         web_server_setServerPort (serverport);
         mainport = (mainport != 80) ? serverport : 80;
         ServerPort1 = -1;
      }
      found |= web_config_stringparam (line, "ServerRoot", ServerRoot, &unused);
      found |= web_config_stringparam (line, "ErrorLog", ErrorLog, &unused);
      found |= web_config_stringparam (line, "TransferLog", TransferLog, &unused);
      found |= web_config_stringparam (line, "AgentLog", AgentLog, &unused);
      found |= web_config_stringparam (line, "RefererLog", RefererLog, &unused);
      found |= web_config_stringparam (line, "ServerName", ServerName, &ServerNameLen);
      found |= web_config_stringparam (line, "UserDir", UserDir, &unused);
      found |= web_config_stringparam (line, "DirectoryIndex", DirectoryIndex, &DirectoryIndexLen);
      found |= web_config_stringparam (line, "DefaultMimeType", DefaultMimeType, &unused);
      if (found) continue;
      if (found |= web_config_stringparam (line, "MimeTypeFile", MimeTypeFile, &unused)) {
         web_config_mimetypes (MimeTypeFile, &MimeTypes, &MimeTypeCount);
      }
      if (found) continue;
      if (found |= web_config_stringparam (line, "MimeEncodingFile", MimeEncodingFile, &unused)) {
         web_config_mimetypes (MimeEncodingFile, &MimeEncodings, &MimeEncodingCount);
      }
      if (found) continue;
      if (found |= web_config_intparam (line, "MaxConns", &MaxConns)) {
         if (MaxConns > WEB_MAX_CONNS) {
            printf ("Config value for MaxConns (%d) exceeds the hard-coded upper limit (%d)\n", MaxConns, WEB_MAX_CONNS);
            exit (0);
         }
      }
      if (found) continue;
      if (found |= web_config_stringparam (line, "DocumentRoot", DocumentRoot, &unused)) {
         int len = strlen(DocumentRoot);
					/* Make sure it ends with a '/' */
         if (DocumentRoot[len] != '/') {
            if (len == WEB_CONFIG_MAXSTRINGLEN) {
               printf ("Config value for DocumentRoot is too long (must be less than %d characters)\n", (WEB_CONFIG_MAXSTRINGLEN-1));
               exit (0);
            }
            DocumentRoot[len] = '/';
            DocumentRoot[(len+1)] = (char) 0;
         }
         web_setDocumentRoot (DocumentRoot);
      }
      if (found) continue;
      if (found |= web_config_stringparam (line, "Alias", tmp, &unused)) {
         web_alias_configline (line);
         continue;
      }
      if (found == 0) {
         printf ("Unknown directive in config file: %s\n", line);
         exit (0);
      }
   }

   if (ServerPort1 != -1) {
      web_server_setServerPort (ServerPort1);
      mainport = (mainport != 80) ? ServerPort1 : 80;
      ServerPort1 = -1;
   }

   if (mainport != 80) {
      sprintf (&ServerName[ServerNameLen], ":%d", mainport);
      ServerNameLen = strlen (ServerName);
      if (ServerNameLen > WEB_CONFIG_MAXSTRINGLEN) {
         printf ("Config value ServerName becomes too long when ServerPort is appended\n");
         exit (0);
      }
   }

   fclose (configfile);
}


void web_config_mimetypes (char *filename, mime_t **types, int *countPtr)
{
   char line[256];
   int linecount = 0;
   int typecount = 0;
   int typeno;
   int i;
   FILE * mimefile;

   if ((mimefile = fopen (filename, "r")) == NULL) {
      printf ("Unable to open mimetypes file: %s\n", filename);
      exit (0);
   }

   while (fgets (line, 255, mimefile)) {
      linecount++;
      if ((line[0] == '#') || (line[0] == '\n')) {
         continue;
      }
      typecount++;
   }
   fseek (mimefile, 0, 0);

   *countPtr = typecount;
   *types = (mime_t *) malloc (typecount * sizeof(mime_t));

   typeno = 0;
   for (i=0; i<linecount; i++) {
      if (fgets (line, 255, mimefile) == 0) {
         printf ("web_config_mimetypes: Non-matching values for i and linecount: %d != %d\n", i, linecount);
         exit (0);
      }
      if ((line[0] == '#') || (line[0] == '\n')) {
         continue;
      }
      if (sscanf (line, "%s", (*types)[typeno].type) != 1) {
         printf ("Invalid value for mime type %d: %s\n", typeno, line);
         exit (0);
      }
      if (strlen ((*types)[typeno].type) >= sizeof(mime_t)) {
         printf ("Mime type is too large (%d characters): %s\n", strlen((*types)[typeno].type), (*types)[typeno].type);
         exit (0);
      }
      if (strcmp ((*types)[typeno].type, "text/html") == 0) {
         mime_contenttype_html = typeno;
      }
      typeno++;
   }
   if (typeno != typecount) {
      printf ("web_config_mimetypes: Non-matching values for typeno and typecount: %d != %d\n", typeno, typecount);
      exit (0);
   }
   if (mime_contenttype_html == -1) {
      sprintf ((*types)[typeno].type, "text/html");
      mime_contenttype_html = typeno;
      typeno++;
      typecount++;
   }

   fclose (mimefile);
/*
   for (i=0; i<typecount; i++) {
      printf ("Mime type %d: %s\n", i, (*types)[i].type);
   }
*/
}

