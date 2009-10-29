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

#include <xok/sysinfo.h>

#include "fd/proc.h"
#include "fd/path.h"

/* for cffs */
#include <fd/cffs/cffs.h>
#include <fd/cffs/cffs_proc.h>

#include <string.h>
#include <sys/mount.h>

#include <fd/mount_sup.h>
#include <exos/callcount.h>
#include <exos/cap.h>
#include <exos/vm-layout.h>
#include <exos/vm.h>
#include <exos/mallocs.h>

extern int nfs_getmount(char *hostname, char *path, struct file *filp);
extern int nfs_putmount(struct file *filp);
extern char *strncpy2 (char *dst, const char *src,size_t n);

/* mount_init: initializes mount data structures, assumes global_ftable
 * has been already set up.*/
int
mount_init(void) {
  int i;
  mounttable_t *mt;
  mount_entry_t *mnt;
  clear_mt_lock();
  mt = global_ftable->mt;
  global_ftable->remotedev = REMOTEDEVSTART;
  mt->nr_mentries = 0;
  mnt = mt->mentries;
  for (i = 0; i < NR_MOUNT; i++,mnt++) mnt->inuse = 0;
  return 0;
}

/* Change mount table
 *
 * Switch to a private new mount table.
 * This new table will be initialized to the contents
 * of the current mount table and will be inhereited
 * by child processes. Effectively, this gives a a process (hierchy) the 
 * ability to maintain a name space separate from the rest of the system.
 *
 */

int
switch_to_private_mount_table ()
{
  struct mounttable *new_mt;
  int ret;


  StaticAssert (sizeof (struct mounttable) <= NBPG);
  new_mt = (struct mounttable *)__malloc (NBPG);
  if (!new_mt) {
    return -1;
  }

  /* We only need to have mount table locked when we make a copy of it. */
  lock_mt ();
  /* XXX -- reference counts!!! */
  memcpy ((char *)new_mt, (char *)global_ftable->mt, sizeof (struct mounttable));
  unlock_mt ();

  ret = _exos_self_insert_pte (CAP_ROOT, vpt[PGNO((u_int )new_mt)] | PG_SHARED, 
                               MOUNT_SHARED_REGION, 0, NULL);

  /* this means we lost our mount table completely...really no
     good reason the above mapping should fail */

  assert (ret >= 0);

  /* from this point on we're using our private mount table */

  unlock_mt();                    /* it was locked when it was copied */

  /* remove the other mapping of the new mount table and tell
     the vm system that this virtual adress is free to be reused */

  ret = _exos_self_unmap_page (CAP_ROOT, (u_int )new_mt);
  assert (ret >= 0);

  __free (new_mt);

  return (0);
}


int
mount_superblock (u_int dev, u_int sb, char *path) {
  int slot = 0;
  struct file *filp = NULL, *dirp = NULL;
  mount_entry_t *m;
  int status;
  int is_root;
#define SRC_TEMPLATE "DISK%d"
  char src_buffer[6];

  lock_mt ();

  if (*path == '/' && *(path+1) == NULL)
    is_root = 1;
  else
    is_root = 0;

  if (!is_root) {
    if ((slot = getslot()) == -1) {
      errno = EMFILE;
      goto error1;
    }

    /* grab a slot in the mount table. */
    m = &global_ftable->mt->mentries[slot];
    dirp = &m->from;
    filp = &m->to;
    clear_filp_lock(filp);
    clear_filp_lock(dirp);
  
    status = namei(NULL,path,dirp,1);

    if (status) {
      errno = ENOENT;
      goto error1;
    }

    if ( ! S_ISDIR(dirp->f_mode)) { /* make sure is a directory */
      errno = ENOTDIR;
      goto error2;
    } 

    filp_refcount_init(dirp);
    filp_refcount_inc(dirp);
    dirp->f_flags |= O_DONOTRELEASE;
    sprintf (src_buffer, SRC_TEMPLATE, dev);
    strncpy2 (m->f_mntonname, path, MNTNAMELEN);
    strncpy2 (m->f_mntfromname, src_buffer, MNTNAMELEN);
    m->f_flags = MNT_LOCAL;
    m->sblk = sb;
  } else {

    filp = getfilp ();
    if (filp == NULL) {
      errno = ENFILE;
      goto error0;
    }

  }

#if 0
  if (is_root) {
    m->f_flags |= MNT_ROOTFS;
  }
#endif

  status = cffs_mount_superblock (dev, sb, 1);
  if (status == -1) {
    /* cffs_mount_superblock sets errno */
    goto error2;
  }
  cffs_grabRoot(dev, sb, filp);
  filp->f_flags |= O_DONOTRELEASE;

  if (!is_root) {
    setslot(slot);  
  } else {

    if (__current->root != 0) {
        filp_refcount_dec(__current->root);
    }
    __current->root = filp;
    filp_refcount_inc(filp);

    if (__current->cwd != 0) {
        filp_refcount_dec(__current->cwd);
    }
    __current->cwd = filp;
    __current->cwd_isfd = 0;
    filp_refcount_inc(filp);

  }

  unlock_mt ();

  return 0;

error2:
  if (is_root)
    putfilp (filp);
  else
    namei_release (dirp, 1);
error1:
  putslot(slot);
error0:
  unlock_mt ();
  return -1;
}

