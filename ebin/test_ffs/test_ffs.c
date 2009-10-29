#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/buf.h>
#include <xuser.h>
#include "../../lib/ffs/ffs.h"
#include "../../lib/ffs/inode.h"

int
main (int argc, char **argv)
{
  int f, i, j, testsize;
  struct inode *ip;
  char *testdata;

  /* first, test in read-only mode */
  i = ffs_init(1, 0x10001, 0, 0, 1);
  if (i)
  {
    printf ("ffs_init(1,0x10001,0,0,1) failed (%d); exiting\n", i);
    return (1);
  }

  /* see if we can open a file for writing */
  f = ffs_open ("/test", O_WRONLY | O_CREAT, 0777);
  if (f >= 0)
  {
    printf ("Opened /test for writing in read-only mode\n");
    ffs_close (f);
  }

  f = ffs_open ("/testdir-ro", O_RDONLY, 0);
  if (f >= 0)
  {
    printf ("/testdir-ro exists; skipping readonly mkdir test\n");
    ffs_close (f);
  }
  else
  {
    i = ffs_mkdir ("/testdir-ro", 0777);
    if (!i)
    {
      printf ("mkdir /testdir-ro succeeded in read-only mode\n");
      i = ffs_rmdir ("/testdir-ro");
      if (i)
	printf ("... but rmdir /testdir-ro failed (%d)\n", i);
    }
  }

  /* make sure /testdata exists and is large enough, and read it in */
  f = ffs_open ("/testdata", O_RDONLY, 0);
  if (f < 0)
  {
    printf ("/testdata doesn't exist (should be 24k file); exiting\n");
    ffs_close (f);
    ffs_shutdown ();
    return (1);
  }

  ip = ffs_fstat (f);
  if (!ip)
  {
    printf ("ffs_fstat(%d) for /testdata failed; exiting\n", f);
    ffs_close (f);
    ffs_shutdown ();
    return (1);
  }
  if (ip->i_size < 24*1024)
  {
    printf ("/testdata should be 24k; exiting\n");
    ffs_close (f);
    ffs_shutdown ();
    return (1);
  }

  testsize = 24*1024; /*ip->i_size;*/
  testdata = malloc (testsize);
  if (!testdata)
  {
    printf ("malloc(%d) failed; exiting\n", testsize);
    ffs_close (f);
    ffs_shutdown ();
    return (1);
  }

  i = ffs_read (f, testdata, testsize);
  if (i < testsize)
  {
    printf ("ffs_read (%d, %p, %d) returned %d", f, testdata, testsize, i);
    if (i <= 0)
    {
      printf ("; exiting\n");
      ffs_close (f);
      ffs_shutdown ();
      return (1);
    }
    printf ("; trying again\n");
  }
  while (i < testsize)
  {
    j = ffs_read (f, testdata + i, testsize - i);
    if (j <= 0)
    {
      printf ("ffs_read (%d, %p, %d) returned %d; exiting\n",
	      f, testdata + i, testsize - i, j);
      ffs_close (f);
      ffs_shutdown ();
      return (1);
    }
    i += j;
  }

  i = ffs_close (f);
  if (i)
  {
    printf ("ffs_close (%d) returned %d\n", f, i);
  }

  i = ffs_shutdown();
  if (i)
  {
    printf ("ffs_shutdown returned %d; exiting\n", i);
    return (1);
  }

  i = ffs_init (1, 0x10001, 0, 0, 0);
  if (i)
  {
    printf ("ffs_init(1,0x10001,0,0,0) failed (%d); exiting\n", i);
    return (1);
  }

  i = ffs_mkdir ("/testdir", 0777);
  if (i)
    printf ("mkdir(/testdir,0777) failed (%d); skipping rmdir\n", i);
  else
  {
    f = ffs_open ("/testdir", O_RDONLY, 0);
    if (f < 0)
      printf ("ffs_open(/testdir, O_RDONLY) failed (%d)\n", f);
    else
    {
      i = ffs_close(f);
      if (i)
	printf ("ffs_close(%d) failed (%d)\n", f, i);
    }
    f = ffs_open ("/testdir", O_RDWR, 0);
    if (f >= 0)
    {
      printf ("ffs_open(/testdir, O_RDWR) succeeded\n");
      ffs_close (f);
    }
    i = ffs_rmdir ("/testdir");
    if (i)
      printf ("rmdir(/testdir) failed (%d)\n", i);
  }

  f = ffs_open ("/testoutput", O_WRONLY | O_CREAT, 0777);
  if (f < 0)
  {
    printf ("ffs_open(/testoutput, O_WRONLY|O_CREAT, 0777) failed (%d);"
	    " exiting\n", f);
    ffs_close (f);
    ffs_shutdown ();
    return (1);
  }

  i = ffs_write (f, testdata, testsize);
  if (i < testsize)
  {
    printf ("ffs_write (%d, %p, %d) returned %d", f, testdata, testsize, i);
    if (i <= 0)
    {
      printf ("; exiting\n");
      ffs_close (f);
      ffs_shutdown ();
      return (1);
    }
    printf ("; trying again\n");
  }
  while (i < testsize)
  {
    j = ffs_write (f, testdata + i, testsize - i);
    if (j <= 0)
    {
      printf ("ffs_write (%d, %p, %d) returned %d; exiting\n",
	      f, testdata + i, testsize - i, j);
      ffs_close (f);
      ffs_shutdown ();
      return (1);
    }
    i += j;
  }

  i = ffs_close (f);
  if (i)
  {
    printf ("ffs_close (%d) returned %d\n", f, i);
  }

  i = ffs_shutdown();
  if (i)
  {
    printf ("ffs_shutdown returned %d; exiting\n", i);
    return (1);
  }

  /* test the scatter/gather stuff also */
  {
    struct buf b;

    b.b_flags = B_READ | B_SYNC;
    b.b_dev = 0;
    b.b_blkno = 20;
    b.b_bcount = 8*1024;
    b.b_memaddr = testdata;
    b.b_resptr = (int*)1;
    if (sys_disk_request(0, 0, 0, &b))
      printf ("sys_disk_request (no s/g) failed!\n");
    else
    {
      struct buf bs[8];
      for (i = 0; i < 8; i++)
      {
	bs[i].b_flags = B_SCATGATH | B_READ | B_SYNC;
	bs[i].b_dev = 0;
	bs[i].b_blkno = i*2 + 20;
	bs[i].b_bcount = 1024;
	bs[i].b_memaddr = testdata + 8*1024 + i*1024;
	bs[i].b_resptr = &bs[i+1];
      }
      bs[0].b_bcount = 8*1024;
      bs[7].b_flags = B_READ | B_SYNC;
      bs[7].b_resptr = (int*)2;
      if (sys_disk_request(0, 0, 0, &bs[0]))
	printf ("sys_disk_request (s/g) failed!\n");
      else if (memcmp(testdata, testdata + 8*1024, 8*1024))
	printf ("memcmp failed!\n");
      else
	printf ("s/g seems to be working\n");
    }
  }

  return (0);
}
