#include <xuser.h>
#include <stdio.h>
#include <xok/pmap.h>
#include <xok/pxn.h>
#include <xok/bc.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <xok/kqueue.h>
#include <exos/vm-layout.h>
#include <xok/kerrno.h>
#include <xok/sysinfo.h>
#include <exos/ubc.h>
#include <exos/vm.h>
#include <sys/wait.h>
#include <assert.h>

/* these next two should really go into libc or bc.h */

struct Xn_name n;

struct bc_entry *bc_lookup (u32 d, u32 b) {
  struct bc_entry *buf;

  buf = KLIST_UPTR (&__bc.bc_qs[bc_hash (d, b)].lh_first, UBC);
  while (buf) {
    if (buf->buf_dev == d && buf->buf_blk == b) {
      return (buf);
    }
    buf = KLIST_UPTR (&buf->buf_link.le_next, UBC);
  }

  return (NULL);
}

void dump () {
  int i;
  struct bc_entry *b;

  for (i = 0; i < BC_NUM_QS; i++) {
    b =  KLIST_UPTR (&__bc.bc_qs[i].lh_first, UBC);
    while (b) {
      printf ("(%u, %u) -> %x", b->buf_dev, b->buf_blk, b->buf_pp);
      if (__ppages[b->buf_pp].pp_status == PP_FREE)
	printf (" (free)");
      printf ("\n");
      b = KLIST_UPTR (&b->buf_link.le_next, UBC);
    }
  }
}
 

void do_writeback (u32 block, u32 time, u32 eid) {
  int i;

  i = __syncd_request_writeback (eid, n.xa_dev, block, 1, time);
  if (i < 0) {
    printf ("error: syncd returned %d\n", i);
    exit (1);
  }
}

#define BLOCK 2
#define ADDR (TEMP_REGION+10*NBPG)

