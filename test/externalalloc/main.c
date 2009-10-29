#include <stdio.h>
#include <stdlib.h>

#include <fd/cffs/cffs.h>
#include <fd/cffs/cffs_alloc.h>
#include <fd/cffs/cffs_proc.h>

int main (int argc, char *argv[]) {
  int ret;
  int dev;

  if (argc != 2) {
    printf ("requires parameter specifying disk number to test on\n");
    return -2;
  }

  dev = atoi (argv[1]);

  printf ("run this after creating a new filesystem on the device %d\n", dev);

  CFFS_CHECK_SETUP(dev);

  ret = cffs_alloc_externalAlloc (dev, 257);
  if (ret != 0) {
    printf ("failed initial alloc with ret %d\n", ret);
    return -2;
  }

  ret = cffs_alloc_externalAlloc (dev, 257);
  if (ret == 0) {
    printf ("failed by allowing alloc of allocated block\n");
    return -2;
  }

  ret = cffs_alloc_externalFree (dev, 257);
  if (ret != 0) {
    printf ("failed free with ret %d\n", ret);
    return -2;
  }

  ret = cffs_alloc_externalFree (dev, 257);
  if (ret == 0) {
    printf ("failed by allowing free of un-allocated block\n");
    return -2;
  }
  
  ret = cffs_alloc_externalAlloc (dev, 257);
  if (ret != 0) {
    printf ("failed alloc after free with ret %d\n", ret);
    return -2;
  }

  printf ("all tests passed\n");
  return 0;
}

