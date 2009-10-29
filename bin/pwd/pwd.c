#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h> 
#include <stdio.h>
#include <assert.h>
#include "errno.h"
#include <dirent.h>
#include <memory.h>
#include <malloc.h>
#include <unistd.h>

#include <sys/param.h>

/* GETCWD forces pwd not to use chdir and use a method similar to
   getcwd it is here for no reason */
/*#define GETCWD*/

extern int lstat();
char *getpwd (char *buf, int size) {
#ifdef GETCWD
  static  char dots[]
    = "../../../../../../../../../../../../../../../../../../../../../../../\
../../../../../../../../../../../../../../../../../../../../../../../../../../\
../../../../../../../../../../../../../../../../../../../../../../../../../..";
  char *dotp = &dots[sizeof dots - 2];
  char temp_name[1024];
#endif
  int malloc_flag = 0;
  dev_t rootdev, thisdev;
  ino_t rootino, thisino;
  dev_t dotdev;
  ino_t dotino;
  struct stat st;
  char path[1024];
  char *pathp = &path[1024];
  --pathp;
  *pathp = 0;
  
  if (lstat (".", &st) < 0)
    return NULL;
  dotdev = st.st_dev;
  dotino = st.st_ino;

  if (lstat ("/", &st) < 0)
    return NULL;
  rootdev = st.st_dev;
  rootino = st.st_ino;
  
  /*   printf("rootdev %d  rootino %d\n",rootdev,rootino); */

  if (!buf) { 
    size = (size > 0) ? size : 1024; /* should be MAXPATHLEN */
    buf = malloc(size);malloc_flag=1;
  }
  
  while (!(dotdev == rootdev && dotino == rootino))
    {
      register DIR *dirstream;
      register struct dirent *d;

      /* printf("dotdev %d  dotino %d\n",dotdev,dotino); */
      /*       printf("dotdev %d  dotino %d \n",dotdev,dotino);  */
#ifdef GETCWD
      dotp -= 3;
      /*       printf("dotp: %s\n",dotp); */
      dirstream = opendir(dotp);
#else 
      chdir("..");
      dirstream = opendir(".");
#endif
      assert(dirstream);
      while ((d = readdir (dirstream)) != NULL) {
#ifdef GETCWD
	strcpy(temp_name,dotp);
	strcat(temp_name,"/");
	strcat(temp_name,d->d_name);
	if (lstat (temp_name, &st) < 0)
	  goto lose;
#else
	if (lstat (d->d_name, &st) < 0)
	  goto lose;
#endif
	thisdev = st.st_dev;
	thisino = st.st_ino;
	/* 	printf("thisdev %d  thisino %d name: %s\n",thisdev,thisino,d->d_name);  */
	
	if (dotdev == thisdev && dotino == thisino) {
	  /* 	  printf("MATCH: %s\n",d->d_name);  */
	  pathp = pathp - d->d_namlen;
	  memcpy(pathp,d->d_name,d->d_namlen);
	  pathp--;
	  *pathp = '/';
	  break;
	}
	
      }
      if (d == NULL) goto lose;

#ifdef GETCWD
      if (lstat (dotp, &st) < 0)
	goto lose;
#else
      if (lstat (".",&st) < 0)
	goto lose;
#endif
      dotdev = st.st_dev;
      dotino = st.st_ino;



    }
  /*   printf("DONE\n"); */

  if (!*pathp) {
    strcpy(buf,"/");
  } else {
    strcpy(buf,pathp);
  }
  return buf;

lose:
  if (malloc_flag) {
    free(buf);
  }
  return NULL;
}

int main() {
  char *cwd;
#if 0
  char path[64];
  printf("please wait...\n");
  cwd = getcwd(path,64);
  if (cwd) printf("getcwd: %s\n",path);
#endif
  if ((cwd = getpwd((char *)NULL, 64)) == NULL) {
    perror ("pwd failed");
    exit (-1);
  } else {
    printf("%s\n", cwd);
  }
  exit(0);

}
