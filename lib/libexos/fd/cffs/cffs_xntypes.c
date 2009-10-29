
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

#include <exos/conf.h>

#ifdef XN

#include "cffs.h"
#include "cffs_embdir.h"
#include "cffs_xntypes.h"
#include <stddef.h>
#include <stdlib.h>

#include <ubb/xn.h>
#include <ubb/ubb.h>
#include <ubb/root-catalogue.h>
#include <ubb/lib/ubb-lib.h>
#include <ubb/lib/mem.h>
#include "spec.h"
#include "sugar.h"

#ifdef JJ
#include <ubb/lib/demand.h>
#else
#include <assert.h>
#define fatal(a)	demand(0,a)
#endif

int xn_import(struct udf_type *t);
int xn_union(struct udf_type* type, struct udf_fun*, int arms, ...);
int xn_interpret_raw(struct udf_type* type, int offset, int size,
		     struct udf_fun* fun);
struct udf_fun* cffs_interpret_sector();
struct udf_fun* cffs_discriminate_inode();

int cffs_super_t;
int cffs_inode_t;

int cffs_data_t = XN_DB;
int cffs_inode_data_t;
int cffs_sindirect_data_t;
int cffs_dindirect_data_t;
int cffs_tindirect_data_t;

int cffs_dir_t = 16;		/* I won't have to do this later -JJJ */
int cffs_inode_dir_t;
int cffs_sindirect_dir_t;
int cffs_dindirect_dir_t;
int cffs_tindirect_dir_t;

#define XN_FIELD(name, type) xn_fixed_seq(&t, field(name), type)

  /* curry the macros */
#define offset(field) 	offsetof(TYPE, field)
#define size(field) 	(sizeof ((TYPE *)0)->field)
#define field(name)     offset(name) , size(name), 1
#define array(name,num) offset(name) , size(name[0]), num

