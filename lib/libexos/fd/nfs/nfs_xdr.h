
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

#ifndef _NFSXDR_H_
#define _NFSXDR_H_

/* xdr.c */

/*
 * Here are a bunch of xdr encode/decode functions that convert
 * between machine dependent and xdr data formats.
 */

/* uncomment next line if you want xdr procedures inlined */
//#define NFS_XDR_INLINE

#ifndef NFS_XDR_INLINE
inline int *
xdr_encode_fhandle(int *p, void *fhandle);

inline int *
xdr_decode_fhandle(int *p, void *fhandle);

inline int *
xdr_encode_string(int *p, const char *string);

inline int *
xdr_decode_string(int *p, char *string, int maxlen);

inline int *
xdr_encode_data(int *p, char *data, int len);

inline int *
xdr_decode_data(int *p, char *data, int *lenp, int maxlen);

int *
xdr_decode_fattr(int *p, struct nfs_fattr *fattr);

int *
xdr_encode_sattr(int *p, struct nfs_sattr *sattr);

int *
xdr_decode_sattr(int *p, struct nfs_sattr *sattr);

int *
xdr_decode_entry(int *p, struct nfs_entry *entry);

int *
xdr_decode_fsinfo(int *p, struct nfs_fsinfo *res);
#else

/* assumption len < 4, and c = 0 which is true for the xdr procedures that use it */
static inline void memset14(char *foo, unsigned char c, int len) {
  switch(len) {
  case 3:
    foo[2] = 0;
  case 2:
    foo[1] = 0;
  case 1:
    foo[0] = 0;
  case 0:
    break;
  }
}

static inline int *xdr_encode_fhandle(int *p, void *fhandle)
{
  /*   *((struct nfs_fh *) p) = *fhandle;
  p += (sizeof (*fhandle) + 3) >> 2;
  return p; */

  memcpy(p,fhandle,NFS_FHSIZE);
  p += (NFS_FHSIZE + 3) >> 2;
  return p;
}

static inline int *xdr_decode_fhandle(int *p, void *fhandle)
{
  /*   *fhandle = *((struct nfs_fh *) p);
  p += (sizeof (*fhandle) + 3) >> 2;
  return p; */

  memcpy(fhandle,p,NFS_FHSIZE);
  p += (NFS_FHSIZE + 3) >> 2;
  return p;
}

static inline int *xdr_encode_string(int *p, const char *string)
{
  int len, quadlen;
	
  len = strlen(string);
  quadlen = (len + 3) >> 2;
  *p++ = htonl(len);
  memcpy((char *) p, (char *)string, len);
  memset14(((char *) p) + len, '\0', (quadlen << 2) - len);
  p += quadlen;
  return p;
}

static inline int *xdr_decode_string(int *p, char *string, int maxlen)
{
  unsigned int len;

  len = ntohl(*p++);
  if (len > maxlen)
    return NULL;
  memcpy(string, (char *) p, len);
  string[len] = '\0';
  p += (len + 3) >> 2;
  return p;
}

static inline int *xdr_encode_data(int *p, char *data, int len)
{
  int quadlen;
	
  quadlen = (len + 3) >> 2;
  *p++ = htonl(len);
  memcpy((char *) p, data, len);
  memset14(((char *) p) + len, '\0', (quadlen << 2) - len);
  p += quadlen;
  return p;
}

static inline int *xdr_decode_data(int *p, char *data, int *lenp, int maxlen)
{
  unsigned int len;

  len = *lenp = ntohl(*p++);
  if (len > maxlen)
    return NULL;
  memcpy(data, (char *) p, len);
  p += (len + 3) >> 2;
  return p;
}

static inline int *xdr_decode_fattr(int *p, struct nfs_fattr *fattr)
{
  fattr->type = (enum nfs_ftype) ntohl(*p++);
  fattr->mode = ntohl(*p++);
  fattr->nlink = ntohl(*p++);
  fattr->uid = ntohl(*p++);
  fattr->gid = ntohl(*p++);
  fattr->size = ntohl(*p++);
  fattr->blocksize = ntohl(*p++);
  fattr->rdev = ntohl(*p++);
  fattr->blocks = ntohl(*p++);
  fattr->fsid = ntohl(*p++);
  fattr->fileid = ntohl(*p++);
  fattr->atime.seconds = ntohl(*p++);
  fattr->atime.useconds = ntohl(*p++);
  fattr->mtime.seconds = ntohl(*p++);
  fattr->mtime.useconds = ntohl(*p++);
  fattr->ctime.seconds = ntohl(*p++);
  fattr->ctime.useconds = ntohl(*p++);
  return p;
}

static inline int *xdr_encode_sattr(int *p, struct nfs_sattr *sattr)
{
  *p++ = htonl(sattr->mode);
  *p++ = htonl(sattr->uid);
  *p++ = htonl(sattr->gid);
  *p++ = htonl(sattr->size);
  *p++ = htonl(sattr->atime.seconds);
  *p++ = htonl(sattr->atime.useconds);
  *p++ = htonl(sattr->mtime.seconds);
  *p++ = htonl(sattr->mtime.useconds);
  return p;
}

static inline int *xdr_decode_entry(int *p, struct nfs_entry *entry)
{
  entry->fileid = ntohl(*p++);
  if (!(p = xdr_decode_string(p, entry->name, NFS_MAXNAMLEN)))
    return NULL;
  entry->cookie = ntohl(*p++);
  entry->eof = 0;
  return p;
}

static inline int *xdr_decode_fsinfo(int *p, struct nfs_fsinfo *res)
{
  res->tsize = ntohl(*p++);
  res->bsize = ntohl(*p++);
  res->blocks = ntohl(*p++);
  res->bfree = ntohl(*p++);
  res->bavail = ntohl(*p++);
  return p;
}

#endif
#endif