int main (int argc, char *argv[]) {
  int ret;

  u_int ppn;
  struct Xn_bptr bptr;
  u8 buf_state = BC_COMING_IN;
  struct Pp_state pp_state;
  struct bc_entry *b;
  int block = BLOCK;

  /* code to run through the buffer cache and print it out */
  if (argc > 1 && !strcmp (argv[1], "-p")) {
    dump ();
    return 0;
  }

  /* specify a specific block number to use */
  if (argc > 1 && !strcmp (argv[1], "-b")) {
    argc--; argv++;
    if (argc > 1) {
      block = atoi (argv[1]);
      argc--; argv++;
    }
  }

  /* ok, let's alloc a pxn and add an extent. Let the kernel assign
     a free device name to us to create a unique pxn name. */
  n.xa_dev = PXN_ANY_DEV;

  ret = sys_pxn_alloc (3, 3, 0, &n);
  if (ret < 0) {
    printf ("sys_pxn_alloc returned %d\n", ret);
    return 0;
  }

  printf ("assigned name {%u, %u, %u}\n", n.xa_dev, n.xa_offset, n.xa_size);

  /* add an extent to the pxn */
  bptr.bptr_type = BPTR_XTNT;
  bptr.bptr_xtnt.xtnt_block = 1;
  bptr.bptr_xtnt.xtnt_size = 2;
  ret = sys_pxn_add_bptr (&n, 0, &bptr, 0);
  if (ret < 0) {
    printf ("sys_pxn_add_bptr returned %d\n", ret);
    return 0;
  }

  /* now lets alloc a page and insert it into the buffer cache */

  if (sys_self_insert_pte (0, PG_W | PG_P | PG_U, ADDR) < 0) {
    printf ("sys_self_insert_pte failed\n");
    return 0;
  }
  ppn = vpt[ADDR >> PGSHIFT] >> PGSHIFT; 

  printf ("ref cnt on pg = %d\n", __ppages[ppn].pp_refcnt);

  pp_state.ps_readers = pp_state.ps_writers = PP_ACCESS_ALL;
  ret = sys_bc_insert (&n, 0, 0, block, 
		       buf_state,
		       0, ppn, &pp_state);
  if (ret < 0) {
    printf ("sys_bc_insert returned %d\n", ret);
    return 0;
  } 
  printf ("inserted ppn %x\n", ppn);

  b = bc_lookup (n.xa_dev, block);
  if (b == NULL || b->buf_dev != n.xa_dev || b->buf_blk != block) {
    printf ("error--not found\n");
    if (b)
      printf ("got b->buf_dev = %d b->buf_blk = %d\n", b->buf_dev, b->buf_blk);
  } else {
    printf ("ok, found it!\n");
  }

  printf ("ref cnt on pg = %d\n", __ppages[b->buf_pp].pp_refcnt);

  if (sys_self_insert_pte (0, 0, ADDR) < 0) {
    printf ("sys_self_insert_pte didn't unmap\n");
    return 0;
  }

  printf ("unmapped buffer...page should be free now\n");
  dump ();

  printf ("about to probe for entry...\n");
  b = bc_lookup (n.xa_dev, block);
  if (b == NULL || b->buf_dev != n.xa_dev || b->buf_blk != block) {
    printf ("error--not found\n");
    if (b)
      printf ("got b->buf_dev = %d b->buf_blk = %d\n", b->buf_dev, b->buf_blk);
  } else {
    printf ("ok, found it!\n");
  }

  if (sys_self_insert_pte (0, b->buf_pp << PGSHIFT | PG_SHARED | PG_P | PG_U | PG_W, ADDR) < 0) {
    printf ("sys_self_insert_pte failed to map free buffer page\n");
    return 0;
  }

  printf ("page mapped\n");
  dump ();
  printf ("ref cnt on pg = %d\n", __ppages[b->buf_pp].pp_refcnt); 
  if (ppn != b->buf_pp)
    printf ("uhm...oddd...ppn != b->buf_pp...hmmmm\n");

  if (argc > 1 && !strcmp (argv[1], "-d")) {
    argc--; argv++;
    do {
      *(volatile int *)ADDR = 3; /* make buffer dirty */
    } while (0);
    return 0;
  }

  if (argc > 1 && !strcmp (argv[1], "-f")) {
    u32 dev = n.xa_dev;
    if (argc > 2)
      dev = strtoul (argv[2], NULL, 10);
    if (dev == 0)
      dev = BC_SYNC_ANY;
    if (argc > 3)
      block = strtoul (argv[3], NULL, 10);
    if (block == 0)
      block = BC_SYNC_ANY;
    printf ("argc = %d dev = %u block = %u\n", argc, (unsigned )dev, block);
    sys_bc_flush (dev, block, 1);
    return 0;
  }

#if 1
#define NUM_CHILDREN 50
  /* stress the syncer */
  if (argc > 1 && !strcmp (argv[1], "-s")) {
    int i;
    int pid;
    int dead;

    for (i = 0; i < NUM_CHILDREN; i++) {
      if ((pid = fork ()) == -1) {
	printf ("error forking!\n");
	return 0;
      }
      if (pid == 0) 
	break;
    }

    if (pid == 0) {
      assert (vpt[ADDR >> PGSHIFT] & PG_SHARED);
      /* printf ("child sending to env %d\n", env);	 */
      for (i = 0; i < 20; i++) {
	do {
	  *(volatile int *)ADDR = 3; /* make buffer dirty */
	} while (0);
	if (vpt[ADDR > PGSHIFT] & PG_D)
	  sys_cputs (".");
	do_writeback (block, 0, __syncd_lookup_env ());
      }
      sleep (3);
    } else {
      printf ("done forking\n");
      for (dead = 0; dead < NUM_CHILDREN; dead++) {
	wait (NULL);
      }
      printf ("all children done\n");
    }
    return 0;
  }
#endif

  /* queue a write back */
  if (argc > 1 && !strcmp (argv[1], "-w")) {
    printf ("should queue buf for syncing 2x, mark it dirty, sycn it once");
    printf ("and then not sync it the second time since it should now be clean\n");
    do_writeback (block, 3, __syncd_lookup_env ());
    do {
      *(volatile int *)ADDR = 3; /* make buffer dirty */
    } while (0);
    do_writeback (block, 6, __syncd_lookup_env ());
    sleep (10);
    return 0;
  }

  /* force the kernel to take pages from the buffer cache by allocating
     all free memory. */

  if (argc > 1 && !strcmp (argv[1], "-a")) {
    int ret;
    u_int addr = ADDR;

    while ((ret = sys_self_insert_pte (0, PG_P|PG_W|PG_U, addr+=NBPG)) >= 0);
    if (ret != -E_NO_RESOURCE) {
      printf ("bombed out of allocating ton-o-pages for unexpected reason %d\n", ret);
      return 0;
    }
    /* free up a page so we have a little breathing room */
    sys_self_insert_pte (0, 0, addr-NBPG);
    printf ("dumping...bc better only have the one guy we mapped above!\n");
    dump ();
  }

  if (__sysinfo.si_ndisks == 0) {
     printf ("No disks -- skipping this test phase\n");
  } else {

     /* now lets alloc a page and insert it into the buffer cache */
   
     if (sys_self_insert_pte (0, PG_W | PG_P | PG_U, ADDR) < 0) {
       printf ("sys_self_insert_pte failed\n");
       return 0;
     }
     ppn = vpt[ADDR >> PGSHIFT] >> PGSHIFT; 
     printf ("ref cnt on pg = %d\n", __ppages[ppn].pp_refcnt);

     bzero ((char *)ADDR, 4096);

     pp_state.ps_readers = pp_state.ps_writers = PP_ACCESS_ALL;
     ret = sys_bc_insert (&__sysinfo.si_pxn[0], 0, 0, 0, (u8) BC_VALID, 0, ppn,
			  &pp_state);
     printf ("Insertion for disk 0 block 0 returned %d\n", ret);
     dump();
  }

  printf ("done\n");
  return 0;
}