int specify_cffs_types(void) {
  struct udf_type t;
  int cffs_super_fields_t;
  int cffs_heading_t;
  int cffs_sector_t;
  int i;

	/* This is the staticly allocated space in superblock */
  StaticAssert (CFFS_XN_NUMTYPES <= 16);

#define blockof(type) \
  xn_fixed_seq(&t, 0, sizeof(db_t), BLOCK_SIZE/sizeof(db_t), type);


  xn_begin(&t, "CFFS indirect block (single, data)", XN_STICKY); {
    blockof(cffs_data_t);
  } xn_end(&t, BLOCK_SIZE);
  if ((cffs_sindirect_data_t = xn_import(&t)) < 0)
    return cffs_sindirect_data_t;

  xn_begin(&t, "CFFS indirect block (double, data)", XN_STICKY); {
    blockof(cffs_sindirect_data_t);
  } xn_end(&t, BLOCK_SIZE);
  if ((cffs_dindirect_data_t = xn_import(&t)) < 0)
    return cffs_dindirect_data_t;

  xn_begin(&t, "CFFS indirect block (triple, data)", XN_STICKY); {
    blockof(cffs_dindirect_data_t);
  } xn_end(&t, BLOCK_SIZE);
  if ((cffs_tindirect_data_t = xn_import(&t)) < 0)
    return cffs_tindirect_data_t;


  xn_begin(&t, "CFFS indirect block (single, directory)", XN_STICKY); {
    blockof(cffs_dir_t);
  } xn_end(&t, BLOCK_SIZE);
  if ((cffs_sindirect_dir_t = xn_import(&t)) < 0)
    return cffs_sindirect_dir_t;

  xn_begin(&t, "CFFS indirect block (double, directory)", XN_STICKY); {
    blockof(cffs_sindirect_dir_t);
  } xn_end(&t, BLOCK_SIZE);
  if ((cffs_dindirect_dir_t = xn_import(&t)) < 0)
    return cffs_dindirect_dir_t;

  xn_begin(&t, "CFFS indirect block (triple, directory)", XN_STICKY); {
    blockof(cffs_dindirect_dir_t);
  } xn_end(&t, BLOCK_SIZE);
  if ((cffs_tindirect_dir_t = xn_import(&t)) < 0)
    return cffs_tindirect_dir_t;
#undef blockof

  xn_begin(&t, "CFFS sector heading", XN_STICKY); {
    //    xn_interpret_raw(&t, 0, 128, cffs_interpret_sector());
    xn_fixed_seq(&t, 0, 128, 1, XN_BYTES);
  } xn_end(&t, 128);
  if ((cffs_heading_t = xn_import(&t)) < 0)
    return cffs_heading_t;


#define TYPE dinode_t
  xn_begin(&t, "CFFS inode (data)", XN_NOT_STICKY); {
    xn_fixed_seq(&t, field(dinodeNum), XN_BYTES);
    xn_fixed_seq(&t, field(length), XN_BYTES);
    xn_fixed_seq(&t, field(type), XN_BYTES);
    xn_fixed_seq(&t, field(openCount), XN_BYTES);
    xn_fixed_seq(&t, field(uid), XN_BYTES);
    xn_fixed_seq(&t, field(gid), XN_BYTES);
    xn_fixed_seq(&t, field(linkCount), XN_BYTES);
    xn_fixed_seq(&t, field(mask), XN_BYTES);
    xn_fixed_seq(&t, field(accTime), XN_BYTES);
    xn_fixed_seq(&t, field(modTime), XN_BYTES);
    xn_fixed_seq(&t, field(creTime), XN_BYTES);
    xn_fixed_seq(&t, array(directBlocks, NUM_DIRECT), cffs_data_t);
    xn_fixed_seq(&t, field(indirectBlocks[0]), cffs_sindirect_data_t);
    xn_fixed_seq(&t, field(indirectBlocks[1]), cffs_dindirect_data_t);
    xn_fixed_seq(&t, field(indirectBlocks[2]), cffs_tindirect_data_t);
    xn_fixed_seq(&t, field(dirNum), XN_BYTES);
    xn_fixed_seq(&t, field(groupstart), XN_BYTES);
    xn_fixed_seq(&t, field(groupsize), XN_BYTES);
    xn_fixed_seq(&t, field(memory_sanity), XN_BYTES);
    cffs_discriminate_inode(&t.get_type, &t.type_access);
  } xn_end(&t, sizeof(TYPE));
#undef TYPE
  if ((cffs_inode_data_t = xn_import(&t)) < 0)
    return cffs_inode_data_t;


#define TYPE dinode_t
  xn_begin(&t, "CFFS inode (directory)", XN_NOT_STICKY); {
    xn_fixed_seq(&t, field(dinodeNum), XN_BYTES);
    xn_fixed_seq(&t, field(length), XN_BYTES);
    xn_fixed_seq(&t, field(type), XN_BYTES);
    xn_fixed_seq(&t, field(openCount), XN_BYTES);
    xn_fixed_seq(&t, field(uid), XN_BYTES);
    xn_fixed_seq(&t, field(gid), XN_BYTES);
    xn_fixed_seq(&t, field(linkCount), XN_BYTES);
    xn_fixed_seq(&t, field(mask), XN_BYTES);
    xn_fixed_seq(&t, field(accTime), XN_BYTES);
    xn_fixed_seq(&t, field(modTime), XN_BYTES);
    xn_fixed_seq(&t, field(creTime), XN_BYTES);
    xn_fixed_seq(&t, array(directBlocks, NUM_DIRECT), cffs_dir_t);
    xn_fixed_seq(&t, field(indirectBlocks[0]), cffs_sindirect_dir_t);
    xn_fixed_seq(&t, field(indirectBlocks[1]), cffs_dindirect_dir_t);
    xn_fixed_seq(&t, field(indirectBlocks[2]), cffs_tindirect_dir_t);
    xn_fixed_seq(&t, field(dirNum), XN_BYTES);
    xn_fixed_seq(&t, field(groupstart), XN_BYTES);
    xn_fixed_seq(&t, field(groupsize), XN_BYTES);
    xn_fixed_seq(&t, field(memory_sanity), XN_BYTES);
    cffs_discriminate_inode(&t.get_type, &t.type_access);
  } xn_end(&t, sizeof(TYPE));
#undef TYPE
  if ((cffs_inode_dir_t = xn_import(&t)) < 0)
    return cffs_inode_dir_t;

#define TYPE dinode_t
  xn_begin(&t, "CFFS inode", XN_NOT_STICKY); {
    t.class = UDF_UNION;
    i = 0;
    t.u.u.ty[i++] = cffs_inode_dir_t;
    t.u.u.ty[i++] = cffs_inode_data_t;
    t.u.u.ntypes = i;
    cffs_discriminate_inode(&t.get_type, &t.type_access);
  } xn_end(&t, 0);
  t.nbytes = sizeof(TYPE);
#undef TYPE
  if ((cffs_inode_t = xn_import(&t)) < 0)
    return cffs_inode_t;

  xn_begin(&t, "CFFS sector", XN_NOT_STICKY); {
    t.class = UDF_STRUCT;
    i = 0;
    t.u.s.ty[i++] = cffs_heading_t;
    t.u.s.ty[i++] = cffs_inode_t;
    t.u.s.ty[i++] = cffs_inode_t;
    t.u.s.ty[i++] = cffs_inode_t;
    t.u.s.ntypes = i;
  } xn_end(&t, 0);
  t.nbytes = 512;
  if ((cffs_sector_t = xn_import(&t)) < 0)
    return cffs_sector_t;

  xn_begin(&t, "CFFS directory block", XN_STICKY); {
    t.class = UDF_STRUCT;
    i = 0;
    t.u.s.ty[i++] = cffs_sector_t;
    t.u.s.ty[i++] = cffs_sector_t;
    t.u.s.ty[i++] = cffs_sector_t;
    t.u.s.ty[i++] = cffs_sector_t;
    t.u.s.ty[i++] = cffs_sector_t;
    t.u.s.ty[i++] = cffs_sector_t;
    t.u.s.ty[i++] = cffs_sector_t;
    t.u.s.ty[i++] = cffs_sector_t;
    t.u.s.ntypes = i;
  } xn_end(&t, 0);
  t.nbytes = BLOCK_SIZE;
  if ((cffs_dir_t = xn_import(&t)) < 0)
    return cffs_dir_t;

#define TYPE cffs_t
  xn_begin(&t, "CFFS super block fields", XN_STICKY); {
    xn_fixed_seq(&t, field(fsname), XN_BYTES);
    xn_fixed_seq(&t, field(fsdev), XN_BYTES);
    xn_fixed_seq(&t, field(rootDInodeNum), XN_BYTES);
    xn_fixed_seq(&t, field(size), XN_BYTES);
    xn_fixed_seq(&t, field(dirty), XN_BYTES);
    xn_fixed_seq(&t, field(numblocks), XN_BYTES);
    xn_fixed_seq(&t, field(numalloced), XN_BYTES);
    xn_fixed_seq(&t, field(allocMap), XN_BYTES);
    xn_fixed_seq(&t, field(space1), XN_BYTES);
  } xn_end(&t, offset(rootDinode));
  if ((cffs_super_fields_t = xn_import(&t)) < 0)
    return cffs_super_fields_t;

  xn_begin(&t, "CFFS super block", XN_STICKY); {
    t.class = UDF_STRUCT;
    i = 0;
    t.u.s.ty[i++] = cffs_super_fields_t;
    t.u.s.ty[i++] = cffs_inode_dir_t;
    t.u.s.ty[i++] = cffs_inode_dir_t;
    t.u.s.ntypes = i;
  } xn_end(&t, 0);
  t.nbytes = sizeof(TYPE);
#undef TYPE
  if ((cffs_super_t = xn_import(&t)) < 0)
    return cffs_super_t;

  return XN_SUCCESS;
}

