
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

/*
 * CFFS meta data specifications. 
 *
 */
#include "cffs.h"
#include "cffs_xntypes.h"
#include <memory.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include <ubb/xn.h>
#include <ubb/lib/ubb-lib.h>
#include <ubb/xn-struct.h>
#include <ubb/root-catalogue.h>
#include <ubb/kernel/virtual-disk.h>
#include <ubb/kernel/kernel.h>
#include <ubb/lib/sync-disk.h>

#ifdef JJ
#include <ubb/lib/demand.h>
#else
#include <exos/uwk.h>
#include <exos/cap.h>
#include <assert.h>
#define fatal(a)	demand(0,a)
#endif

#include "spec.h"

#ifdef XN


void
chkpt(char* msg) {
#if 0
  kprintf(msg);
  kprintf("\n");
  usleep(50000);
#endif
}


/* Create a write (memcpy(inode + offset, data, nbytes) */
void
m_write(struct xn_m_vec *m, size_t offset, void *data, int nbytes)
{
  m->offset = offset;
  m->nbytes = nbytes;
  m->addr = data;
}

db_t
install_root(char *name, cap_t c, xn_elem_t t) {
	int res;
	db_t root;
	struct root_entry r;

	switch((res = sys_xn_mount(&r, name, c))) {
	case XN_SUCCESS:
		assert(r.t == t);
		assert(r.nelem == 1);
		return r.db;
	case XN_NOT_FOUND:
		break;
	default:
		printf("res = %d\n", res);
		fatal(Some error);
	}

	do { 
		/* GROK - don't do syscall after mapping freemap */
		root = sys_db_find_free(random() % XN_NBLOCKS, 1);
		{ /* XXX - HACK! fix XN to use trup and propagating faults */
		  char n[80];

		  bzero(n, 80);
		  strcpy(n, name);
		  res = sys_install_mount(n, &root, 1, t, c);
		}
	} while(res == XN_CANNOT_ALLOC);
	
	switch(res) {
	case XN_SUCCESS:
		break;
	case XN_DUPLICATE:
		fatal(Should not happen (race condition));
	default:
		printf("root = %ld, %d\n", root, res);
		fatal(root failed!);
	}
	return root;
}

int xn_import(struct udf_type *t) {                        
        struct root_entry r;                        
        int res;                                    
        size_t nblocks, nbytes;
        cap_t c;                                   
        long db;
        int i, tid;
        xn_cnt_t n;                               
        da_t da;                                    
        void *p;

        if((tid = sys__type_mount(t->type_name)) >= 0) {
                //printf("hit in cache for %s (tid %d, class %d)\n", t->type_name, tid, t->class);
                return tid;
        } else         
                //printf("<%s> did not hit in cache\n", t->type_name);

        c = (cap_t)CAP_ROOT;

        /* Allocate space, place in root, import. */
        if(sys__xn_mount(&r, t->type_name, c) == XN_SUCCESS) {
                xn_cnt_t cnt;
                ppn_t ppn;

                cnt = r.nelem;
                printf("reading %d\n", r.nelem);
                /* alloc storage. */
                for(ppn = -1, i = 0; i < r.nelem; i++, ppn++) {
                        ppn = sys__xn_bind(r.db+i, ppn, (cap_t)CAP_ROOT, XN_FORCE_READIN, 1); 
                        if(ppn < 0) {               
                                printf("ppn = %ld\n", ppn);
                                fatal(Death);                                   
                        }
                }  
                /* now need to load. */           
                if((res = sys__xn_readin(r.db, r.nelem, &cnt)) != XN_SUCCESS) {
                        printf("res = %d\n", res);
                        fatal(Death);
                }
		/* GROK -- not my code, but I think cnt should have been 1 */
		/* going into the readin (rather than nelem), since each   */
		/* disk request gets notification (rather than each block  */
		/* read or written).                                       */
		wk_waitfor_value (&cnt, 0, 0);
                printf("hit in cache for %s\n", t->type_name);
                if((res = sys__type_mount(t->type_name)) < XN_SUCCESS) {
                        printf("res = %d\n", res);
                        fatal(SHould have hit);
                }
                return res;
        }

        chkpt("xn_mount");

        nblocks = bytes_to_blocks(sizeof *t);
        db = 0;
        if(sys__install_mount(t->type_name, &db, nblocks, XN_DB, c) < 0)
                fatal(Mount failed);
                
        //printf("nblocks = %ld\n", (long)nblocks); 
        /* Allocate pages, and copyin. */           
        for(i = 0; i < nblocks; i++) {                     
                if(sys__xn_bind(db+i, -1, c, XN_ZERO_FILL, 1) < 0)              
                        fatal(Could not bind);
                da = da_compose(db+i, 0);
                p = (char *)t + i * PAGESIZ;      
                nbytes = (i == (nblocks - 1)) ?                                
                                (sizeof *t % XN_BLOCK_SIZE) : PAGESIZ;

                if((res = sys__xn_writeb(da, p, nbytes, c)) < 0) {
                        printf("res = %d\n", res);
                        fatal(Could not write);
                }
                n = 1;
                if(sys__xn_writeback(db+i, 1, &n) < 0)
                        fatal(Cannot write back);
		wk_waitfor_value (&n, 0, 0);
        }
        chkpt("type import..");
        if((tid = sys__type_import(t->type_name)) < 0)
                fatal(Cannot import);
        return tid;
}


db_t gross_get_superblock_db (char *name)
{
   struct root_catalogue rc;
   int err;
   int i;

   err = sys_xn_read_catalogue (&rc);
   assert (err == 0);

   for (i=0; i<R_NSLOTS; i++) {
      if (!strcmp(rc.roots[i].name, name)) {
         return (rc.roots[i].db);
      }
   }

   return (0);
}

#endif /* XN */