/* Mount system call. 
 *
 * fromname can be of the form:
 * host:path for NFS (for example zwolle:/disk/zw0) or
 * DISK# for CFFS (for example DISK1 or DISK14)
 */
int
mount(const char *dirto, const char *fromname) {
  mount_entry_t *m;
  struct file *filp;
  struct file *dirp;
  int status,slot;
  int disknum;
  char hostname[64],path[MAXPATHLEN];

  DPRINTF(SYS_LEVEL,("mount: entering\n"));

  lock_mt ();

  if ((slot = getslot()) == -1) {
    errno = EMFILE;
    goto error1;
  }
  /* grab a slot in the mount table. */
  m = &global_ftable->mt->mentries[slot];
  dirp = &m->from;
  filp = &m->to;
  clear_filp_lock(filp);
  clear_filp_lock(dirp);

  status = namei(NULL,dirto,dirp,1);
  if (status) {
    errno = ENOENT;
    goto error1;
  }

  if ( ! S_ISDIR(dirp->f_mode)) { /* make sure is a directory */
    errno = ENOTDIR;
    goto error2;
  }

  hostname[0] = path[0] = 0;
  disknum = -99;

  if (sscanf(fromname,"%[^:]:%s",hostname,path) == 2) {
    fprintf(stderr,"Mounting NFS %s:%s...",hostname,path);
    status = nfs_getmount(hostname,path,filp);
    if (status == -1) {
      goto error2;
    }
    m->f_flags = 0;
  } else if (sscanf(fromname,"DISK%d",&disknum) == 1) {

    if ((disknum < 0) || (disknum >= __sysinfo.si_ndisks)) {
      errno = ENODEV;
      goto error2;
    }

    if (global_ftable->mounted_disks[disknum]) {
      errno = EBUSY;
      goto error2;
    }

    status = cffs_mountFS(disknum);
    m->sblk = CFFS_SUPERBLKNO;
    if (status == -1) {
      /* cffs_mountFS sets errno */
      goto error2;
    }
    cffs_grabRoot(disknum, CFFS_SUPERBLKNO, filp);
    global_ftable->mounted_disks[disknum] = 1;
    m->f_flags = MNT_LOCAL;
  } else {
    errno = EINVAL;
    goto error1;
  }

  /* to make sure they are not released */
  dirp->f_flags |= O_DONOTRELEASE;
  filp_refcount_init(dirp);
  filp_refcount_inc(dirp);
  filp->f_flags |= O_DONOTRELEASE;
  strncpy2(m->f_mntonname,dirto,MNTNAMELEN);
  strncpy2(m->f_mntfromname,fromname,MNTNAMELEN);
  setslot(slot);

  unlock_mt ();
  return 0;

error2:
  namei_release (dirp, 1);
error1:
  putslot(slot);
  unlock_mt ();
  return -1;
}

/* umount system call
 *
 */
 
int
unmount (const char *path) {
  return __unmount (path, 0);
}

/* called with mount table locked */
static int
__unmount_a_fs (int i, int weak) {
  mounttable_t *mt;
  mount_entry_t *m;
  int error;

  mt = global_ftable->mt;
  m = &mt->mentries[i];

  if (!weak) {
    error = isbusy_fs(m->to.f_dev, m->to.op_type);
    assert (error >= 0);
    if (error > 1) {
      errno = EBUSY;
      return -1;
    }
  }

  if (m->to.op_type == NFS_TYPE) {
    /* NFS type */
    error = nfs_putmount(&m->to);
    assert (error == 0);
  } else if (m->to.op_type == CFFS_TYPE) {
    /* Disk */
    if (!weak) {
      if (global_ftable->mounted_disks[m->to.f_dev] != 1) {
        errno = EINVAL;
        return -1;
      }
    }
    cffs_unmount_superblock (m->to.f_dev, m->sblk, weak);
    if (!weak) {
      global_ftable->mounted_disks[m->to.f_dev] = 0;
    }
  } else {
    assert (0);
  }

  namei_release (&m->from, 1);
  putslot(i);
  return 0;
}

