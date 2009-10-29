
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
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <memory.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#include <unistd.h>
#include <stdlib.h>

#include "web_general.h"
#include "web_datetime.h"

#ifndef EXOPC
typedef unsigned char	uint8;
typedef unsigned short	uint16;
typedef unsigned int	uint32;
#endif

#ifdef HIGHLEVEL
#define inet_checksum(a,b,c,d)	0
#else
#include <exos/netinet/cksum.h>
#endif

/* GROK -- all of this probably provides little to no real value... */


char datetime[31] = "Sun, 00 Jan 1970 00:00:00 GMT\n";
int datetimelen = 30;
int datetimeoff = 36;	/* offset of Date string in pre-computed header */
int datetimesum = 0;


#define DAYOFWEEK0	0
#define DAYOFWEEK1	(DAYOFWEEK0 + 1)
#define DAYOFWEEK2	(DAYOFWEEK0 + 2)
#define DAYOFMONTH1	5
#define DAYOFMONTH2	(DAYOFMONTH1 + 1)
#define MONTH0		8
#define MONTH1		(MONTH0 + 1)
#define MONTH2		(MONTH0 + 2)
#define YEAR1		12
#define YEAR2		(YEAR1 + 1)
#define YEAR3		(YEAR1 + 2)
#define YEAR4		(YEAR1 + 3)
#define HOUR1		17
#define HOUR2		(HOUR1 + 1)
#define MINUTE1		20
#define MINUTE2		(MINUTE1 + 1)
#define SECOND1		23
#define SECOND2		(SECOND1 + 1)

/* ASCII representations of digits */

#define ASCIIDIGIT0	48
#define ASCIIDIGIT1	(ASCIIDIGIT0 + 1)
#define ASCIIDIGIT2	(ASCIIDIGIT0 + 2)
#define ASCIIDIGIT3	(ASCIIDIGIT0 + 3)
#define ASCIIDIGIT4	(ASCIIDIGIT0 + 4)
#define ASCIIDIGIT5	(ASCIIDIGIT0 + 5)
#define ASCIIDIGIT6	(ASCIIDIGIT0 + 6)
#define ASCIIDIGIT7	(ASCIIDIGIT0 + 7)
#define ASCIIDIGIT8	(ASCIIDIGIT0 + 8)
#define ASCIIDIGIT9	(ASCIIDIGIT0 + 9)

/* help structures for construcing datetime */

typedef struct month {
   char str[4];
   int days;
   int leapday;
} month_t;
month_t months[] = {
        {"Jan", 31, 0},
        {"Feb", 28, 1},
        {"Mar", 31, 0},
        {"Apr", 30, 0},
        {"May", 31, 0},
        {"Jun", 30, 0},
        {"Jul", 31, 0},
        {"Aug", 31, 0},
        {"Sep", 30, 0},
        {"Oct", 31, 0},
        {"Nov", 30, 0},
        {"Dec", 31, 0},
};
char *days = "Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat";

/* components of datetime */

int year;
int dayofweek;
int monthno;
int dayofmonth;
int hour;
int minute;
int second;

time_t lasttime;

#define incrmod(x,y) ((((x)+1) == (y)) ? 0 : ((x)+1))
#define leapyear(x) ((((x) & 0x00000003) == 0) && ((x) % 1000))

void web_datetime_init ()
{
   char buffer[28];
   int ret;

   (void)setenv("TZ", "GMT0", 1);
   if (time(&lasttime) == -1) {
      printf ("web_datetime_init: error getting time()\n");
      exit(0);
   }
   (void)strftime(buffer, 26, "%a %b %e %H:%M:%S %Y", localtime(&lasttime));
   buffer[26] = 0;
   printf("Start date/time: %s\n", buffer);

/*
   assert (sscanf (buffer, "%d %d:%d:%d %d", &dayofmonth, &hour, &minute, &second, &year) == 5);
*/
   ret = sscanf (&buffer[8], "%d %d:%d:%d %d", &dayofmonth, &hour, &minute, &second, &year);
   if (ret != 5) {
      printf ("web_datetime_init: only %d of 5 arguments from sscanf\n", ret);
      printf ("%d %d:%d:%d %d\n", dayofmonth, hour, minute, second, year);
      exit (0);
   }
   dayofweek = 0;
   while ((dayofweek < 7) && (bcmp (&buffer[0], &days[(dayofweek << 2)], 3) != 0)) {
      dayofweek++;
   }
   assert (dayofweek < 7);
   monthno = 0;
   while ((monthno < 12) && (bcmp (&buffer[4], months[monthno].str, 3) != 0)) {
      monthno++;
   }
   assert (monthno < 12);

   /* Adjust EST to GMT */
   hour += 5;
   if (hour > 23) {
      hour -= 24;
      dayofweek = incrmod (dayofweek, 7);
      dayofmonth++;
      if (dayofmonth >= months[monthno].days) {
         if ((months[monthno].leapday == 0) || (dayofmonth > months[monthno].days) || (leapyear(year) == 0)) {
            dayofmonth = 0;
            monthno = incrmod (monthno, 12);
            if (monthno == 0) {
               year++;
            }
         }
      }
   }

   /* Initialize datetime */;
   sprintf (datetime, "%s, %02d %s %4d %02d:%02d:%02d GMT\n", &days[(dayofweek << 2)], dayofmonth, months[monthno].str, year, hour, minute, second);
   assert (strlen(datetime) == datetimelen);

   datetimesum = inet_checksum ((unsigned short *) datetime, datetimelen, 0, 0);
}


