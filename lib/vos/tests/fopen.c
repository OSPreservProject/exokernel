
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

/*
 * test fopen, and therefore also fsrv 
 */

int 
main()
{
  FILE *fp;
  char s[1024];
  int r;
  
  fp = fopen("/etc/hosts", "r");
  r = fread(s,1024,1,fp);
  printf("fread returned %d, should be 0\n",r);
  printf("checking if eof is reached, should be: ");
  if (feof(fp))
    printf("eof reached\n");
  else
    printf("noop, feof failed\n");
  fclose(fp);

  fp = fopen("/home/benjie/test/simple.out", "w+");
  fwrite(s, 1024, 1, fp);
  fclose(fp);

  return 0;
}


