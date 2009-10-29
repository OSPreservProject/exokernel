
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

//#undef PRINTF_LEVEL
//#define PRINTF_LEVEL 99
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>

#include <sys/times.h>

#include <fd/proc.h>
#include <fd/path.h>
#include <exos/debug.h>
#include "nfs_rpc.h"
#include "nfs_rpc_procs.h"
#include "nfs_xdr.h"
#include "errno.h"

/*
 * One function for each procedure in the NFS protocol.
 */

static char *
get_file_type(int typenum);

/* nfs_proc_getattr

 gets the attributes of a file handler.
 returns the number of byte written or an error.
 */

int 
nfs_proc_getattr(struct nfs_fh *fhandle, struct nfs_fattr *fattr) {
  int *p, *p0;
  int status;
  int ruid = 0;
  struct generic_server *server = fhandle->server;

  DPRINTF(CLUHELP_LEVEL,
	  ("NFS call getattr\n"));
  if (!(p0 = overhead_rpc_alloc(server->rsize)))
    return -EIO;
 retry:	/* soon we'll get rid of this label */
  p = nfs_rpc_header(p0, NFSPROC_GETATTR, ruid);
  p = xdr_encode_fhandle(p, fhandle);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    p = xdr_decode_fattr(p, fattr);
    DPRINTF(CLUHELP_LEVEL,("NFS reply getattr\n"));
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,
	    ("NFS reply getattr failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}

int 
nfs_proc_setattr(struct nfs_fh *fhandle,
		 struct nfs_sattr *sattr, struct nfs_fattr *fattr) {
  int *p, *p0;
  int status;
  int ruid = 0;
  struct generic_server *server = fhandle->server;

  DPRINTF(CLUHELP_LEVEL,("NFS call  setattr\n"));
  if (!(p0 = overhead_rpc_alloc(server->wsize)))
    return -EIO;
 retry:
  p = nfs_rpc_header(p0, NFSPROC_SETATTR, ruid);
  p = xdr_encode_fhandle(p, fhandle);
  p = xdr_encode_sattr(p, sattr);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    p = xdr_decode_fattr(p, fattr);
    DPRINTF(CLUHELP_LEVEL,("NFS reply setattr Ok\n"));
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      kprintf("RETRIED\n");
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,
	    ("NFS reply setattr failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}

int 
nfs_proc_lookup(struct nfs_fh *dir, 
		const char *name,
		struct nfs_fh *fhandle, struct nfs_fattr *fattr) {
  int *p, *p0;
  int status;
  int ruid = 0;
  struct generic_server *server = dir->server;

  DPRINTF(CLUHELP_LEVEL,
	  ("NFS call lookup \"%s\"\n", name));

  if (!(p0 = overhead_rpc_alloc(server->rsize)))
    return -EIO;
 retry:	/* soon to be removed */
  p = nfs_rpc_header(p0, NFSPROC_LOOKUP, ruid);
  p = xdr_encode_fhandle(p, dir);
  p = xdr_encode_string(p, name);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    p = xdr_decode_fhandle(p, fhandle);
    fhandle->server = dir->server; /* copy the servers */
    p = xdr_decode_fattr(p, fattr);
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,("NFS reply lookup failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}

int 
nfs_proc_readlink(struct nfs_fh *fhandle, char *res) {
  int *p, *p0;
  int status;
  int ruid = 0;
  struct generic_server *server = fhandle->server;

  DPRINTF(CLUHELP_LEVEL,("NFS call  readlink\n"));
  if (!(p0 = overhead_rpc_alloc(server->rsize)))
    return -EIO;
 retry:
  p = nfs_rpc_header(p0, NFSPROC_READLINK, ruid);
  p = xdr_encode_fhandle(p, fhandle);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    if (!(p = xdr_decode_string(p, res, NFS_MAXPATHLEN))) {
      DPRINTF(CLUHELP_LEVEL,("nfs_proc_readlink: giant pathname\n"));
      status = NFSERR_IO;
    }
    else
      DPRINTF(CLUHELP_LEVEL,("NFS reply readlink %s\n", res));
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,
	    ("NFS reply readlink failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}


int 
nfs_proc_create(struct nfs_fh *dir, 
		const char *name, struct nfs_sattr *sattr,
		struct nfs_fh *fhandle, struct nfs_fattr *fattr) {
  int *p, *p0;
  int status;
  int ruid = 0;
  struct generic_server *server = dir->server; /* modified this 11/13 */


  DPRINTF(CLUHELP_LEVEL,("NFS call  create %s\n", name));
  if (!(p0 = overhead_rpc_alloc(server->wsize)))
    return -EIO;
 retry:
  p = nfs_rpc_header(p0, NFSPROC_CREATE, ruid);
  p = xdr_encode_fhandle(p, dir);
  p = xdr_encode_string(p, name);
  p = xdr_encode_sattr(p, sattr);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    p = xdr_decode_fhandle(p, fhandle);
    fhandle->server = dir->server; /* copy the servers */
    p = xdr_decode_fattr(p, fattr);
    DPRINTF(CLUHELP_LEVEL,("NFS reply create\n"));
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,("NFS reply create failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}

int 
nfs_proc_remove(struct nfs_fh *dir, const char *name) {
  int *p, *p0;
  int status;
  int ruid = 0;
  struct generic_server *server = dir->server;


  DPRINTF(CLUHELP_LEVEL,("NFS call  remove %s\n", name));
  if (!(p0 = overhead_rpc_alloc(server->wsize)))
    return -EIO;
 retry:
  p = nfs_rpc_header(p0, NFSPROC_REMOVE, ruid);
  p = xdr_encode_fhandle(p, dir);
  p = xdr_encode_string(p, name);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    DPRINTF(CLUHELP_LEVEL,("NFS reply remove\n"));
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,("NFS reply remove failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}

int 
nfs_proc_rename(struct nfs_fh *old_dir, const char *old_name,
		struct nfs_fh *new_dir, const char *new_name) {
  int *p, *p0;
  int status;
  int ruid = 0;
  struct generic_server *server = old_dir->server;


  DPRINTF(CLUHELP_LEVEL,("NFS call  rename %s -> %s\n", old_name, new_name));
  if (!(p0 = overhead_rpc_alloc(server->wsize)))
    return -EIO;
 retry:
  p = nfs_rpc_header(p0, NFSPROC_RENAME, ruid);
  p = xdr_encode_fhandle(p, old_dir);
  p = xdr_encode_string(p, old_name);
  p = xdr_encode_fhandle(p, new_dir);
  p = xdr_encode_string(p, new_name);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    DPRINTF(CLUHELP_LEVEL,("NFS reply rename\n"));
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,("NFS reply rename failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}

int 
nfs_proc_link(struct nfs_fh *fhandle,
	      struct nfs_fh *dir, const char *name) {
  int *p, *p0;
  int status;
  int ruid = 0;
  struct generic_server *server = fhandle->server;

  DPRINTF (CLUHELP_LEVEL, ("NFS call  link %s\n", name));
  if (!(p0 = overhead_rpc_alloc(server->wsize)))
    return -EIO;
 retry:
  p = nfs_rpc_header(p0, NFSPROC_LINK, ruid);
  p = xdr_encode_fhandle(p, fhandle);
  p = xdr_encode_fhandle(p, dir);
  p = xdr_encode_string(p, name);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    DPRINTF(CLUHELP_LEVEL,("NFS reply link\n"));
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,("NFS reply link failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}

int 
nfs_proc_symlink(struct nfs_fh *dir, const char *name, 
		 const char *path, struct nfs_sattr *sattr) {
  int *p, *p0;
  int status;
  int ruid = 0;
  struct generic_server *server = dir->server;


  DPRINTF(CLUHELP_LEVEL,("NFS call  symlink %s -> %s\n", name, path));
  if (!(p0 = overhead_rpc_alloc(server->wsize)))
    return -EIO;
 retry:
  p = nfs_rpc_header(p0, NFSPROC_SYMLINK, ruid);
  p = xdr_encode_fhandle(p, dir);
  p = xdr_encode_string(p, name);
  p = xdr_encode_string(p, path);
  p = xdr_encode_sattr(p, sattr);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    DPRINTF(CLUHELP_LEVEL,("NFS reply symlink\n"));
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,("NFS reply symlink failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}

int 
nfs_proc_mkdir(struct nfs_fh *dir,
	       const char *name, struct nfs_sattr *sattr,
	       struct nfs_fh *fhandle, struct nfs_fattr *fattr) {
  int *p, *p0;
  int status;
  int ruid = 0;
  struct generic_server *server = dir->server; /* modified 11/13 */


  DPRINTF(CLUHELP_LEVEL,("NFS call  mkdir %s\n", name));
  if (!(p0 = overhead_rpc_alloc(server->wsize)))
    return -EIO;
 retry:
  p = nfs_rpc_header(p0, NFSPROC_MKDIR, ruid);
  p = xdr_encode_fhandle(p, dir);
  p = xdr_encode_string(p, name);
  p = xdr_encode_sattr(p, sattr);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = -NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    p = xdr_decode_fhandle(p, fhandle);
    fhandle->server = dir->server; /* copy the servers */
    p = xdr_decode_fattr(p, fattr);
    DPRINTF(CLUHELP_LEVEL,("NFS reply mkdir\n"));
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,("NFS reply mkdir failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}

int 
nfs_proc_rmdir(struct nfs_fh *dir, const char *name) {
  int *p, *p0;
  int status;
  int ruid = 0;
  struct generic_server *server = dir->server;

  DPRINTF(CLUHELP_LEVEL,("NFS call  rmdir %s\n", name));
  if (!(p0 = overhead_rpc_alloc(server->wsize)))
    return -EIO;
 retry:
  p = nfs_rpc_header(p0, NFSPROC_RMDIR, ruid);
  p = xdr_encode_fhandle(p, dir);
  p = xdr_encode_string(p, name);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    DPRINTF(CLUHELP_LEVEL,("NFS reply rmdir\n"));
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,("NFS reply rmdir failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}

int 
nfs_proc_readdir(struct nfs_fh *fhandle,
		 int cookie, int count, struct nfs_entry *entry) {
  int *p, *p0;
  int status;
  int ruid = 0;
  int i = 0;			/* = 0 is for gcc */
  int size;
  int eof;
  struct generic_server *server = fhandle->server;

  DPRINTF(CLUHELP_LEVEL,("NFS call  readdir %d @ %d\n", count, cookie));
  size = server->rsize;
  if (!(p0 = overhead_rpc_alloc(server->rsize)))
    return -EIO;
 retry:
  p = nfs_rpc_header(p0, NFSPROC_READDIR, ruid);
  p = xdr_encode_fhandle(p, fhandle);
  *p++ = htonl(cookie);
  *p++ = htonl(size);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    for (i = 0; i < count && *p++; i++) {
      if (!(p = xdr_decode_entry(p, entry++)))
	break;
    }
    if (!p) {
      DPRINTF(CLUHELP_LEVEL,("nfs_proc_readdir: giant filename\n"));
      status = NFSERR_IO;
    }
    else {
      eof = (i == count && !*p++ && *p++)
	|| (i < count && *p++);
      if (eof && i)
	entry[-1].eof = 1;
      DPRINTF(CLUHELP_LEVEL,("NFS reply readdir %d %s\n", i,
	     eof ? "eof" : ""));
    }
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,("NFS reply readdir failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return (status == NFS_OK) ? i : nfs_stat_to_errno(status);
}

int 
nfs_proc_statfs(struct nfs_fh *fhandle, struct nfs_fsinfo *res) {
  int *p, *p0;
  int status;
  int ruid = 0;
  struct generic_server *server = fhandle->server;

  printf("NFS call  statfs\n");
  if (!(p0 = overhead_rpc_alloc(server->rsize)))
    return -EIO;
 retry:
  p = nfs_rpc_header(p0, NFSPROC_STATFS, ruid);
  p = xdr_encode_fhandle(p, fhandle);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    p = xdr_decode_fsinfo(p, res);
    DPRINTF(CLUHELP_LEVEL,("NFS reply statfs\n"));
  }
  else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,("NFS reply statfs failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}


/*
 * We need to translate between nfs status return values and
 * the local errno values which may not be the same.
 */

static struct {
  int stat;
  int errno;
} nfs_errtbl[] = {
  { NFS_OK,		0		},
  { NFSERR_PERM,		EPERM		},
  { NFSERR_NOENT,		ENOENT		},
  { NFSERR_IO,		EIO		},
  { NFSERR_NXIO,		ENXIO		},
  { NFSERR_ACCES,		EACCES		},
  { NFSERR_EXIST,		EEXIST		},
  { NFSERR_NODEV,		ENODEV		},
  { NFSERR_NOTDIR,	ENOTDIR		},
  { NFSERR_ISDIR,		EISDIR		},
  { NFSERR_INVAL,		EINVAL		},
  { NFSERR_FBIG,		EFBIG		},
  { NFSERR_NOSPC,		ENOSPC		},
  { NFSERR_ROFS,		EROFS		},
  { NFSERR_NAMETOOLONG,	ENAMETOOLONG	},
  { NFSERR_NOTEMPTY,	ENOTEMPTY	},
  { NFSERR_DQUOT,		EDQUOT		},
  { NFSERR_STALE,		ESTALE		},
#ifdef EWFLUSH
  { NFSERR_WFLUSH,	EWFLUSH		},
#endif
  { -1,			EIO		}
};

int 
nfs_stat_to_errno(int stat) {
  int i;
  for (i = 0; nfs_errtbl[i].stat != -1; i++) {
    if (nfs_errtbl[i].stat == stat)
      return nfs_errtbl[i].errno; /* - hector - changed sign */
  }
  printf("nfs_stat_to_errno: bad nfs status return value: %d\n", stat);
  return stat; /* - hector - changed sign */
}




static char *get_file_type(int typenum)
{
  switch (typenum) {
  case NFNON:     return("NFNON");    break;
  case NFREG:     return("NFREG");    break;
  case NFDIR:     return("NFDIR");    break;
  case NFBLK:     return("NFBLK");    break;
  case NFCHR:     return("NFCHR");    break;
  case NFLNK:     return("NFLNK");    break;
  case NFSOCK:    return("NFSOCK");   break;
  case NFBAD:     return("NFBAD");    break;
  case NFFIFO:    return("NFFIFO");   break;
  default:        return("unknown type");    break;
  }
}
/* for debugging purposes */

void print_fattr (struct nfs_fattr *fattr)
{
  char *ftype;
  ftype = get_file_type(fattr->type);

  printf("FILE ATTRIBUTES:
type: %2d (%s) mode: %o (%d)
nlink: %2d uid: %5d gid: %5d size: %d
blocksize: %d rdev: %d blocks: %d fsid: %d fileid: %d
atime: %d.%06d mtime %d.%06d ctime: %d.%06d\n",
	 fattr->type,ftype,fattr->mode,fattr->mode,fattr->nlink,fattr->uid,
	 fattr->gid,fattr->size,
	 fattr->blocksize,fattr->rdev,fattr->blocks,fattr->fsid,fattr->fileid,
	 fattr->atime.seconds, fattr->atime.useconds,
	 fattr->mtime.seconds, fattr->mtime.useconds,
	 fattr->ctime.seconds, fattr->ctime.useconds);
};
