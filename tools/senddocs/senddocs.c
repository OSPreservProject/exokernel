
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>

#include "assert.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netdb.h>

#include <string.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define WEBFS_PORTNO	1081

char* recipient = "127.0.0.1";

char *userDir = "public_html";
int userDirLen = 11;

#define BUFSIZE 4096
char buf[BUFSIZE];

#define ALFS_EEXIST	-17
#define ALFS_MAX_FILESIZE	4194304


/*
 * Compute Internet Checksum for "count" bytes beginning at location "addr".
 * Taken from RFC 1071.
 */

static long inet_sum(addr, count, start, last)
unsigned short *addr;
int count;
long start;
int last;
{
    register long sum = start;

    /* An unrolled loop */
    while ( count > 15 ) {
       sum += htons (* (unsigned short *) &addr[0]);
       sum += htons (* (unsigned short *) &addr[1]);
       sum += htons (* (unsigned short *) &addr[2]);
       sum += htons (* (unsigned short *) &addr[3]);
       sum += htons (* (unsigned short *) &addr[4]);
       sum += htons (* (unsigned short *) &addr[5]);
       sum += htons (* (unsigned short *) &addr[6]);
       sum += htons (* (unsigned short *) &addr[7]);
       addr += 8;
       count -= 16;
    }

    /*  This is the inner loop */
    while( count > 1 )  {
        sum += htons (* (unsigned short *) addr++);
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if(count > 0)
        sum += * (unsigned char *) addr;

    if (last) {
       /*  Fold 32-bit sum to 16 bits */
       while (sum>>16) {
          sum = (sum & 0xffff) + (sum >> 16);
       }
       return (~sum & 0xffff);
    }
    return sum;
}


int openconn (char* host)
{
   int fd;
   struct sockaddr_in webServerAddr;
   int webServerAddrLen;
   struct hostent* ent = gethostbyname(host);
   if (!ent)
     ent = gethostbyaddr(host, strlen(host), AF_INET);
   if (!ent) {
     herror("openconn");
     exit(0);
   }

   if ((fd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
      fprintf(stderr, "Couldn't create a TCP socket\n");
      exit(0);
   }

   bzero ((char *) &webServerAddr, sizeof(webServerAddr));
   webServerAddr.sin_family = AF_INET;
   webServerAddr.sin_addr.s_addr = *(int*)ent->h_addr;
   webServerAddr.sin_port = htons (WEBFS_PORTNO);
   webServerAddrLen = sizeof (webServerAddr);

   if (connect (fd, (struct sockaddr*)&webServerAddr, webServerAddrLen) < 0) {
      fprintf(stderr, "cannot establish connection to server: portno %d\n", WEBFS_PORTNO);
      perror("dammit");
      exit(0);
   }

   return (fd);
}


int send_aloha (greet)
int greet;
{
   int fd;
   int len = 19;
   int ret;
   char msg = (greet) ? '0' : '3';

   fd = openconn (recipient);

   sprintf (buf, "%c %8d %8x", msg, 0, 0);
   if (len != strlen(buf)) {
      fprintf (stderr, "Unexpected length for send_aloha message: %d != %d\n", strlen(buf), len);
      exit (0);
   }
   
   len++;  /* to get end-of-line character too */
   ret = write (fd, buf, len);
   if (ret != len) {
      fprintf (stderr, "Failed to send entire send_aloha message: %d != %d\n", ret, len);
      exit (0);
   }

   len = read (fd, buf, 4);
   ret = atoi(buf);
   if ((len != 4) || (ret == 0)) {
      fprintf (stderr, "Unexpcted response to send_aloha: len %d, ret %d\n", len, ret);
      exit (0);
   }

   close (fd);
   return 0;
}


int send_mkdir (pathname)
char *pathname;
{
   int fd;
   int len;
   int ret;

   fd = openconn (recipient);

   len = strlen (pathname);
   sprintf (buf, "1 %8d %8x %s", len, 0, pathname);
   len += 20;
   if (len != strlen(buf)) {
      fprintf (stderr, "Unexpected length for send_mkdir message: %d != %d\n", strlen(buf), len);
      exit (0);
   }
   
   len++;  /* to get end-of-line character too */
   ret = write (fd, buf, len);
   if (ret != len) {
      fprintf (stderr, "Failed to send entire send_mkdir message: %d != %d\n", ret, len);
      exit (0);
   }

   len = read (fd, buf, 4);
   ret = atoi(buf);
   if ((len != 4) || (ret == 0)) {
      fprintf (stderr, "Unexpcted response to send_mkdir: len %d, ret %d\n", len, ret);
      exit (0);
   }

   close (fd);
printf ("Moving on\n");
   return (ret);
}


int send_file (pathname)
char *pathname;
{
   int ret, ret2;
   int filefd;
   int fd;
   int filesize = 0;
   int len;
   int sum = 0;

   if ((filefd = open (pathname, O_RDONLY)) < 0) {
      if ((errno == EACCES) || (errno == ENOENT)) {
         return (0);
      }
      fprintf (stderr, "Unable to open file: %s (%d)\n", pathname, filefd);
      perror ("dammit");
      exit (0);
   }

   fd = openconn (recipient);

   while ((ret = read (filefd, &buf[0], 4096)) > 0) {
      sum = inet_sum ((unsigned short *) buf, ret, sum, 0);
   }
   lseek (filefd, 0, 0);

   len = strlen (pathname);
   sprintf (buf, "2 %8d %8x %s", len, sum, pathname);
   len += 20;
   if (len != strlen(buf)) {
      fprintf (stderr, "Unexpected length for send_file message: %d != %d\n", strlen(buf), len);
      exit (0);
   }
   
   len++;  /* to get end-of-line character too */
   ret = write (fd, buf, len);
   if (ret != len) {
      fprintf (stderr, "Failed to send entire send_file message: %d != %d\n", ret, len);
      exit (0);
   }

   len = read (fd, buf, 4);
   ret = atoi(buf);
   if ((len != 4) || (ret == 0)) {
      fprintf (stderr, "Unexpcted response to send_file: len %d, ret %d\n", len, ret);
      exit (0);
   }

   sum = 0;
   while ((ret = read (filefd, &buf[0], 4096)) > 0) {
      filesize += ret;
      sum = inet_sum ((unsigned short *) buf, ret, sum, 0);
/*
printf ("running sum: %d (count %d)\n", sum, filesize);
*/
      ret2 = write (fd, buf, ret);
      if (ret2 != ret) {
         fprintf (stderr, "Non-matched amount of data sent: %d != %d\n", ret2, ret)
;
         exit (0);
      }
   }
printf ("filesize: %d (sum %x)\n", filesize, sum);

   close (filefd);

   close (fd);
/*
if (strcmp (pathname, "/home/am2/josh/public_html/index.html") == 0) {
   send_aloha (0);
   exit (0);
}
*/
   return (ret);
}


int find_wordend (sentence, delim)
char *sentence;
char delim;
{
   int i = 0;

   while ((sentence[i] != delim) && (sentence[i] != (char) 0)) {
      i++;
   }
   return (i);
}


int fix_personal_directory_name (result, pathname)
char *result;
char *pathname;
{
   char *rem;
   int wordlen;
   struct passwd *pw;
   int remlen;

   if (pathname[0] != '~') {
      fprintf(stderr, "fix_personal_directory_name called with illegal pathname: %s\n", pathname);
      exit(0);
   }

   wordlen = find_wordend (&pathname[1], '/');
   rem = &pathname[(wordlen+2)];

   pathname[(wordlen+1)] = (char) NULL;
   if ((pw = getpwnam (&pathname[1])) == NULL) {
      fprintf (stderr, "Unknown user name: %s (%d)\n", &pathname[1], wordlen);
      exit(0);
   }
   pathname[(wordlen+1)] = '/';

   wordlen = strlen (pw->pw_dir);
   bcopy (pw->pw_dir, result, wordlen);
   result[wordlen] = '/';
   bcopy (userDir, &result[(wordlen+1)], userDirLen);
   remlen = strlen(rem);
   if (remlen > 0) {
      result[(wordlen + userDirLen + 1)] = '/';
      bcopy (rem, &result[(wordlen + userDirLen + 2)], (remlen+1));
   } else {
      result[(wordlen + userDirLen + 1)] = (char) 0;
      remlen = -1;
   }
   return (wordlen + userDirLen + remlen + 2);
}


int get_documents (pathname, namelen)
char *pathname;
int namelen;
{
   struct dirent *ep;
   DIR *dirfile;
   int runsize = 0;
   struct stat statbuf;
   int getit = 1;
   int ret;

   if ((dirfile = opendir (pathname)) != NULL) {
      printf("(3) add %s\n", pathname);
/* XXX
      ret = sys_mkdir (pathname, 0777);
*/
      ret = send_mkdir (pathname);
      assert ((ret == 1) || (ret == ALFS_EEXIST));
      while ((ep = readdir (dirfile)) != NULL) {
         if (strcmp(ep->d_name, ".") && strcmp(ep->d_name, "..")) {
            pathname[namelen] = '/';
            strcpy (&pathname[(namelen+1)], ep->d_name);
            runsize += get_documents (pathname, namelen+strlen(ep->d_name)+1);
            pathname[namelen] = (char) 0;
         }
      }
      closedir (dirfile);
      printf("%s: %d\n", pathname, runsize);
   } else {
      if (lstat (pathname, &statbuf) != 0) {
         fprintf (stderr, "Unable to lstat file: %s\n", pathname);
         exit(0);
      }
/*
      printf ("found web document: %s\n", pathname);
*/
      if (S_ISLNK(statbuf.st_mode)) {
         char linkname[1024];
         int linklen = readlink (pathname, linkname, 1024);
         if (linklen == -1) {
            perror ("Unable to read symbolic link");
            exit (0);
         }
         linkname[linklen] = (char) 0;
         if (stat (pathname, &statbuf) != 0) {
            fprintf (stderr, "Unable to stat soft link: %s\n", linkname);
            getit = 0;
         }
      }
      runsize = statbuf.st_size;
      if ((getit) && (runsize <= ALFS_MAX_FILESIZE)) {
         int ret;
         printf ("add: %s\n", pathname);
         ret = send_file (pathname);
         if (ret < 0) {
            fprintf (stderr, "Unexpected error from send_file: %d\n", ret);
            exit (0);
         }
/* XXX
         int realfd = open (pathname, O_RDONLY);
         int alfsfd;

         if (realfd >= 0) {
            alfsfd = sys_open (pathname, (OPT_RDWR | OPT_CREAT), 0777);
            assert (alfsfd >= 0);

            while ((ret = read (realfd, buf, BUFSIZE)) > 0) {
               assert (sys_write (alfsfd, buf, ret) == ret);
            }

            close (realfd);
            sys_close (alfsfd);
         } else {
            assert ((errno == EACCES) || (errno == ENOENT));
         }
*/
      }
   }
   return (runsize);
}


int main (argc, argv)
int argc;
char **argv;
{
   char inname[1024];
   char realname[1024];
   FILE *docdirfile;
   int len;
   int totalsize = 0;
   int tmplen;
   int ret;

   if (argc != 2) {
     if (argc != 3) {
       fprintf (stderr, "Usage: %s <docdirs> [host]\n", argv[0]);
       exit (0);
     }
     recipient = argv[2];
   }

   if ((docdirfile = fopen (argv[1], "r")) == NULL) {
      fprintf (stderr, "Unable to open docdir file: %s\n", argv[1]);
      exit (0);
   }

   send_aloha (1);

   while (fgets (inname, 1023, docdirfile)) {
      len = strlen (inname);
      assert (inname[(len-1)] == '\n');
      inname[(len-1)] = (char) 0;

      if (inname[0] == '~') {
         len = fix_personal_directory_name (realname, inname);
         tmplen = 0;
         while (1) {
            tmplen++;
            tmplen += find_wordend (&realname[tmplen], '/');
            realname[tmplen] = (char) 0;
            printf("(1) add %s\n", realname);
/* XXX
            ret = sys_mkdir (realname, 0777);
*/
            ret = send_mkdir (realname);
            if ((ret != 1) && (ret != ALFS_EEXIST)) {
               fprintf (stderr, "Problem with send_mkdir for %s: ret %d\n", realname, ret);
            }
            assert ((ret == 1) || (ret == ALFS_EEXIST));
            if (tmplen >= len) {
               break;
            }
            realname[tmplen] = '/';
         }
         totalsize += get_documents (realname, len);
      } else {
         tmplen = 0;
         while (1) {
            tmplen++;
            tmplen += find_wordend (&inname[tmplen], '/');
            inname[tmplen] = (char) 0;
            printf("(2) add %s\n", inname);
/* XXX
            ret = sys_mkdir (inname, 0777);
*/
            ret = send_mkdir (inname);
            assert ((ret == 1) || (ret == ALFS_EEXIST));
            if (tmplen >= len) {
               break;
            }
            inname[tmplen] = '/';
         }
         totalsize += get_documents (inname, (len-1));
      }
   }
   printf ("total size of grabbed documents: %d\n", totalsize);
   send_aloha (0);
/* XXX
   unmountFS ();
*/
   return 0;
}

