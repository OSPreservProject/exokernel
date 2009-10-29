/*
 * Copyright (C) 1998 Exotec, Inc.
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
 * associated documentation will at all times remain with Exotec, Inc..
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by Exotec, Inc. The rest
 * of this file is covered by the copyright notices, if any, listed below.
 */

#define __DISK_MODULE__

#include <xok/bc.h>
#include <xok/disk.h>
#include <xok/kerrno.h>
#include <xok/micropart.h>
#include <xok/printf.h>
#include <xok/pxn.h>
#include <xok/sys_proto.h>
#include <xok_include/assert.h>
#include <xok/pmap.h>
#include <xok/sysinfo.h>


/* Crude disk multiplexing. A partition is divided into fixed-sized
   micro-partitions. The kernel associates each micro-partition with a
   capability that must be presented to establish a pxn for that
   partition. Then anyone with that capability may read/write the
   micro- partition.

   The only important kernel data structure is the freemap that tracks
   who owns each micro-partition (this is where the capability
   described above is actually stored). The freemap for each partition
   is allocated in a page that is in the buffer cache and which has
   backing store associated with it. The first micro-partition of each
   partition is reserved for use by the kernel. Currently, the first
   block of this reserved partition is used to store the freemap.

   Since the freemap is in the bc, any process may map it (read-only)
   and initiate writes of it back to disk. Currently, a special kernel
   syscall in this file must be used in order to initially read the
   freemap in or to initialize a partition in the first place.  */

/* XXX -- need a "free part" routine to free the entire partition and
   relinquish our control of it. */

/* XXX -- should just add bptr's to a pxn rather than keep creating
   new ones. Maybe. */

/* XXX -- need a signature on the part so we know it's been inited so
   that load knows it can trust the data */

/* XXX -- need to unpin pages when we're done with a part */

/* Access control and freemap for micropartitions
 *
 * Each entry is either an invalid capability, meaning the
 * micropartition is free for the taking, or a valid capability which
 * governs the pxn required to access the partition.
 */

struct Micropart micropart[MAX_DISKS];

static u64
micropart_part_off (u_int part, u_int upart)
{
  assert (part < SYSINFO_GET(si_ndisks));
  assert (upart < micropart[part].mp_header->h_num_microparts);

  return (micropart[part].mp_header->h_micropart_sz * upart);
}

static u32
micropart_num_parts (u_int part)
{
  assert (part < SYSINFO_GET(si_ndisks));

  return (micropart[part].mp_header->h_num_microparts);
}

static int
micropart_freemap_io_pending (u_int part)
{
  struct bc_entry *bc;

  bc = bc_lookup (part, MICROPART_METADATA_PAGE);
  if (!bc) return 0;

  /* we pin the page once when we load the part. Any i/o
     in progress will result in an additional pinning
     of the page. */

  if (Ppage_pp_pinned_get(ppages_get(bc->buf_ppn)) > 1) return 1;

  return 0;
}

/* initialize the micropart subsystem at boot up */
void
micropart_init ()
{
  u_int i;

  for (i = 0; i < MAX_DISKS; i++) {
    micropart[i].mp_loaded = 0;
  }
}

/* Intialize a partition into micropartitions (all unallocated). part
   identifies which partition to use and k dominates the capability
   for this disk. Does not really take affect until the freemap is
   written to disk. */

/* XXX -- remove any pxn's for prior micro-parts if they existed */

int
sys_micropart_init (u_int sn, u_int part, u_int k)
{
  u_int i;
  struct bc_entry *bc;
  cap c_pxn;
  struct Pxn *pxn;
  int r;

  /* make sure part is valid */
  if (part >= SYSINFO_GET(si_ndisks))
    return -E_INVAL;

  /* first verify the user has permission to init this partition */
  pxn = lookup_pxn (SYSINFO_PTR_AT(si_pxn,part));
  
  if (!pxn)
    return -E_NOT_FOUND;
  
  if ((r = env_getcap (curenv, k, &c_pxn)) < 0 ||
      !pxn_verify_access (pxn, &c_pxn, ACL_W, &r))
    return r;

  /* do we already have storage for the freemap? */
  if (micropart[part].mp_loaded) {
    assert (micropart[part].mp_header);

    /* don't touch the freemap while there's pending i/o */
    if (micropart_freemap_io_pending (part))
      return -E_INVAL;

  } else {
    bc = ppage_get_reclaimable_buffer (part, MICROPART_METADATA_PAGE, BC_VALID,
				       &r, 0);
    if (!bc) return r;
    micropart[part].mp_bc = bc;

    /* make sure this page stays here */
    ppage_pin (ppages_get(bc->buf_ppn));

    /* setup a pointer to this page */
    micropart[part].mp_header = 
      (struct Micropart_header *)pp2va (ppages_get(bc->buf_ppn));

    /* fill in the number of micro-partitions */
#define MICRO_SZ_BYTES (64*1024*1024)
    micropart[part].mp_header->h_micropart_sz = 
      MICRO_SZ_BYTES / disk_sector_sz (part);
    micropart[part].mp_header->h_num_microparts = 
      disk_size(part) / micropart[part].mp_header->h_micropart_sz;

    micropart[part].mp_header->h_freemap = 
      (struct FSID *)((char *)micropart[part].mp_header + 
		      sizeof (struct Micropart_header));

    /* XXX - make sure the micropart header all fits into a single page */
    assert ((sizeof (struct Micropart_header) + 
	     micropart_num_parts (part) * 
	     sizeof (*micropart[part].mp_header->h_freemap)) <= NBPG);

    /* set the signature */
    micropart[part].mp_header->h_signature = MICROPART_SIG;

    /* ok, this one is ready to go */
    micropart[part].mp_loaded = 1;
  }

  /* XXX - make sure a single page is big enough to hold the freemap */
  assert ((sizeof (*micropart[part].mp_header) + 
	   (sizeof (*micropart[part].mp_header->h_freemap) * 
	    micropart[part].mp_header->h_num_microparts)) <= NBPG);

  /* initialize the freemap to zero */
  for (i = 0; i < micropart_num_parts(part); i++) {
    micropart[part].mp_header->h_freemap[i].fs_cap.c_valid = 0;
  }

  return 0;
}

