
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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <xok/sys_ucall.h>
#include <exos/process.h>
#include <exos/osdecl.h>

#include <xok/sysinfo.h>
#define ae_getrate()    __sysinfo.si_rate
#define ae_gettick()    __sysinfo.si_system_ticks

#define TESTSZ  100 * 1024
#define SIZE    8192
#define N       10000

char aarray[TESTSZ];
char buffer[1024];

int
main (int argc, char **argv)
{
  int i;
  struct timeval s, t;
  int pfdr[2];
  int pfdw[2];
  int writefd, readfd;
  int status;
  int sid;
  int *shmaddr;
  int val;
  int r;

  printf("TESTING PIPE\n");

  if (pipe(pfdw) < 0) {
    printf("pipe problems, call the plumber\n");
    return (0);
  } else {
    printf("pipe ok, fd0: %d fd1: %d\n", pfdw[0], pfdw[1]);
  }
  readfd = pfdw[0];
  writefd = pfdw[1];

  printf("good test\n");
  status = write(writefd, "test", 5);
  printf("write status %d, errno: %d\n", status, errno);
  status = read(readfd, buffer, 5);
  printf("read status %d, errno: %d\n", status, errno);
  printf("buffer: %s\n", buffer);

  printf("permissions test\n");
  status = write(readfd, "test", 5);
  printf("write status %d, errno: %d\n",status, errno);
  status = read(writefd, buffer, 5);
  printf("read status %d, errno: %d\n", status, errno);
  printf("buffer: %s\n", buffer);


  /* Create a second pipe */
  if (pipe(pfdr) < 0) {
    printf("pipe problems, call the plumber\n");
    return (0);
  } else {
    printf("pipe ok, fd0: %d fd1: %d\n",pfdr[0],pfdr[1]);
  }

  if ((sid = shmget(IPC_PRIVATE, 4096, IPC_CREAT)) < 0) {
      perror("shmget");
  }

  printf("sid %d\n", sid);


  printf("VCOPY\n");

  for (i=0; i < sizeof(buffer); i++)
    buffer[i] = buffer[i];
  for (i=0; i < sizeof(aarray); i++)
    aarray[i] = aarray[i];
  gettimeofday(&s, NULL);
  for (i = 0; i < N; i++) {
      if ((r = sys_vcopyout(buffer, __envid, (u_long) aarray, 1)) != 0) {
	  printf("vcopy failed %d\n", r);
      }
  }
  gettimeofday(&t, NULL);
  printf("%d vcopy: in %lu usec\n", N, 
	 (t.tv_sec - s.tv_sec) * 1000000 + t.tv_usec - s.tv_usec);

  printf("FORKING\n");
  if (fork()) {
      int env = __envid;
      int child;

      close(readfd);
      close(pfdr[1]);

      printf("PARENT\n");

      if ((shmaddr = (int *) shmat(sid, 0, 0)) < 0) {
	  perror("shmat");
      }
      *shmaddr = 0;

      for (i = 0; i < SIZE; i++) {
	  aarray[i] = 0;
      }

      /* Exchange envids */
      write(writefd, &env, sizeof(env));
      read(pfdr[0], &child, sizeof(child));
      printf("parent: %d child %d\n", env, child);

      printf("YIELD\n");

      gettimeofday(&s, NULL);
      val = 0;
      for (i = 0; i < N; i++) {
	  *shmaddr = val +  1;
	  val += 2;
	  do {
	      yield(child);
	  } while (*shmaddr < val);
      }
      gettimeofday(&t, NULL);
      printf("%d: yield: in %lu usec\n", N, 
	     (t.tv_sec - s.tv_sec) * 1000000 + t.tv_usec - s.tv_usec);

      gettimeofday(&s, NULL);
      for (i = 0; i < N; i++) {
	  *shmaddr = val +  1;
	  val += 2;
	  do {
	      yield(-1);
	  } while (*shmaddr < val);
      }
      gettimeofday(&t, NULL);
      printf("%d: undirected yield: in %lu usec\n", N, 
	     (t.tv_sec - s.tv_sec) * 1000000 + t.tv_usec - s.tv_usec);

      if (shmctl(sid, IPC_RMID, 0)) {
	  perror("shmctl");
      }

      printf("PINGPONG one byte\n");
    
      aarray[0] = 0;

      gettimeofday(&s, NULL);
      for (i = 0; i < N; i++) {
	  write(writefd, &aarray[0], 1);
	  yield(child);
	  read(pfdr[0], &aarray[0], 1);
      }
      gettimeofday(&t, NULL);
      printf("%d: pingpong: %d bytes in %lu usec\n", N, 1, 
	     (t.tv_sec - s.tv_sec) * 1000000 + t.tv_usec - s.tv_usec);

      printf("PINGPONG %d bytes\n", SIZE);

      gettimeofday(&s, NULL);
      for (i = 0; i < N; i++) {
	  write(writefd, &aarray[0], SIZE);
	  read(pfdr[0], &aarray[0], SIZE);
      }
      gettimeofday(&t, NULL);

      printf("%d pingpong: %d bytes in %ld usec\n", N, SIZE, 
	     (t.tv_sec - s.tv_sec) * 1000000 + t.tv_usec - s.tv_usec);
      close(pfdw[0]);

      printf("TESTING HUMONGOUS WRITE TO PIPE\n");
      for (i = 0; i < TESTSZ; i++) {
	  aarray[i] = i % 13;
      }
      
      r = write(writefd, aarray, TESTSZ);
      if (r != TESTSZ) {
	  perror("write");
      }
      printf("write on %d returned %d\n", writefd, r);

      r = write(writefd, aarray, TESTSZ);
      if (r != TESTSZ) {
	  perror("write");
      }
      printf("write on %d returned %d\n", writefd, r);

      printf("DONE\n");
      sleep(20);
      printf("DONE WRITER\n");
      close(writefd);
      printf("CLOSER WRITE FD\n");
  } else {
      int env = __envid;
      int parent; 

      close(writefd);
      close(pfdr[0]);

      printf("CHILD\n");

      if ((shmaddr = (int *) shmat(sid, 0, 0)) < 0) {
	  perror("shmat");
      }
      *shmaddr = 0;

      for (i = 0; i < SIZE; i++) {
	  aarray[i] = 0;
      }

      read(readfd, &parent, sizeof(parent));
      write(pfdr[1], &env, sizeof(env));

      printf("child: %d parent %d\n", env, parent);

      val = 1;
      for (i = 0; i < N; i++) {
	  do {
	      yield(parent);
	  } while (*shmaddr < val);
	  *shmaddr = val + 1;
	  val = val + 2;
      }

      printf("done: %d\n", val);

      for (i = 0; i < N; i++) {
	  do {
	      yield(-1);
	  } while (*shmaddr < val);
	  *shmaddr = val + 1;
	  val = val + 2;
      }
      printf("done: %d\n", val);


      for (i = 0; i < N; i++) {
	  read(readfd, &aarray[0], 1);
	  write(pfdr[1], &aarray[0], 1);
	  yield(parent);
      }

      for (i = 0; i < N; i++) {
	  read(readfd, &aarray[0], SIZE);
	  write(pfdr[1], &aarray[0], SIZE);
      }

      close(pfdr[1]);

      for (i = 0; i < TESTSZ; i++) {
	  aarray[i] = '-';
      }
      aarray[TESTSZ - 1] = 0;

      printf("read returned %d\n",read(readfd, aarray, TESTSZ));
      printf("testting aarray:\n");
      for (i = 0; i < TESTSZ; i++) {
	  if (aarray[i] != i % 13) {
	      printf("differ at offset %d read %d written %d\n",
		     i,(u_int)aarray[i],i % 13);
	      exit(0);
	  }
      }

      for (i = 0; i < TESTSZ; i++) {aarray[i] = '-';}

      printf("reading one by one\n");
      for (i = 0; i < TESTSZ; i++) {
	  if ((status = read(readfd,(char *)&aarray[i],1)) != 1) {
	      printf("read returned %d\n",status);
	      printf("FAILED\n");
	      exit(-1);
	  }
      }
      printf("testting aarray:\n");
      for (i = 0; i < TESTSZ; i++) {
	  if (aarray[i] != i % 13) {
	      printf("differ at offset %d read %d written %d\n",
		     i,(u_int)aarray[i],i % 13);
	  }
				      
      }
      printf("THIS READ SHOULD RETURN 0\n");
      printf("READ: %d\n",read(readfd,aarray,10));
      printf("DONE\n");

  }
  printf("Ok\n");
  return (0);
}

