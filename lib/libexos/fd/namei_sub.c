
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

#undef PRINTF_LEVEL
#define PRINTF_LEVEL -1

#ifdef EXOPC
#include <fd/proc.h>
#endif /* EXOPC */
#include <string.h>
#include <exos/debug.h>
#include <assert.h>

#ifndef EXOPC
#define NAME_MAX 255
#define PATH_MAX 1024

int main(int argc, char **argv) {
  char next[NAME_MAX];
  char *path;
  argv++;
  while(*argv) {
    path = *argv;
    printf("TRAVERSE PATH; %s\n",path);
    while(*path) {
      printf("path before: %s\n",path);
      path = getNextComponent(path,next);
      printf("name: %s\nnext: %s\n\n",path,next);
    }
    argv++;
  }
  return 0;
}
#endif /* !EXOPC */

/* getNextComponent

   Strip off the leftmost component of a pathname and returns it

   path is the pathname
   next is buffer to put leftmore component of path into

   returns path minus leftmore component
 */



const char *
getNextComponent (const char *path, char *next) {
     int count_name = 0;
     const char *pathB = path;
#ifdef PRINTF_LEVEL
     char *next0 = next;
     const char *path0 = path;
#endif
     /* int count_path = 0; */

     /* strip leading slashes, if any */
     while (*path == '/' && *path != (char)NULL) {
          path++;
     }

     /* copy over chars to next */
     while (*path != '/' && *path != (char)NULL && count_name < NAME_MAX) {
          *next++ = *path++;
          count_name++;
     }
     
     /* check to make sure nobody's using names that are too long */
     if (count_name > NAME_MAX) printf("name longer than maximum\n");
     /* and terminate next */
     if (pathB != path && count_name == 0) {
       next[0] = '.';
       next[1] = 0;
     } else {
       *next = (char)NULL;
     }
     /* while(*path == '/') path++;*/

     DPRINTF(CLU_LEVEL,("getNextComponent: 
               path: \"%s\"
               next: \"%s\"
               left: \"%s\" c: %d\n",
			path0,next0,path,count_name));
     return (path);
}

/* SplitPath */
void 
SplitPath(const char *path, char *suffix, char *prefix)
{
  int seen = 0;
  char *temp,*temp2,*temp3;
  char buffer[PATH_MAX];

  DPRINTF(CLU_LEVEL,("SplitPath: %s\n",path));

  temp = buffer;
  if (path[0] == '/' && path[1] == (char)0) {
    prefix[0] = '/';
    suffix[0] = '.';
    prefix[1] = suffix[1] = (char)0;
    return;
  }
  /* copy over chars to next */
  while (*path != (char)NULL) {
    
    if (*path == '/')

      {if (seen){
	path++;
      } else {    
          *temp++ = *path++;
	  seen = 1;}

     } else {
	*temp++ = *path++;
	seen = 0;}
  }

  if (temp != buffer)
    {
      if (temp[-1] == '/') {*--temp = (char)NULL;}
      else {*temp = (char)NULL;}
      
      temp2 = temp;

      while(*temp != '/' && temp != buffer - 1) {temp--;}
      temp3 = ++temp;
      while(temp != temp2) {*suffix++ = *temp++;}
      *suffix = (char)NULL;
      temp = buffer;
      while(temp != temp3) {*prefix++ = *temp++;}
      *prefix = (char)NULL;


      return;

    } 
  else {*suffix = (char)NULL; *prefix = (char)NULL; return;}
}