/* free a partition (stop using it for microparts) */
int
sys_micropart_free (u_int sn, u_int part, u_int k)
{
  cap c;
  struct Pxn *pxn;
  struct Xn_name xn;
  int ret;
  u_int i;

  /* sanity check */
  if (part >= SYSINFO_GET(si_ndisks))
    return -E_INVAL;

  pxn = lookup_pxn (SYSINFO_PTR_AT(si_pxn,part));
  if (!pxn)
    return -E_NOT_FOUND;

  if ((ret = env_getcap (curenv, k, &c)) < 0) return ret;
  if (!pxn_verify_access (pxn, &c, ACL_W, &ret)) return ret;

  if (!micropart[part].mp_loaded) return 0;

  for (i = 0; i < micropart_num_parts(part); i++) {
    if (micropart[part].mp_header->h_freemap->fs_cap.c_valid) {
      xn.xa_dev = part;
      xn.xa_name = micropart[part].mp_header->h_freemap->fs_id;
      pxn = lookup_pxn (&xn);
      if (pxn)
	pxn_dealloc (pxn);
    }
  }

  ppage_unpin (ppages_get(micropart[part].mp_bc->buf_ppn));

  micropart[part].mp_loaded = 0;

  return 0;
}

/* load a partition in. requires block (part, MICROPART_METADATA_PAGE) to
   already be in the bc. */
int
sys_micropart_load (u_int sn, u_int part, u_int k)
{
  int ret;
  struct Pxn *pxn;
  cap c_pxn;
  struct bc_entry *bc;

  /* sanity check */
  if (part >= SYSINFO_GET(si_ndisks))
    return -E_INVAL;
  if (micropart[part].mp_loaded)
    return -E_INVAL;

  /* just to be paranoid, only let privledged users load a partition */
  pxn = lookup_pxn (SYSINFO_PTR_AT(si_pxn,part));
  if (!pxn)
    return -E_NOT_FOUND;
  if ((ret = env_getcap (curenv, k, &c_pxn)) < 0) return ret;
  if (!pxn_verify_access (pxn, &c_pxn, ACL_W, &ret)) return ret;

  /* lookup the header */
  bc = bc_lookup (part, MICROPART_METADATA_PAGE);
  assert (bc); /* XXX - return something instead */

  /* pin the page so that it's always in the buffer cache */
  ppage_pin (ppages_get(bc->buf_ppn));

  /* point the freemap to this page */
  micropart[part].mp_header = 
    (struct Micropart_header *)pp2va (ppages_get(bc->buf_ppn));

  micropart[part].mp_header->h_freemap = 
    (struct FSID *)((char *)micropart[part].mp_header + 
		    sizeof (struct Micropart_header));

  /* make sure there's a valid signature */
  if (micropart[part].mp_header->h_signature != MICROPART_SIG) return -E_INVAL;

  /* sanity check partition size info */
  if ((micropart_num_parts (part) * 
       micropart[part].mp_header->h_micropart_sz) > disk_size (part)) {
    warn ("micropart_load: bogus micropartition sizes and counts. Not "
	  "loading.");
    return -E_INVAL;
  }

  /* and finally, fill in the rest of the micropart structure for this part */
  micropart[part].mp_bc = bc;
  micropart[part].mp_loaded = 1;

  return 0;
}

