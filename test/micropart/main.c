#include <xok/sys_ucall.h>
#include <xok/pxn.h>
#include <exos/cap.h>
#include <stdio.h>
#include <xok/micropart.h>
#include <assert.h>
#include <exos/ubc.h>
#include <exos/vm.h>

#define PART 1
#define MICRO_PART 1
#define FSID 17
#define VA 0x87000000

#define micropart_part_off(x) ((32*1024*1024*x)/NBPG)

void main () {
  int ret;
  struct Xn_name xn;
  struct bc_entry *b;

  printf ("Hello\n");

  xn.xa_dev = PART;
  xn.xa_name = FSID;

  sys_bc_flush (PART, micropart_part_off (MICRO_PART), 1);

  ret = sys_micropart_init (PART, CAP_ROOT);
  printf ("sys_micropart_init returned %d\n", ret);

  ret = sys_micropart_bootfs (PART, CAP_ROOT, FSID);
  printf ("sys_micropart_bootfs returned %d\n", ret);

  ret = _exos_bc_read_and_insert (PART, micropart_part_off (MICRO_PART), 1,
				  NULL);
  printf ("_exos_bc_read_and_insert returned %d\n", ret);

  b = __bc_lookup (PART, micropart_part_off (MICRO_PART));
  assert (b);

  ret = sys_self_bc_buffer_map (&xn, CAP_ROOT, (b->buf_ppn << PGSHIFT) | PG_U|PG_P,
				VA);
  printf ("sys_self_bc_buffer_map returned %d\n", ret);
  assert (sys_self_insert_pte (CAP_ROOT, 0, VA) == 0);

  ret = sys_micropart_alloc (PART, MICRO_PART, CAP_ROOT, FSID);
  printf ("sys_micropart_alloc returned %d\n", ret);

  ret = sys_self_bc_buffer_map (&xn, CAP_ROOT, (b->buf_ppn << PGSHIFT) | PG_U|PG_P,
				VA);
  printf ("sys_self_bc_buffer_map returned %d\n", ret);
  assert (sys_self_insert_pte (CAP_ROOT, 0, VA) == 0);

  ret = sys_micropart_dealloc (PART, MICRO_PART, CAP_ROOT, FSID);
  printf ("sys_micropart_dealloc returned %d\n", ret);

  ret = sys_self_bc_buffer_map (&xn, CAP_ROOT, (b->buf_ppn << PGSHIFT) | PG_U|PG_P,
				VA);
  printf ("sys_self_bc_buffer_map returned %d\n", ret);
  assert (sys_self_insert_pte (CAP_ROOT, 0, VA) == 0);

  ret = sys_micropart_alloc (PART, MICRO_PART, CAP_ROOT, FSID);
  printf ("sys_micropart_alloc returned %d\n", ret);

  ret = sys_self_bc_buffer_map (&xn, CAP_ROOT, (b->buf_ppn << PGSHIFT) | PG_U|PG_P,
				VA);
  printf ("sys_self_bc_buffer_map returned %d\n", ret);
  assert (sys_self_insert_pte (CAP_ROOT, 0, VA) == 0);
}
