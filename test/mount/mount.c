#include <fd/cffs/cffs.h>
#include <fd/cffs/cffs_proc.h>

#include <fd/mount_sup.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

extern char *__progname;

void usage () {
  printf ("usage: %s dev [superblock]\n\n", __progname);
  printf ("without superblock writes data; with superblock ls's prior written data\n");
  printf ("you must have a directory /unique that gets mounted over\n");
  printf ("you should have a new filesystem on dev but not have it mounted\n");
  exit (-1);
}

void main (int argc, char *argv[]) {
  unsigned int new_superblock = 144;
  unsigned int dev;

  if (argc !=2 && argc != 3)
    usage ();

  dev = atoi (argv[1]);

  if (argc == 3) {
    new_superblock = atoi (argv[2]);
  }

  assert (switch_to_private_mount_table () == 0);

  if (argc == 2) {
    assert (cffs_init_superblock (dev, &new_superblock) == 0);
    printf ("got back superblock %d\n", new_superblock);
  }

  assert (mount_superblock (dev, new_superblock, "/unique") == 0);

  global_ftable->mounted_disks[dev] = 1;

  if (argc == 2) {
    system ("cp -r /benchmarks /unique");
  }
  system ("ls -lR /unique");

  __unmount ("/unique", 0);

  global_ftable->mounted_disks[dev] = 0;

  printf ("/unique unmounted\n");

  system ("ls /unique");
}
  
