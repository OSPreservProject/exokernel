#include "fs.h"
#include "dinode.h"
#include "dirent.h"

#include <fcntl.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

struct dir;

/* Arguments: disk, extent group, extent w/in group, capability, rdonly */
int ffs_init (u_int, u_int, u_int, u_int, int);
int ffs_shutdown (void);

int            ffs_open (const char *, int, int mode);
int            ffs_close (int);
int            ffs_read (int, void *, u_int);
int            ffs_write (int, void *, u_int);
int            ffs_lseek (int, int, int);
struct inode  *ffs_fstat (int);

struct dir    *ffs_opendir (const char *);
int            ffs_closedir (struct dir *);
struct dirent *ffs_readdir (struct dir *);

int            ffs_unlink (const char *);
struct inode  *ffs_stat (const char *);

int            ffs_mkdir (const char *name, int mode);
int            ffs_rmdir (const char *name);
