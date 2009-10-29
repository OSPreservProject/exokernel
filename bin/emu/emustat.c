
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
#include <sys/stat.h>
#include "emustat.h"

int emu_stat(const char *path, struct stat *sb)
{
  int res;

  bzero(sb,sizeof(*sb)); 

  res = stat(path, sb);

  return res;
}

int emu_lstat(const char *path, struct stat *sb)
{
  int res;

  bzero(sb,sizeof(*sb)); 

  res = lstat(path, sb);

  if (sb->st_uid == -2) sb->st_uid = 0;

  return res;
}

int emu_fstat(int fd, struct stat *sb)
{
  int res;

  bzero(sb,sizeof(*sb)); 

  res = fstat(fd, sb);

  return res;

  /*
  kprintf("{ %x }",osb->st_dev);

  sb.st_dev = osb->st_dev;
  sb.st_ino = osb->st_ino;
  sb.st_mode = osb->st_mode;
  sb.st_nlink = osb->st_nlink;
  sb.st_uid = osb->st_uid;
  sb.st_gid = osb->st_gid;
  sb.st_rdev = osb->st_rdev;
  sb.st_size = osb->st_size;
  sb.st_atime = osb->st_atime;
  sb.st_spare1 = osb->st_atimensec;
  sb.st_mtime = osb->st_mtime;
  sb.st_spare2 = osb->st_mtimensec;
  sb.st_ctime = osb->st_ctime;
  sb.st_spare3 = osb->st_ctimensec;
  sb.st_blksize = osb->st_blksize;
  sb.st_blocks = osb->st_blocks;
  sb.st_gennum = osb->st_gen;
  sb.st_spare4 = osb->st_lspare;

  kprintf("{ %x }",sb.st_dev);
  
  res = fstat(fd, &sb);

  kprintf("{ %x }",sb.st_dev);

  osb->st_dev = sb.st_dev;
  osb->st_ino = sb.st_ino;
  osb->st_mode = sb.st_mode;
  osb->st_nlink = sb.st_nlink;
  osb->st_uid = sb.st_uid;
  osb->st_gid = sb.st_gid;
  osb->st_rdev = sb.st_rdev;
  osb->st_atime = sb.st_atime;
  osb->st_atimensec = sb.st_spare1;
  osb->st_mtime = sb.st_mtime;
  osb->st_mtimensec = sb.st_spare2;
  osb->st_ctime = sb.st_ctime;
  osb->st_ctimensec = sb.st_spare3;
  osb->st_size = sb.st_size;
  osb->st_blocks = sb.st_blocks;
  osb->st_blksize = sb.st_blksize;
  osb->st_flags = 0;
  osb->st_gen = sb.st_gennum;
  osb->st_lspare = sb.st_spare4;
  osb->st_qspare[0] = 0;
  osb->st_qspare[1] = 0;

  kprintf("{ %x }",osb->st_dev);
*/   
}
