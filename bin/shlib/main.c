#include <assert.h>
#include <errno.h>
#include <exos/exec.h>
#include <exos/ptrace.h>
#include <exos/vm-layout.h>
#include <exos/vm.h>
#include <fcntl.h>
#include <exos/osdecl.h>
#include <exos/mallocs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <xok/mmu.h>
#include <sys/param.h>
#include <xok/sys_ucall.h>
#include <unistd.h>
#include <a.out.h>

extern int (*__main_trampoline)(int, char **, char **);

int main (int argc, char *argv[], char *envp[])
{
  u_int text_start_addr;
  int ret;

  if (__eea->eea_prog_fd != -1) {
    text_start_addr = __load_prog_fd(__eea->eea_prog_fd, 0, __envid);
    close(__eea->eea_prog_fd);
  } else
    text_start_addr = __load_prog(UAREA.name, argv[0], 0, __envid);
  if (!text_start_addr) return -1;

  __main_trampoline = (void*)text_start_addr;

  _exos_ptrace_setup();

  /* jump into dynamically loaded program */
  sys_cputs(""); /* tell me why the following statement uses absolute mode */
                 /* with the cputs and relative mode without.... */
  ret = __main_trampoline(argc, argv, envp);
  return ret;
}
