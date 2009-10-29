
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



#include <sys/types.h>
#include <stdio.h>

#include <sys/tick.h>

#include "../web_datetime.h"


int delay (iters)
int iters;
{
   int i;
   int crap = 0;

   for (i=0; i<iters; i++) {
      crap += i / 6;
      if ((crap % 14) == 0) {
         crap -= i / 5;
      }
   }
/*
   printf ("poll again: %d\n", crap);
*/
   return (crap);
}


typedef struct month {
   char str[4];
   int days;
   int leapday;
} month_t;
extern month_t months[];
extern char *days;
extern int year;
extern int dayofweek;
extern int monthno;
extern int dayofmonth;
extern int hour;
extern int minute;
extern int second;
extern int usec;

extern int lasttick;
extern int tickrate;


int main (int argc, char *argv[]) {
   int iters = 250000;
   int updates = 10;
   unsigned int crap = 0;
   web_datetime_init ();
/*
   year = 2000;
   dayofweek = 3;
   monthno = 1;
   dayofmonth = 28;
   hour = 23;
   minute = 59;
   second = 57;
   usec = 0;
   lasttick = ae_gettick ();
   tickrate = ae_getrate ();
*/
   sprintf (datetime, "%s, %02d %s %4d %02d:%02d:%02d GMT", &days[(dayofweek << 2)], dayofmonth, months[monthno].str, year, hour, minute, second);


   while (updates) {
      updates--;
      web_datetime_update ();
      printf ("datetime: %s\n", datetime);
      crap |= delay (iters);
   }

   printf ("done (crap %d)\n", crap);

   return 1;
}
