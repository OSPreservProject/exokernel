#include <stdio.h>
#include <xok/sys_ucall.h>
#include <xok/pmap.h>
#include <exos/mallocs.h>
#include <assert.h>
#include <xok/kerrno.h>
#include <xok/sysinfo.h>

#define PTEBITS (PG_P|PG_U|PG_W)

int main () {
  struct Ppage *pp;
  int ret;
  u_int temp_page;

  printf ("hi\n");
  temp_page = (u_int)__malloc(NBPG);
  assert(temp_page != 0);

  printf ("making sure exposed kernel data structures are on PP_KERNRO pages...\n");
  pp = &__ppages[vpt[UBC >> PGSHIFT] >> PGSHIFT];
  assert (pp->pp_status == PP_KERNRO);
  pp = &__ppages[vpt[SYSINFO >> PGSHIFT] >> PGSHIFT];
  assert (pp->pp_status == PP_KERNRO);
  
  assert (sys_self_insert_pte (0, PTEBITS, temp_page) >= 0);
  
  pp = &__ppages[vpt[temp_page >> PGSHIFT] >> PGSHIFT];

  /* one writer one reader now. should fail to move to 1 reader 0 writers */

  ret = sys_pstate_mod (0, vpt[temp_page >> PGSHIFT] >> PGSHIFT, 
			  PP_ACCESS_ALL, PP_ACCESS_NONE);
  assert (ret == -E_SHARE);

  assert (sys_self_mod_pte_range (0, 0, PG_W, temp_page, 1) >= 0);

  assert (sys_pstate_mod (0, vpt[temp_page >> PGSHIFT] >> PGSHIFT, 
			  PP_ACCESS_ALL, PP_ACCESS_NONE) >= 0);

  __free((void*)temp_page);
  printf ("all ok\n");

  return 42;
}
