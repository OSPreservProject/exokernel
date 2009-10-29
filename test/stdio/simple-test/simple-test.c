
#include <stdio.h>

int main (int argc, char **argv)
{
/*
   setlinebuf(stdout);
*/
   extern int errno;
   errno=0;
   printf ("Hello world!!\n");
   /* Note: errno will be 9 for ptys */
/*
   printf ("errno: %d\n", errno);
   perror ("perror");
   fflush (stdout);
*/
   exit (0);
}

