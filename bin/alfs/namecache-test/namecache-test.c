
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


#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "alfs/name_cache.h"

void main ()
{
   nc_t *namecache;
   int value;
   int ret;

   printf("sizeof(nc_t) == %d\n", sizeof(nc_t));
   printf("sizeof(nc_entry_t) == %d\n", sizeof(nc_entry_t));
   namecache = name_cache_init (176, 2);

   name_cache_printContents (namecache);

   name_cache_addEntry (namecache, 1, "hello", 5, 42);

   name_cache_printContents (namecache);

   ret = name_cache_findEntry (namecache, 1, "hello", 5, &value);
   assert ((ret == 1) && (value == 42));

   printf("found simple entry\n");

   ret = name_cache_findEntry (namecache, 1, "hello", 4, &value);
   assert (ret == 0);
   ret = name_cache_findEntry (namecache, 1, "hello", 6, &value);
   assert (ret == 0);
   ret = name_cache_findEntry (namecache, 2, "hello", 5, &value);
   assert (ret == 0);
   ret = name_cache_findEntry (namecache, 1, "hellp", 5, &value);
   assert (ret == 0);

   printf("didn't find it incorrectly\n");
   name_cache_printContents (namecache);

   name_cache_removeEntry (namecache, 1, "hello", 4);
   name_cache_removeEntry (namecache, 1, "hello", 6);
   name_cache_removeEntry (namecache, 2, "hello", 5);
   name_cache_removeEntry (namecache, 1, "hellp", 5);

   name_cache_printContents (namecache);
   printf("was it removed incorrectly??\n");

   name_cache_removeEntry (namecache, 1, "hello", 5);

   name_cache_printContents (namecache);
   printf("now it should be gone\n");

   ret = name_cache_findEntry (namecache, 1, "hello", 5, &value);
   assert (ret == 0);

   printf("didn't find removed entry\n");

   name_cache_addEntry (namecache, 1, "hello", 5, 42);

   name_cache_printContents (namecache);

   name_cache_addEntry (namecache, 3, "hello", 5, 42);

   name_cache_printContents (namecache);

   name_cache_addEntry (namecache, 2, "hello", 5, 42);

   name_cache_printContents (namecache);

   name_cache_addEntry (namecache, 2, "hello", 5, 42);

   name_cache_printContents (namecache);

   name_cache_addEntry (namecache, 2, "hello", 5, 43);

   name_cache_printContents (namecache);

   name_cache_removeEntry (namecache, 2, "hello", 5);

   name_cache_printContents (namecache);

   ret = name_cache_findEntry (namecache, 1, "hello", 5, &value);
   printf("find 1 hello: ret %d, value %d\n", ret, value);
   assert (ret == 1);
   ret = name_cache_findEntry (namecache, 3, "hello", 5, &value);
   printf("find 3 hello: ret %d, value %d\n", ret, value);
   assert (ret == 1);
   ret = name_cache_findEntry (namecache, 2, "hello", 5, &value);
   printf("find 2 hello: ret %d, value %d\n", ret, value);
   assert (ret == 0);

   name_cache_addEntry (namecache, 5, "hello", 5, 47);

   name_cache_printContents (namecache);

   name_cache_addEntry (namecache, 7, "he lo", 4, 49);

   name_cache_printContents (namecache);

   ret = name_cache_findEntry (namecache, 1, "he lo", 4, &value);
   assert (ret == 0);

   name_cache_addEntry (namecache, 7, "hellothereworldhowyadoin", 24, 49);

   name_cache_printContents (namecache);

   name_cache_removeID (namecache, 7);

   name_cache_printContents (namecache);

   name_cache_addEntry (namecache, 7, "he lo", 4, 49);

   name_cache_printContents (namecache);

   name_cache_removeValue (namecache, 49);

   name_cache_printContents (namecache);

   name_cache_shutdown(namecache);

   name_cache_addEntry (namecache, 6, "hello", 5, 67);
   name_cache_addEntry (namecache, 7, "hello", 5, 77);

   name_cache_printContents (namecache);

   name_cache_addEntry (namecache, 8, "hello", 5, 87);

   name_cache_printContents (namecache);

   name_cache_addEntry (namecache, 6, "hello", 5, 87);

   name_cache_printContents (namecache);

   name_cache_removeValue (namecache, 87);

   name_cache_printContents (namecache);

   name_cache_printStats (namecache);

   printf ("Done\n");
   exit (0);
}

