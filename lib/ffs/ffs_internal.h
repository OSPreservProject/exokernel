#include <machine/param.h>
#include "disk.h"
#include "ffs.h"
#define KERNEL
#include "inode.h"

#define LINK_MAX 32767
#define DIRBLKSIZ DEV_BSIZE
#define DIRBLKMSK (DIRBLKSIZ-1)

#ifndef howmany                                                           
#define howmany(x, y)   (((x)+((y)-1))/(y))
#endif                                                                       

/* requires y is a power of 2 */
#ifndef roundup
#define roundup(x, y)	(((x)+(y)-1)&~((y)-1))
#endif

#define MAX_FDS 2
extern struct fd_info
{
  int used, flags;
  struct inode in;
  u_int offset;
} fds[MAX_FDS];

struct dir
{
  char buf[8192];
  struct inode in, ain;
  u_int offset, block;
};

extern u_int diskblocks;
extern struct fs *fs;
extern char *ffs_space;

/* ffs_internal.c */
int  ffs_getinode (ino_t, struct inode *);
int  ffs_writeinode (struct inode *);
int  ffs_getdinode (ino_t, struct dinode *);
int  ffs_blkatoff (struct inode *, int, char **, char **);
int  ffs_wblkatoff (struct inode *, int, char *);

int  ffs_lookup (const char *, struct inode *, int);
#define LOOKUP_EMPTY 1
int  ffs_insert (const char *, struct inode *, struct inode *, int);
int  ffs_delete (const char *, struct inode *);
int  ffs_dirempty (struct inode *);
int  ffs_clear (struct inode *);
int  ffs_clearblk (daddr_t, int);

/* ffs_alloc.c: */
void    ffs_blkfree (struct inode *, daddr_t, long);
int     ffs_ialloc (register struct inode *, register struct inode *, int);
int     ffs_alloc (register struct inode *, daddr_t, int, daddr_t *);
int     ffs_realloccg (register struct inode *, daddr_t, daddr_t, int, int, daddr_t *);
int     ffs_free (register struct inode *, ino_t, int);
daddr_t ffs_blkpref (struct inode *, daddr_t, int, daddr_t *);

/* ffs_balloc.c */
int ffs_balloc (register struct inode *, register daddr_t, int, int, daddr_t*);

/* ffs_bmap.c */
int ffs_bmap (struct inode *, daddr_t, daddr_t *);
int ffs_getlbns (register daddr_t, struct indir *, int *);

/* ffs_subr.c */
void ffs_fragacct (struct fs *, int, int32_t [], int);
int  ffs_isblock (struct fs *, unsigned char *, daddr_t);
void ffs_clrblock (struct fs *, u_char *, daddr_t);
void ffs_setblock (struct fs *, u_char *, daddr_t);
int  scanc (u_int, register u_char *, register u_char [], register u_char);
int  skpc (register int, u_int, register u_char *);

#define        NBBY    8               /* number of bits in a byte */

/* Bit map related macros. */
#define	setbit(a,i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a,i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a,i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a,i)	(((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)