#define twodigitupdate(dig1,dig2) \
				if (datetime[(dig2)] == ASCIIDIGIT9) { \
				   datetime[(dig1)]++; \
				   datetime[(dig2)] = ASCIIDIGIT0; \
				} else { \
				   datetime[(dig2)]++; \
				}

#define twodigitzero(dig1,dig2) \
				datetime[(dig1)] = ASCIIDIGIT0; \
				datetime[(dig2)] = ASCIIDIGIT0;

int web_datetime_update ()
{
   time_t curtime;
   int tmpval;
   int secs;

   if ((curtime = time(NULL)) == -1) {
      printf ("web_datetime_update: time() failed\n");
      exit (0);
   }

   if (curtime < lasttime) {
      kprintf ("web_datetime_update: curtime %d, lasttime %d\n", (uint)curtime, (uint)lasttime);
   }
   assert (curtime >= lasttime);
   secs = curtime - lasttime;
   lasttime = curtime;
/*
   if (secs >= 60) {
      printf ("web_datetime_update: too many secs (%d) have passed\n", secs);
      exit (0);
   }
*/
   tmpval = secs;
   while (tmpval > 0) {
      tmpval--;
      second = incrmod (second, 60);
      if (second != 0) {
         twodigitupdate (SECOND1, SECOND2);
      } else {
         twodigitzero (SECOND1, SECOND2);
         minute = incrmod (minute, 60);
         if (minute != 0) {
            twodigitupdate (MINUTE1, MINUTE2);
         } else {
            twodigitzero (MINUTE1, MINUTE2);
            hour = incrmod (hour, 24);
            if (hour != 0) {
               twodigitupdate (HOUR1, HOUR2);
            } else {
               twodigitzero (HOUR1, HOUR2);
               dayofweek = incrmod (dayofweek, 7);
               datetime[DAYOFWEEK0] = days[(dayofweek << 2)];
               datetime[DAYOFWEEK1] = days[((dayofweek << 2)+1)];
               datetime[DAYOFWEEK2] = days[((dayofweek << 2)+2)];
               dayofmonth++;
               if (dayofmonth >= months[monthno].days) {
                  if ((months[monthno].leapday == 0) || (dayofmonth > months[monthno].days) || (leapyear(year) == 0)) {
                     dayofmonth = 0;
                     monthno = incrmod (monthno, 12);
                     datetime[MONTH0] = months[monthno].str[0];
                     datetime[MONTH1] = months[monthno].str[1];
                     datetime[MONTH2] = months[monthno].str[2];
                     if (monthno == 0) {
                        year++;
                        if (datetime[YEAR4] == ASCIIDIGIT9) {
                           datetime[YEAR4] = ASCIIDIGIT0;
                           if (datetime[YEAR3] == ASCIIDIGIT9) {
                              datetime[YEAR3] = ASCIIDIGIT0;
                              if (datetime[YEAR2] == ASCIIDIGIT9) {
                                 datetime[YEAR2] = ASCIIDIGIT0;
                                 datetime[YEAR1]++;
                              } else {
                                 datetime[YEAR2]++;
                              }
                           } else {
                              datetime[YEAR3]++;
                           }
                        } else {
                           datetime[YEAR4]++;
                        }
                     }
                  }
               }
               if (dayofmonth == 0) {
                  datetime[DAYOFMONTH1] = ASCIIDIGIT0;
                  datetime[DAYOFMONTH2] = ASCIIDIGIT1;
               } else {
                  twodigitupdate (DAYOFMONTH1, DAYOFMONTH2);
               }
            }
         }
      }
/*
printf ("updated datetime: %s", datetime);
*/
      datetimesum = inet_checksum ((unsigned short *) datetime, datetimelen, 0, 0);
   }
   return (secs);
}