int
sys_micropart_bootfs (u_int sn, u_int part, u_int k, u_int fsid)
{
  cap c;
  struct Xn_name xn;
  struct Xn_xtnt xtnt;
  int ret;
  u_int i;

  /* sanity check */
  if (part >= SYSINFO_GET(si_ndisks))
    return -E_INVAL;
  if (!micropart[part].mp_loaded)
    return -E_INVAL;

  /* don't touch the freemap while there's pending i/o */
  if (micropart_freemap_io_pending (part))
    return -E_INVAL;

  /* lookup the capability we're tagging this micropartition with */
  if ((ret = env_getcap (curenv, k, &c)) < 0)
    return ret;
  assert (c.c_valid); /* XXX - return something */

  xn.xa_dev = part;
  xn.xa_name = fsid;

  ret = pxn_alloc (3, 3, micropart[part].mp_header->h_num_microparts, &c, &c,
		   &xn);
  if (ret < 0) return ret;

  for (i = 0; i < micropart[part].mp_header->h_num_microparts; i++) {
    if (!micropart[part].mp_header->h_freemap[i].fs_cap.c_valid) continue;

    if (micropart[part].mp_header->h_freemap[i].fs_id == fsid) {
      if (!cap_dominates (&c, 
			  &micropart[part].mp_header->h_freemap[i].fs_cap)) {
	/* XXX - cleanup - dealloc pxn */
	return -E_CAP_INSUFF;
      }

      xtnt.xtnt_block = micropart_part_off (part, i);
      xtnt.xtnt_size = micropart[part].mp_header->h_micropart_sz;

      ret = pxn_kernel_add_xtnt (&xn, &xtnt);
      if (ret < 0) {
	/* XXX - cleanup - dealloc pxn */
	return ret;
      }
    }
  }

  return 0;
}

/* Allocate a micro-partition. Currently, anyone can do this. This
   allocation does not really take affect until the freemap is written
   out to disk. Until that point, if we crash, the allocation will not
   have appeared to have happened.

   Must already have a pxn named (part, fsid) that will receive
   a new extent for this micropartition. */

int
sys_micropart_alloc (u_int sn, u_int part, u_int upart, u_int k, u_int fsid)
{
  cap c;
  struct Xn_name xn;
  struct Xn_xtnt xtnt;
  int ret;
  
  /* sanity check */
  if (part >= SYSINFO_GET(si_ndisks))
    return -E_INVAL;

  if (!micropart[part].mp_loaded)
    return -E_INVAL;

  if (upart >= micropart_num_parts (part))
    return -E_INVAL;

  /* don't touch the freemap while there's pending i/o */
  if (micropart_freemap_io_pending (part))
    return -E_INVAL;

  /* not already allocated */
  if (micropart[part].mp_header->h_freemap[upart].fs_cap.c_valid)
    return -E_EXISTS;

  /* lookup the capability we're tagging this micropartition with */
  if ((ret = env_getcap (curenv, k, &c)) < 0)
    return ret;
  assert (c.c_valid); /* XXX - return something */

  /* mark the micropart as allocated by installing the capability */
  micropart[part].mp_header->h_freemap[upart].fs_cap = c;
  micropart[part].mp_header->h_freemap[upart].fs_id = fsid;

  /* note that the freemap needs to be written out */
  bc_set_dirty (micropart[part].mp_bc);

  /* add an extent for this micropartition to the user's pxn */

  xn.xa_dev = part;
  xn.xa_name = fsid;

  xtnt.xtnt_block = micropart_part_off (part, upart);
  xtnt.xtnt_size = micropart[part].mp_header->h_micropart_sz;

  ret = pxn_kernel_add_xtnt (&xn, &xtnt);
  if (ret < 0) {
    /* XXX - cleanup */
    return ret;
  }

  return 0;
}

/* deallocate a micropartition and remove the associated pxn */
int
sys_micropart_dealloc (u_int sn, u_int part, u_int upart, u_int k, u_int fsid)
{
  cap c;
  struct Xn_name xn;
  struct Xn_xtnt xtnt;
  int ret;
  struct Pxn *p;

  /* sanity check */
  if (part >= SYSINFO_GET(si_ndisks))
    return -E_INVAL;
  if (!micropart[part].mp_loaded)
    return -E_INVAL;
  if (upart >= micropart_num_parts (part))
    return -E_INVAL;

  /* don't touch the freemap while there's pending i/o */
  if (micropart_freemap_io_pending (part))
    return -E_INVAL;

  /* must be already allocated */
  if (!micropart[part].mp_header->h_freemap[upart].fs_cap.c_valid)
    return -E_EXISTS;

  /* lookup the capability associated with this micropart */
  if ((ret = env_getcap (curenv, k, &c)) < 0)
    return ret;
  
  /* check to see if it dominates the capability that owns this
     micro-partition */
  if (!cap_dominates (&c, &micropart[part].mp_header->h_freemap[upart].fs_cap))
    return -E_CAP_INSUFF;
  if (micropart[part].mp_header->h_freemap[upart].fs_id != fsid)
    return -E_INVAL;

  /* now we need to remove the associated pxn */
  
  /* build a pxn name for this micropart */
  xn.xa_dev = part;
  xn.xa_name = fsid;

  xtnt.xtnt_block = micropart_part_off (part, upart);
  xtnt.xtnt_size = micropart[part].mp_header->h_micropart_sz;

  p = lookup_pxn (&xn);
  if (!p) return -E_NOT_FOUND;

  pxn_remove_xtnt (p, &xtnt);
    
  /* ok, free the micropart */
  micropart[part].mp_header->h_freemap[upart].fs_cap.c_valid = 0;
  bc_set_dirty (micropart[part].mp_bc);

  return 0;
}


#ifdef __ENCAP__
#include <xok/pmapP.h>
#include <xok/sysinfoP.h>
#endif