int
__unmount_all () {
  int i;
  int error;

  lock_mt ();

  for (i = 0; i < maxslot (); i++) {
    if (!isslotset (i)) continue;
    error = __unmount_a_fs (i, 1);
    if (error) {
      unlock_mt ();
      return -1;
    }
  }

  unlock_mt ();
  return 0;
}

static int find_mount_entry_by_filp (struct file *filp) {
  int i;
  mounttable_t *mt;
  mount_entry_t *m;

  mt = global_ftable->mt;
  m = mt->mentries;

  for (i = 0; i < mt->nr_mentries; i++,m++) {
    if (!isslotset (i)) continue;
    if (FILPEQ(filp,&m->to)) {
      return (i);
    }
  }
  return -1;
}  

int
__unmount(const char *path, int weak) {
  int i, error;
  struct file filp;
  mounttable_t *mt;
  mount_entry_t *m;

  lock_mt ();
  
  mt = global_ftable->mt;
  m = mt->mentries;

  DPRINTF(SYS_LEVEL,("unmount: entering\n"));

  /* if we're trying to unmount the root,
     just return */

  if (*path == '/' && *(path+1) == NULL) {
    unlock_mt ();
    return 0;
  }

  error = namei(NULL,path,&filp,1);
  if (error) {
    unlock_mt ();
    errno = ENOENT;
    return -1;
  }
  
  i = find_mount_entry_by_filp (&filp);
  namei_release (&filp, 1);
  if (i == -1) {
    unlock_mt ();
    errno = EINVAL;
    return -1;
  }

  error = __unmount_a_fs (i, weak);
  unlock_mt ();
  return error;
}

/* XXX Everything below this point is completely
   hacked and fairly wrong. */

static struct statfs const sb = {
  0,             /* sb->f_type,     */
  0,             /* sb->f_flags,     */
  4096,          /* sb->f_bsize,     */
  4096,          /* sb->f_iosize,     */
  47643,         /* sb->f_blocks,     */
  29659,         /* sb->f_bfree,     */
  27276,         /* sb->f_bavail,     */
  12094,         /* sb->f_files,     */
  10468,         /* sb->f_ffree,     */
  1024,          /* sb->f_fsid,     */
  0,             /* sb->f_owner,     */
  {0,0,0,0},     /* sb->f_spare */
  "",            /* sb->f_fstypename,     */
  "cffs",        /* sb->f_mntonname,     */
  "/"            /* sb->f_mntfromname     */
};

int
fstatfs (int fd, struct statfs *b) {
  struct file *filp;

  OSCALLENTER(OSCALL_fstatfs);
  *b = sb;
  CHECKFD(fd, OSCALL_fstatfs);
  filp = __current->fd[fd];
  if (filp->f_dev < 0) {
    strcpy(b->f_fstypename,"nfs");
  } else {
    strcpy(b->f_fstypename,"cffs");
  }
  /* XXX need to set flags here */

  OSCALLEXIT(OSCALL_fstatfs);
  return 0;
}

int
statfs (const char *path, struct statfs *b) {
  struct file filp;
  int error;

  OSCALLENTER(OSCALL_statfs);
  *b = sb;

  /* lookup the pathname */
  error = namei(NULL,path,&filp,1);
  if (error) {
    errno = ENOENT;
    goto error;
  }
  if (!(S_ISDIR(filp.f_mode))) {
    errno = ENOTDIR;
    goto error;
  }

  if (filp.f_dev < 0) {
    strcpy(b->f_fstypename,MOUNT_NFS);
  } else {
    strcpy(b->f_fstypename,MOUNT_FFS);
  }
  /* XXX need to set flags here */

  namei_release (&filp, 1);

  OSCALLEXIT(OSCALL_statfs);
  return 0;

 error:
  namei_release (&filp, 1);
  return -1;
}

int
getfsstat (struct statfs *a, long b, int c) {
  OSCALLENTER(OSCALL_getfsstat);
  demand(0, getfsstat not supported yet);
  OSCALLEXIT(OSCALL_getfsstat);
  return 0;

}
