
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

#ifndef _XOK_CAPABILITY_H_
#define _XOK_CAPABILITY_H_

#include <xok/types.h>
#ifdef KERNEL
#include <xok_include/string.h>
#else
#include <string.h>
#endif
#include <xok/kerrno.h>

/* capability name */
typedef struct Capability {
  u_char c_perm : 3;	   /* Access rights to resource */
/* Permissions bits for capabilities in access control lists: */
#define ACL_ALL 0x7	   /* Allow this capability all access */
#define ACL_MOD 0x4	   /* This capability allows modification of the ACL */
#define ACL_W 0x1	   /* This acl entry grants write access to resource */
#define ACL_R 0x0	   /* default */
/* Permissions bits for capabilities in environment clists: */
#define CL_ALL 0x7	   /* This capability is complete */
#define CL_DUP 0x4	   /* This capability can be duplicated */
#define CL_W 0x1	   /* This capability can be used for writing */
  u_char c_valid : 1;	   /* This capibility of ACL entry is valid */
  u_char c_isptr : 1;	   /* This is a pointer to a list of capabilities */
  u_char c_len : 3;	   /* Length of the name */
/* An array of seven (7--I'm cheating) bytes naming the capability if
 * it is not a pointer: */
  u_char c_name[1];
/* The length and address of the capability list if this is a pointer: */
  u_short cl_len;
  struct Capability *cl_list;
} cap;

#define cap_null (cap) {0, 0, 0, 0, {0}, 0, 0}

/* initialize an acl */
static inline void
acl_init (cap *acl, u_int len)
{
  int i;

  for (i = 0; i < len; i++) {
    acl->c_valid = 0;
  }
}

/* insert a capability onto an acl */
static inline int 
acl_add (cap *acl, u_int len, cap *new_entry, int *error) {
  int i;

  for (i = 0; i < len; i++) {
    if (!acl->c_valid) {
      acl[i] = *new_entry;
      return 0;
    }
  }

  *error = -E_ACL_SIZE;
  return 1;
}

/* initialize c to be a valid zero length capability with all perms */
static inline void
cap_init_zerolength (cap *c)
{
  c->c_valid = 1;
  c->c_perm = CL_ALL;
  c->c_len = 0;
  c->c_isptr = 0;
}


/* initialize c to be the lowest capability */
static inline void
cap_init_lowest (cap *c, int access) {
  c->c_valid = 1;
  c->c_perm = access;
  c->c_len = 7;
  c->c_isptr = 0;
  memset (c->c_name, 0xff, 7);
}

static inline int cap_isroot (cap *c)
{
   return ((c->c_valid) && (c->c_len == 0) && (c->c_perm == CL_ALL));
}

/* Returns 1 if c1 valid and dominates valid c2, 0 otherwise. */
static inline int
cap_dominates (cap *c1, cap *c2)
{
  if (! c1->c_valid || c1->c_isptr
      || !c2->c_valid || c2->c_isptr
      || c1->c_len > c2->c_len
      || (c2->c_perm & c1->c_perm) != c2->c_perm)
    return (0);
  return (! memcmp (c1->c_name, c2->c_name, c1->c_len));
}

#ifdef KERNEL

int acl_access (cap *, cap *, u_short, const u_char);
int acl_read (cap *acl, u_short acl_len, cap *res, u_short res_len);
int acl_setsize (cap *, u_short, u_short);
int acl_mod (cap *, cap *, u_short, u_short, cap *);


#endif /* KERNEL */

#define MAX_CAPNO 0x13	   /* Number of capabilities in an environment */

/* list of capabilities kernel uses */

/* format of a disk capability is CAP_NM_DISK.partno.micropartno */
#define CAP_NM_DISK 128

/* format of a network capability is CAP_NM_DPF.* */
#define CAP_NM_DPF  192

#endif /* !_XOK_CAPABILITY_H_ */
