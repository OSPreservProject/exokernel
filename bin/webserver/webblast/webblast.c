
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
#include <unistd.h>
#include <sys/types.h>

//#include <exos/tick.h>

#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>

extern char *get_ip_from_name(const char *name);

static int portno = 80;

#define PAGESIZ 4096

static int fd;

static int numconns = 0;
static int MaxConns = 1;
static int *connfds = NULL;

static char pagebuf[32768+1];
static char buf[8192];

static char *servername = NULL;
static char *ip_addr = NULL;
static struct sockaddr_in connaddr;

typedef struct urlptr {
   struct urlptr *next;
   int len;
   char line[(512 - sizeof(int) - sizeof (struct urlptr *))];
} urlptr_t;

urlptr_t *urlptr_extra = NULL;

static void urlptr_allocateextra()
{
   urlptr_t *temp;
   int i;

   if ((temp = (urlptr_t *) malloc(PAGESIZ)) == NULL) {
      printf("Unable to allocate space for urlptr_t's\n");
      exit(0);
   }
   for (i=0; i<((PAGESIZ/sizeof(urlptr_t))-1); i++) {
      temp[i].next = &temp[i+1];
   }
   temp[((PAGESIZ/sizeof(urlptr_t))-1)].next = NULL;
   urlptr_extra = temp;
}


static void free_urlptr(temp)
urlptr_t *temp;
{
   temp->next = urlptr_extra;
   urlptr_extra = temp;
}


static urlptr_t * get_urlptr()
{
   urlptr_t *temp = urlptr_extra;

   if (temp == NULL) {
      urlptr_allocateextra();
      temp = urlptr_extra;
   }
   urlptr_extra = urlptr_extra->next;
   temp->next = NULL;
   return(temp);
}