struct udf_fun*
cffs_interpret_sector() {
  struct udf_fun* f;
  int ent, typ, len, ino;
  struct udf_inst *ip;

  NEW(f);
	
  if (!u_alloc_f(f, 1, UBB_SET, 1, 1)) {
    fatal(Cannot allocate function);
  }

  ip = f->insts;

  ent = u_getreg();
  len = u_getreg();
  typ = u_getreg();
  ino = u_getreg();

  u_movi(ip++, ent, 0);

  u_ldii(ip++, len, ent, offsetof(embdirent_t, entryLen));
  u_ldbi(ip++, typ, ent, offsetof(embdirent_t, type));
  
  u_beq(ip++, typ, 0, 3);	/* Is it valid? */
  u_ldii(ip++, ino, ent, offsetof(embdirent_t, inodeNum));
  u_divi(ip++, ino, ino, 32);
  u_add_cexti(ip++, ino, 1, cffs_inode_t);

  u_add(ip++, ent, ent, len);	/* Next entry */
  u_blti(ip++, ent, 128, -8);	/* Exmained all entries? */
  
  u_reti(ip++, 1);		/* Return success */

  f->ninst = ip - f->insts;

  return f;
}

struct udf_fun*
cffs_discriminate_inode(struct udf_fun* f, struct udf_ext* ext) {
  int typ, num;
  struct udf_inst *ip;

  if (!u_alloc_f(f, 1, UBB_SET, 1, 1)) {
    fatal(Cannot allocate function);
  }

  ip = f->insts;

  typ = u_getreg();
  num = u_getreg();

#define TYPE dinode_t

  u_ldsi(ip++, num, U_NO_REG, offset(dinodeNum));
  u_bnei(ip++, num, 0, 1);	/* Is it valid? */
  u_reti(ip++, XN_NIL);		/* No type */
  u_ldsi(ip++, typ, U_NO_REG, offset(type));
  u_bnei(ip++, typ, S_IFDIR, 1); /* Is it a directory? */
  u_reti(ip++, cffs_inode_dir_t);
  u_reti(ip++, cffs_inode_data_t);

  f->ninst = ip - f->insts;

  ext->n = 2;
  ext->v[0].lb = offset(dinodeNum);
  ext->v[0].ub = offset(dinodeNum) + size(dinodeNum);
  ext->v[1].lb = offset(type);
  ext->v[1].ub = offset(type) + size(type);
#undef TYPE

  return f;
}

