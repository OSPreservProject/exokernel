
#include <stdio.h>
#include <xok/sys_ucall.h>
#include <exos/vm.h>
#include <exos/vm-layout.h>

#define EXOS_COMPAT_MODE
#include <vos/uinfo.h>

uinfo_t *uinfo = (uinfo_t*) FORK_TEMP_PG;

static int 
map_vos_uinfo()
{
  int r = sys_uinfo_get();
  Pte pte;
  
  if (r == 0)
  {
    printf("exos fsrv: cannot map vos uinfo!\n");
    return -1;
  }

  pte = (r << PGSHIFT) | PG_U | PG_P | PG_W;

  r = _exos_self_insert_pte(0, pte, FORK_TEMP_PG, 0, 0);
  if (r < 0)
  {
    printf("exos fsrv: cannot map vos uinfo!\n");
    return -1;
  }
  return 0;
}

static int
unmap_vos_uinfo()
{
  _exos_self_unmap_page(0, FORK_TEMP_PG);
  return 0;
}


int
write_uinfo()
{
  if (map_vos_uinfo() < 0)
    return -1;
  uinfo->fsrv_envid = sys_geteid();
  printf("exos fsrv: save envid in uinfo.\n");
  unmap_vos_uinfo();
  return 0;
}


