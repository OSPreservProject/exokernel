
#include <stdio.h>
#include <xok/sys_ucall.h>
#include <vos/vm-layout.h>
#include <vos/errno.h>
#include <vos/cap.h>
#include <vos/vm.h>
#include <vos/kprintf.h>
#include <vos/uinfo.h>


#define dprintf if (0) kprintf


uinfo_t *uinfo = (uinfo_t*) UINFO_REGION;

void 
uinfo_init(void)
{
  int r = sys_uinfo_get();
  Pte pte;
  
  if (r == 0)
  {
    kprintf("uinfo_map: cannot map uinfo!\n");
    exit(-1);
  }

  pte = (r << PGSHIFT) | PG_U | PG_P;

  dprintf("uinfo_map: mapping uinfo at %d\n",pte>>PGSHIFT);
  r = vm_self_insert_pte(CAP_USER, pte, UINFO_REGION, 0, 0);
  if (r < 0)
  {
    kprintf("uinfo_map: cannot map uinfo!\n");
    exit(-1);
  }
}


int
uinfo_remap()
{
  int r = sys_uinfo_get();
  Pte pte;
  
  if (r == 0)
  {
    kprintf("uinfo_map: cannot map uinfo!\n");
    exit(-1);
  }

  pte = (r << PGSHIFT) | PG_U | PG_P | PG_W;

  dprintf("uinfo_map: mapping writable uinfo at %d\n",pte>>PGSHIFT);
  r = vm_self_insert_pte(CAP_USER, pte, UINFO_REGION, 0, 0);
  if (r < 0)
  {
    kprintf("uinfo_map: cannot map writable uinfo!\n");
    RETERR(V_NOMEM);
  }
  return 0;
}