#undef offset
#undef size
#undef field

static struct xn_op*
cffs_op(size_t offset, size_t slot,
	db_t from, db_t to, unsigned level, int dir) {
  static xn_op op;
  static struct xn_m_vec m;
  static db_t s_db;
  int type;
  s_db = to;

  assert(level <= 3);
  switch (level) {
  case 3: type = dir ? cffs_tindirect_dir_t : cffs_tindirect_data_t; break;
  case 2: type = dir ? cffs_dindirect_dir_t : cffs_dindirect_data_t; break;
  case 1: type = dir ? cffs_sindirect_dir_t : cffs_sindirect_data_t; break;
  case 0: type = dir ? cffs_dir_t : cffs_data_t; break;
  default: assert(0);  
  }

  op.u.db = from ? from : to;
  op.u.nelem = 1;
  op.u.type = type;

  op.m.cap = 0;
  op.m.own_id = slot;
  op.m.n = 1;
  op.m.mv = &m;
  m_write(&m, offset, &s_db, sizeof s_db);

  return &op;
}

/* This is for allocating into inodes */
struct xn_op*
cffs_inode_op(size_t slot, db_t from, db_t to, unsigned level, int dir) {
  return cffs_op(offsetof(dinode_t, directBlocks[slot]), slot,
		 from, to, level, dir);
}

/* This is for allocating into indirect blocks */
struct xn_op*
cffs_indirect_op(size_t slot, db_t from, db_t to, unsigned level, int dir) {
  return cffs_op(slot * sizeof(db_t), slot, from, to, level, dir);
}


void cffs_fill_xntypes (buffer_t *superblockBuf)
{
   cffs_superblock_setXNtype (superblockBuf, CFFS_XN_SUPER_T, cffs_super_t);
   cffs_superblock_setXNtype (superblockBuf, CFFS_XN_INODE_T, cffs_inode_t);
   cffs_superblock_setXNtype (superblockBuf, CFFS_XN_INODE_DATA_T, cffs_inode_data_t);
   cffs_superblock_setXNtype (superblockBuf, CFFS_XN_SINDIRECT_DATA_T, cffs_sindirect_data_t);
   cffs_superblock_setXNtype (superblockBuf, CFFS_XN_DINDIRECT_DATA_T, cffs_dindirect_data_t);
   cffs_superblock_setXNtype (superblockBuf, CFFS_XN_TINDIRECT_DATA_T, cffs_tindirect_data_t);
   cffs_superblock_setXNtype (superblockBuf, CFFS_XN_INODE_DIR_T, cffs_inode_dir_t);
   cffs_superblock_setXNtype (superblockBuf, CFFS_XN_SINDIRECT_DIR_T, cffs_sindirect_dir_t);
   cffs_superblock_setXNtype (superblockBuf, CFFS_XN_DINDIRECT_DIR_T, cffs_dindirect_dir_t);
   cffs_superblock_setXNtype (superblockBuf, CFFS_XN_TINDIRECT_DIR_T, cffs_tindirect_dir_t);
   cffs_superblock_setXNtype (superblockBuf, CFFS_XN_DIR_T, cffs_dir_t);
   cffs_superblock_setXNtype (superblockBuf, CFFS_XN_DATA_T, cffs_data_t);
}


void cffs_pull_xntypes (cffs_t *superblock)
{
   cffs_super_t = superblock->xntypes[CFFS_XN_SUPER_T];
   cffs_inode_t = superblock->xntypes[CFFS_XN_INODE_T];
   cffs_inode_data_t = superblock->xntypes[CFFS_XN_INODE_DATA_T];
   cffs_sindirect_data_t = superblock->xntypes[CFFS_XN_SINDIRECT_DATA_T];
   cffs_dindirect_data_t = superblock->xntypes[CFFS_XN_DINDIRECT_DATA_T];
   cffs_tindirect_data_t = superblock->xntypes[CFFS_XN_TINDIRECT_DATA_T];
   cffs_inode_dir_t = superblock->xntypes[CFFS_XN_INODE_DIR_T];
   cffs_sindirect_dir_t = superblock->xntypes[CFFS_XN_SINDIRECT_DIR_T];
   cffs_dindirect_dir_t = superblock->xntypes[CFFS_XN_DINDIRECT_DIR_T];
   cffs_tindirect_dir_t = superblock->xntypes[CFFS_XN_TINDIRECT_DIR_T];
   cffs_dir_t = superblock->xntypes[CFFS_XN_DIR_T];
   cffs_data_t = superblock->xntypes[CFFS_XN_DATA_T];
}

#endif /* XN */
