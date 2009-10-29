
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
#include "web_alias.h"
#include "web_general.h"

#ifndef HIGHLEVEL
#include "web_fs.h"
#endif

#if 1
#define DPRINTF(a,b)	if ((a) < 0) { printf b; }
#else
#define DPRINTF(a,b)
#endif

#include <memory.h>
#include <string.h>

#ifndef HIGHLEVEL
extern dinode_t *DocumentRootInode;
extern int DocumentRootInodeNum;
#endif

typedef struct {
   int inodenum;
   int dirInodenum;
   int namelen;
   char name[244];
} alias_t;

#define WEB_MAX_NUMALIASES	32
static alias_t aliases[WEB_MAX_NUMALIASES];
static int numaliases = 1;


int web_alias_add (int dirInodenum, char *name, int namelen, int inodenum)
{
   if (numaliases == WEB_MAX_NUMALIASES) {
      printf ("Too many aliases (and alias components)\n");
      exit (0);
   }

   if (namelen >= 243) {
      name[namelen] = (char) 0;
      printf ("Alias name component too long: %s\n", name);
      exit (0);
   }

   aliases[numaliases].inodenum = inodenum;
   aliases[numaliases].dirInodenum = dirInodenum;
   aliases[numaliases].namelen = namelen;
   bcopy (name, aliases[numaliases].name, namelen);
   aliases[numaliases].name[namelen] = (char) 0;

printf ("web_alias_add: root %d, name %s, namelen %d, inodenum %d\n", dirInodenum, aliases[numaliases].name, namelen, inodenum);

   numaliases++;
   return (-numaliases);
}


void web_alias_configline (char *line)
{
#ifndef HIGHLEVEL
   char fake[256];
   char real[256];
   dinode_t *inode;
   dinode_t *tmpinode = DocumentRootInode;
   char *currpoint = fake;
   char *endoffake;
   int complen = 0;
   int inodenum = DocumentRootInodeNum;
   int lastnum = DocumentRootInodeNum;
   char *lastpoint = currpoint;
   int lastlen = 0;

   if (sscanf (line, "Alias %s %s", fake, real) != 2) {
      printf ("Bad format for Alias config line: %s\n", line);
      exit (0);
   }
   endoffake = fake + strlen(fake);
   if (((endoffake - fake) > 255) || (strlen(real) > 255)) {
      printf ("Oversized name(s) in Alias config line: %s\n", line);
      exit (0);
   }

   inode = web_fs_translateNameToInode (real, alfs_FSdev, alfs_currentRootInode, 0);
   if (inode == NULL) {
      printf ("Real name (%s) for Alias does not exist\n", real);
      return;
   }

   while (currpoint < endoffake) {

      lastnum = inodenum;
      currpoint = web_fs_path_getNextPathComponent (currpoint, &complen);

      if (complen == 0) {
         break;
      }

      if (tmpinode) {
         inodenum = web_fs_lookupname (alfs_FSdev, tmpinode, currpoint, complen, BUFFER_READ);
         if (tmpinode != DocumentRootInode) {
            web_fs_releaseDInode (tmpinode, alfs_FSdev, 0);
         }
         if (inodenum > 0) {
            tmpinode = web_fs_getDInode (inodenum, alfs_FSdev, BUFFER_READ);
         } else {
            tmpinode = NULL;
         }
      }
      if (tmpinode == NULL) {
         inodenum = web_alias_add (lastnum, currpoint, complen, -(numaliases+1));
      }
      lastpoint = currpoint;
      lastlen = complen;
      currpoint += complen;
   }
   if (tmpinode) {
      web_alias_add (lastnum, currpoint, complen, inode->dinodeNum);
      if (tmpinode != DocumentRootInode) {
         web_fs_releaseDInode (tmpinode, alfs_FSdev, 0);
      }
   } else {
      aliases[(numaliases-1)].inodenum = inode->dinodeNum;
   }
#endif
}


int web_alias_match (int dirInodenum, char *name, int namelen)
{
#ifndef HIGHLEVEL
   int i;

DPRINTF (2, ("web_alias_match: root %d, name %s, namelen %d", dirInodenum, name, namelen));
   for (i=0; i<numaliases; i++) {
      if ((dirInodenum == aliases[i].dirInodenum) && (namelen == aliases[i].namelen) && (bcmp(name, aliases[i].name, namelen) == 0)) {
DPRINTF (2, (", result %d\n", aliases[i].inodenum));
         return (aliases[i].inodenum);
      }
   }
DPRINTF (2, (", result -1\n"));
#endif
   return (-1);
}


void web_alias_setDocumentRoot (int newnum, int oldnum)
{
   int i;

   for (i=1; i<numaliases; i++) {
      if (aliases[i].dirInodenum == oldnum) {
         aliases[i].dirInodenum = newnum;
      }
   }

   aliases[0].dirInodenum = newnum;
   aliases[0].namelen = 2;
   aliases[0].name[0] = '/';
   aliases[0].name[1] = (char) 0;
   aliases[0].inodenum = newnum;
}

