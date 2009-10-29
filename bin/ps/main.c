#include <exos/conf.h>
#include <exos/process.h>
#include <exos/cap.h>
#include <xok/env.h>
#include <exos/vm-layout.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <exos/fpu.h>
#include <exos/mallocs.h>

#include <exos/conf.h>

#ifdef PROCESS_TABLE
static u_int read_write_memory (int request, pid_t pid, u_int addr, int data) {
  u_int write;
  u_int pte;
  u_int env;
  u_int offset;
  u_int va = (u_int )addr;
  u_int temp_page;

#define PT_WRITE_D 1
#define PT_WRITE_I 1
#define PT_READ_D 0
#define PT_READ_I 0

  if (request == PT_WRITE_D || request == PT_WRITE_I) {
    write = 1;
  } else {
    write = 0;
  }

  env = ProcInfo->pidMap[pid];
  assert (env);
  if (!(pte = sys_read_pte (va, 0, env))) {
    printf ("ptrace: couldn't read pte (va %x, env %x)\n", va, env);
    return (0);
  }
  temp_page = (u_int)__malloc(NBPG);
  if (temp_page == 0 ||
      _exos_self_insert_pte (0, write ? pte | PG_W : pte, temp_page,
			     ESIP_DONTPAGE) < 0) {
    printf ("ptrace: couldn't map page\n");
    __free((void*)temp_page);
    return (0);
  }
  offset = va & PGMASK;
  va = temp_page+offset;

  if (write) {
    *(int *)va = data;
  } else {
#if 0
printf ("readin eip from %x\n", va);
#endif
    data = *(int *)va;
  }

  if (_exos_self_unmap_page (0, temp_page) < 0) {
    printf ("ptrace: couldn't unmap page\n");
    __free((void*)temp_page);
    assert (0);
  }

  __free((void*)temp_page);
  return (data);
}

unsigned int get_eip (pid_t pid) {
  unsigned int esp;
  unsigned int eip = 0;
#if 0
  unsigned int tmp;
  int i;
#endif

  struct Env *e = (struct Env *)&__envs[envidx (ProcInfo->pidMap[pid])];
  esp = e->env_tf.tf_esp + 36;

  /* check if pid is saving its fpu state */ 
  { 
    struct Uenv cu;
    int ret;
    if ((ret = sys_rdu(CAP_ROOT,e->env_id,&cu)) >= 0) {
      if (cu.u_fpu) esp += FPU_SAVE_SZ; 
    }
    else
      printf("sys_rdu returned %d\n",ret);
  }

#if 0
  for (i = 0; i < 10; i++) {
    tmp = read_write_memory (PT_READ_D, pid, esp+(i-5)*4, 0);
    printf ("eip %d = %x\n", i, tmp);
    if (i == 5)
      eip = tmp;
  }
#endif
  eip = read_write_memory (PT_READ_D, pid, esp, 0);

  return (eip);
}
#endif /* PROCESS_TABLE */

void process_table_ps(void) {
#ifdef PROCESS_TABLE
  int i;
  struct Uenv cu;
  char *state = "unknown";
  int mypid = getpid ();

  printf ("%3s %4s %4s %11s %-25s %5s  %9s %9s\n","PID","PPID", "REFS", "ENV", "NAME","STATE", "CPUTICKS", "EIP");

  for ( i = 0; i < PID_MAX; i++) {
    if (ProcInfo->pidMap[i] == 0) {
      continue;
    }
    if (sys_rdu (CAP_ROOT, ProcInfo->pidMap[i], &cu) < 0) {
      continue;
    }

    switch (__envs[envidx (ProcInfo->pidMap[i])].env_status) {
    case ENV_DEAD: state = "dead"; break;
    case ENV_QXX: state = "qxx"; break;
    case ENV_OK:
      switch (cu.u_status) {
      case U_SLEEP: state = "sleep"; break;
      case U_RUN: 
	switch (cu.state) {
	case P_RUNNING: state = "run"; break;
	case P_ZOMBIE: state = "zombie"; break;
	}
	break;
      }
    }
    printf ("%3d  %3d %4d %7d:%3d %-25s %5s  %9u %9x\n",
	    i,
	    cu.parent,
	    GetRefCount (i),
	    ProcInfo->pidMap[i],
	    envidx(ProcInfo->pidMap[i]),
	    cu.name,
	    state,
	    __envs[envidx (ProcInfo->pidMap[i])].env_ticks,
	    (mypid == i) ? 0 : get_eip (i));
  }
#endif
}

int main (int argc, char **argv) {
#ifdef PROCESS_TABLE
  process_table_ps();
#endif
#ifdef PROCD
  extern void procd_ps();
  procd_ps();
#endif
  return 0;
}
	    