void request_url (urlptr)
urlptr_t *urlptr;
{
   int newfd;
   int i;

   if ((newfd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
      fprintf (stderr, "Unable to create the main server TCP socket\n");
      exit(0);
   }

   bzero ((char *) &connaddr, sizeof(connaddr));
   connaddr.sin_family = AF_INET;
   connaddr.sin_addr.s_addr = *((uint *) ip_addr);
   connaddr.sin_port = htons (portno);

   if (connect (newfd, (struct sockaddr *) &connaddr, sizeof(connaddr)) == -1) {
      perror ("cannot establish connection to server\n");
      exit(0);
   }

   if (write (newfd, urlptr->line, urlptr->len) != urlptr->len) {
      printf ("failed to write entire include request\n");
      exit (0);
   }

   for (i=0; i<MaxConns; i++) {
      if (connfds[i] == -1) {
         connfds[i] = newfd;
         break;
      }
   }
   assert (i != MaxConns);
   numconns++;
}


int string_compare (str1, str2, len)
char *str1;
char *str2;
int len;
{
   int i;
   char c1, c2;

   for (i=0; i<len; i++) {
      c1 = str1[i];
      c2 = str2[i];
      if ((c1 >= 65) && (c1 <= 90)) {
         c1 += 32;
      }
      if ((c2 >= 65) && (c2 <= 90)) {
         c2 += 32;
      }
      if (c1 != c2) {
         return (c1 - c2);
      }
   }
   return (0);
}


urlptr_t *scan_for_urls (pagebuf, pagelen, base)
char *pagebuf;
int pagelen;
char *base;
{
   urlptr_t *urllist = NULL;
   urlptr_t *tmp = NULL;
   int state = 0;
   int i;
   int cycle = 0;

   for (i=0; i<pagelen; i++) {
      if (cycle > 0) {
         cycle--;
         continue;
      }
      if (pagebuf[i] == ' ') {
         continue;
      }
      if (state == 0) {
         if (pagebuf[i] == '<') {
            state = 1;
         }
      } else if (state == 1) {
         if (string_compare (&pagebuf[i], "body", 4) == 0) {
            state = 3;
/*
         } else if (string_compare (&pagebuf[i], "base", 4) == 0) {
            state = 2;
*/
         } else if (string_compare (&pagebuf[i], "img", 3) == 0) {
            state = 4;
         } else {
            state = 0;
         }
      } else if (state == 2) {
         if (string_compare (&pagebuf[i], "href=", 5) == 0) {
            cycle = 4;
            state = 5;
         } else if (string_compare (&pagebuf[i], "</base>", 7) == 0) {
            state = 0;
         }
      } else if (state == 3) {
         if (string_compare (&pagebuf[i], "background=", 11) == 0) {
            cycle = 10;
            state = 5;
         } else if (pagebuf[i] == '>') {
            state = 0;
         }
      } else if (state == 4) {
         if (string_compare (&pagebuf[i], "src=", 4) == 0) {
            cycle = 3;
            state = 5;
         } else if (string_compare (&pagebuf[i], "</img>", 7) == 0) {
            state = 0;
         }
      } else if (state == 5) {
         if (pagebuf[i] == '"') {
            if (urllist == NULL) {
               tmp = get_urlptr ();
               urllist = tmp;
            } else {
               tmp->next = get_urlptr ();
               tmp = tmp->next;
            }
            tmp->next = NULL;
            tmp->line[0] = 'G';
            tmp->line[1] = 'E';
            tmp->line[2] = 'T';
            tmp->line[3] = ' ';
            tmp->len = 4;
            state = 6;
         } else {
            printf ("unexpected character in URL: %c\n", pagebuf[i]);
            state = 0;
         }
      } else if (state == 6) {
         if (pagebuf[i] != '"') {
            if ((tmp->len == 4) && (string_compare (&pagebuf[i], "http:", 5) != 0)) {
               bcopy (base, &tmp->line[4], strlen(base));
               tmp->len += strlen(base);
            } else {
               tmp->len = 0;
               state = 0;
               continue;
            }
            tmp->line[tmp->len] = pagebuf[i];
            tmp->len++;
         } else {
            assert (tmp->len > 4);
            assert (tmp->len < (sizeof(tmp->line) - 20));
                             /* must use this newline for Harvest */
            bcopy (" HTTP/1.0\r\n\r\n", &tmp->line[tmp->len], 14);
            tmp->len += 14;
            state = 0;
/*
printf ("next url:\n%s", tmp->line);
*/
         }
      }
   }
   return (urllist);
}


int main (argc, argv)
int argc;
char **argv;
{
   int ret = 0;
   char *req;
   int filesize = 0;
   int bytes = 0;
   int rate = 1000000; //ae_getrate ();
   int bpms;
   int ticks;
   int time1;
   FILE *urlfile;
   char line[256];
   int webpages = 0;
   int pagebytes;
   urlptr_t *urllist;
   int i;
   int repeatcount = 0;
   int repeatmax = 1;
   double totaltime = 0.0;

   if ((argc != 3) && (argc != 4) && (argc != 5)) {
      printf ("Usage: %s <server> <urlfile> [ <iters> <portno> ]\n", argv[0]);
      exit (0);
   }

   servername = argv[1];

#ifdef EXOPC
   ip_addr = get_ip_from_name (servername);
#else
   ip_addr = malloc(4);
   ip_addr[0] = 128;
   ip_addr[1] = 2;
   ip_addr[2] = 252;
   ip_addr[3] = 81;
#endif
   if (ip_addr == NULL) {
      fprintf (stderr, "server %s is unknown (i.e., not listed in hosttable\n", argv[1]);
      exit(1);
   }

repeat:
   filesize = 0;
   bytes = 0;

   if ((urlfile = fopen (argv[2], "r")) == NULL) {
      printf ("Unable to open specified list of URLs: %s\n", argv[2]);
      exit (0);
   }

   if (argc >= 4) {
      repeatmax = atoi (argv[3]);
      assert (repeatmax > 0);
      if (argc == 5) {
         portno = atoi (argv[4]);
         assert (portno > 0);
      }
   }

   connfds = (int *) malloc (MaxConns * sizeof (int));
   assert (connfds);
   for (i=0; i<MaxConns; i++) {
      connfds[i] = -1;
   }

                        /* must use this newline for Harvest */
   req = "GET / HTTP/1.0\r\n\r\n";
   line[0] = 'G';
   line[1] = 'E';
   line[2] = 'T';
   line[3] = ' ';
   line[4] = '/';

/*
printf ("going to loop\n");
*/

/*
   gettimeofday (&time1, &crap);
   lasttime1 = time1;
*/
   time1 = time(NULL); //ae_gettick();
   while (fgets (&line[5], 250, urlfile)) {
/*
printf ("%s\n", line);
*/

      if ((fd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
         fprintf (stderr, "Unable to create the main server TCP socket\n");
         exit(0);
      }

      bzero ((char *) &connaddr, sizeof(connaddr));
      connaddr.sin_family = AF_INET;
      connaddr.sin_addr.s_addr = *((uint *) ip_addr);
      connaddr.sin_port = htons (portno);

      if (connect (fd, (struct sockaddr *) &connaddr, sizeof(connaddr)) == -1) {
         perror ("cannot establish connection to server\n");
         exit(0);
      }
      ret = strlen (line);
      assert (ret < 240);
                        /* must use this newline for Harvest */
      bcopy (" HTTP/1.0\r\n\r\n\0", &line[(ret-1)], 14);
      ret = write (fd, line, (ret+12));
/*
printf ("next HTML request:\n%s",line);
      printf ("\nret %d\n", ret);
*/
      /* set up base pathname */
      ret = strlen(line) - 11;
      line[ret] = (char) 0;
      while (line[ret] != '/') {
         line[ret] = (char) 0;
         ret--;
         assert (ret > 0);
      }

      pagebytes = 0;
      while ((ret = read (fd, &pagebuf[pagebytes], 8192)) > 0) {
         filesize += ret;
         bytes += ret;
         pagebytes += ret;
         if (pagebytes > (32768 - 8192)) {
            printf ("Page might exceed maximum length\n");
            exit (0);
         }
      }
      close (fd);
      urllist = scan_for_urls (pagebuf, pagebytes, &line[4]);
/*
if (pagebytes < 500) {
printf ("pagebytes %d, urllist %x\n", pagebytes, (unsigned) urllist);
pagebuf[pagebytes] = (char) 0;
printf ("%s\n", pagebuf);
}
*/
      while ((urllist != NULL) && (numconns < MaxConns)) {
         urlptr_t *tmp = urllist;
         urllist = tmp->next;
         if (tmp->len > 0) {
            request_url (tmp);
         }
         free_urlptr (tmp);
      }
      while (numconns != 0) {
         for (i=0; i<MaxConns; i++) {
            if (connfds[i] != -1) {
               while (read (connfds[i], buf, 8192) > 0) {
               }
               close (connfds[i]);
               connfds[i] = -1;
               numconns--;
               if (urllist) {
                  urlptr_t *tmp = urllist;
                  urllist = tmp->next;
                  if (tmp->len > 0) {
                     request_url (tmp);
                  }
                  free_urlptr (tmp);
               }
/* don't know when it is closed ??
               if (fstat (connfds[i], &statbuf) != 0) {
                  perror ("unable to stat socket descriptor");
                  exit (0);
               }
               while (statbuf.st_size > 0) {
                  if ((ret = read (fd, buf, min (8192, statbuf.st_size))) != min (8192, statbuf.st_size)) {
                     printf ("input size not equal to expected: %d != %d\n", ret, min (statbuf.st_size, 8192));
                     exit (0);
                  }
                  statbuf.st_size -= min (8192, statbuf.st_size);
               }
*/
            }
         }
      }
      
      webpages++;
/*
      if (webpages > 0) {
         struct timeval tmp;
         gettimeofday (&tmp, &crap);
         usecs = (tmp.tv_sec * 1000000) + tmp.tv_usec - (lasttime1.tv_sec * 1000000) - lasttime1.tv_usec;
         bpms = (bytes / (usecs / 1000)) * 1000;
         printf ("after pageno %d -- bytes: %7d, bytes/sec: %6d\n", webpages, bytes, bpms);
         lasttime1 = tmp;
         bytes = 0;
      }
*/
   }

   ticks = time(NULL) - time1; //ae_gettick() - time1;
   bpms = (filesize / (ticks * rate / 1000)) * 1000;
   totaltime += (double) ticks * (double) rate / 1000000.0;
   printf ("bytes: %7d, usecs %8d, bytes/sec: %6d, pages/sec %f\n", filesize, (ticks * rate / 1000), bpms, ((double)webpages/totaltime));

   repeatcount++;
   if (repeatcount < repeatmax) {
      goto repeat;
   }

   printf ("done: ret %d (webpages %d)\n", ret, webpages);
/*
tcp_statistics();
*/
   return (1);
}

