#include <exos/cap.h>
#include <exos/critical.h>
#include <exos/ipc.h>
#include <exos/uwk.h>
#include <stdio.h>
#include <xok/env.h>
#include <xok/sys_ucall.h>
#include <xok/pctr.h>
#include <xok/sysinfo.h>
#include <unistd.h>
#include <xok/mmu.h>
#include <xuser.h>
#include <signal.h>
#include <exos/vm.h>
#include <string.h>
#include <stdlib.h>

#define SECONDS 2

extern int __asm_ashipc(int, int, int, int, int) __attribute__ ((regparm(3)));
extern void setup_ash();

extern void test_proc(void);

int ipchandler(int a, int b, int c, int d, u_int e) {
  return 0;
}

void pctr_start(u_int func, int ctr) {
  ctr = ctr ? PCIOCS1 : PCIOCS0;

  func |= P6CTR_U | P6CTR_K | P6CTR_EN/* | P6CTR_E*/;
  sys_pctr(ctr, 0, &func);
}

void pctr_stop(u_int func, int ctr, u_quad_t i) {
  struct pctrst st;

  sys_pctr(PCIOCRD, 0, &st);
  if (!ctr)
    printf("%3d[0x%2x]: %10qu(%6qu/%6qu)    ", func, func, st.pctr_hwc[ctr],
	   st.pctr_hwc[ctr] / i, st.pctr_hwc[ctr] % i);
  else
    printf("%10qu(%6qu/%6qu)    i = %6qu\n", st.pctr_hwc[ctr],
	   st.pctr_hwc[ctr] / i, st.pctr_hwc[ctr] % i, i);
}

#define FD_PGS 512
static int garb[NBPG*FD_PGS];
void filldtlb() {
  int i;
  
  for (i=FD_PGS-1; i >= 0; i--)
    garb[i*NBPG] = i;
}

void usedtlb() {
  int i;

  for (i=FD_PGS-1; i >= 0; i--)
    assert(garb[i*NBPG] == i);
}

int main() {
  pctrval total, t;
  pid_t p;
  char *cp, *cp2;
  quad_t i;
  int x, eid, pc1;

  cp = malloc(NBPG*1000);
  assert(__vm_alloc_region((u_int)cp, NBPG*1000, 0, PG_W | PG_U | PG_P) == 0);

#if 1
#define J 512
total = 0;
for (i=0; i < 10; i++) {
EnterCritical();
{
  int j;
  pctrval t2;
  
  /* Trash TLB */
  for (j=J; j >= 0; j--)
    garb[j*NBPG] = j;

  /* Trash L1&L2 Cache & TLB */
  for (j=(J*NBPG/4)-1; j >= 0; j--)
    garb[j] = j;

  j = (J*NBPG/4)-1;
  t = rdtsc();
  if (garb[j] != j) goto eee;
  if (garb[j-NBPG*20] != j-NBPG*20) goto eee;
  t2 = rdtsc();
  if (!UAREA.u_interrupted)
    total += t2 - t;
  else
    i--;
}
ExitCritical();
}

printf("%qu\n", (total / i) - 33);
return 0;

 eee:
ExitCritical();
exit(-1);

#endif

  /* null ipc and ash-ipc calls */
  total = 0;
  i = 0;
  p = fork();
  if (p == 0) {
    int wkv;
    int eidp;

    eidp = pid2envid(getppid());
    wkv = 1;
    UAREA.u_entipca = 0x2000;
    setup_ash();
    ipc_register(IPC_PING, ipchandler);
    while (1)
#if 1
      wk_waitfor_value(&wkv, 0, CAP_ROOT);
#else
      yield(eidp);
#endif
  }
  eid = pid2envid(p);
  yield(eid);
  while (sipcout(eid, IPC_PING, 0, 0, 0) != 0);
  assert(__asm_ashipc(0,0,0,0,eid) == 0x0);
  for (pc1=0; pc1 < 0x10; pc1++) {
    yield(-1);
    i = 0;
    total = 0;
    EnterCritical();
    filldtlb();
    /*    pctr_start(pc1 / 2, pc1 % 2); */
    t = rdtsc();
    for (i=0; i < 5000; i++) {
      assert(sipcout(eid, IPC_PING, 0, 0, 0) == 0);
      /*      assert(__asm_ashipc(0,0,0,0,eid) == 0x0); */
      /*      test_proc(); */
      /*      sys_null(); */
      /*      yield(eid); */
      usedtlb();
    }
    if (!UAREA.u_interrupted)
      total = rdtsc() - t;
    else
      pc1--;
    /*    pctr_stop(pc1 / 2, pc1 % 2, i); */
    ExitCritical();
  }
  kill(p, SIGKILL);
  printf("ipc: %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);
  printf("%qu\n", (((total * 1000) / __sysinfo.si_mhz) / i) / FD_PGS);
return 0;

  printf("Some microbenchmarks:\n"
	 "This first set times each call inside the loop:\n");

  /* null direct yield */
  total = 0;
  i = 0;
  p = fork();
  if (p == 0) {
    int eidp = pid2envid(getppid());
    while (1) {
      yield(eidp);
    }
  }
  eid = pid2envid(p);
  yield(eid);
  while ((total / __sysinfo.si_mhz) < SECONDS*10 * 1000000) {
    i++;
    EnterCritical();
    t = rdtsc();
    yield(eid);
    total += rdtsc() - t;
    ExitCritical();
  }
  kill(p, SIGKILL);
  printf("direct yield: %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);

  /* null ipc and ash-ipc calls */
  total = 0;
  i = 0;
  p = fork();
  if (p == 0) {
    int wkv=1;

    UAREA.u_entipca = 0x2000;
    setup_ash();
    ipc_register(IPC_PING, ipchandler);
    while (1)
      wk_waitfor_value(&wkv, 0, CAP_ROOT);
  }
  yield(pid2envid(p));
  while (sipcout(pid2envid(p), IPC_PING, 0, 0, 0) != 0);
  while ((total / __sysinfo.si_mhz) < SECONDS * 1000000) {
    i++;
    EnterCritical();
    t = rdtsc();
    assert(sipcout(pid2envid(p), IPC_PING, 0, 0, 0) == 0);
    total += rdtsc() - t;
    ExitCritical();
  }
  printf("ipc: %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);
  /* ash part */
  total = 0;
  i = 0;
  while ((total / __sysinfo.si_mhz) < SECONDS * 1000000) {
    i++;
    EnterCritical();
    t = rdtsc();
    assert(__asm_ashipc(0,0,0,0,pid2envid(p)) == 0x0);
    total += rdtsc() - t;
    ExitCritical();
  }
  kill(p, SIGKILL);
  printf("ashipc: %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);

  /* vcopy a word */
  total = 0;
  i = 0;
  while ((total / __sysinfo.si_mhz) < SECONDS * 1000000) {
    i++;
    EnterCritical();
    t = rdtsc(); 
    assert(sys_vcopyout(&x, geteid(), (u_int)&x, sizeof(x)) == 0);
    total += rdtsc() - t;
    ExitCritical();
  }
  printf("vcopy: %qunsec\n",
	 ((total * 1000) / __sysinfo.si_mhz) / (i*1));

  /* procedure call */
  total = 0;
  i = 0;
  while ((total / __sysinfo.si_mhz) < SECONDS * 1000000) {
    i++;
    EnterCritical();
    t = rdtsc();
    test_proc();
    total += rdtsc() - t;
    ExitCritical();
  }
  printf("proc call: %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);

  /* null kernel call */
  total = 0;
  i = 0;
  while ((total / __sysinfo.si_mhz) < SECONDS * 1000000) {
    i++;
    EnterCritical();
    t = rdtsc();
    sys_null();
    total += rdtsc() - t;
    ExitCritical();
  }
  printf("sys_null: %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);

  /* bzero a page cold */
  total = 0;
  i = 0;
  cp = malloc(NBPG*500);
  assert(__vm_alloc_region((u_int)cp, NBPG*500, 0, PG_W | PG_U | PG_P) == 0);
  while ((total / __sysinfo.si_mhz) < SECONDS * 1000000) {
    i++;
    EnterCritical();
    t = rdtsc();
    bzero(cp, NBPG*500);
    total += rdtsc() - t;
    ExitCritical();
  }
  printf("bzero(cold): %qunsec\n",
	 ((total * 1000) / __sysinfo.si_mhz) / (i * 500));

  /* bcopy a page cold */
  total = 0;
  i = 0;
  cp2 = malloc(NBPG*500);
  assert(__vm_alloc_region((u_int)cp2, NBPG*500, 0, PG_W | PG_U | PG_P) == 0);
  while ((total / __sysinfo.si_mhz) < SECONDS * 1000000) {
    i++;
    EnterCritical();
    t = rdtsc();
    bcopy(cp, cp2, NBPG*500);
    total += rdtsc() - t;
    ExitCritical();
  }
  printf("bcopy(cold): %qunsec\n",
	 ((total * 1000) / __sysinfo.si_mhz) / (i*500));

  /* bzero a page cool */
  total = 0;
  i = 0;
  while ((total / __sysinfo.si_mhz) < SECONDS * 1000000) {
    i++;
    EnterCritical();
    t = rdtsc();
    bzero(cp, NBPG*20);
    total += rdtsc() - t;
    ExitCritical();
  }
  printf("bzero(cool): %qunsec\n",
	 ((total * 1000) / __sysinfo.si_mhz) / (i*20));

  /* bcopy a page cool */
  total = 0;
  i = 0;
  while ((total / __sysinfo.si_mhz) < SECONDS * 1000000) {
    i++;
    EnterCritical();
    t = rdtsc();
    bcopy(cp, cp2, NBPG*20);
    total += rdtsc() - t;
    ExitCritical();
  }
  printf("bcopy(cool): %qunsec\n",
	 ((total * 1000) / __sysinfo.si_mhz) / (i*20));

  /* bzero a page warm */
  total = 0;
  i = 0;
  while ((total / __sysinfo.si_mhz) < SECONDS * 1000000) {
    i++;
    EnterCritical();
    t = rdtsc();
    bzero(cp, NBPG);
    total += rdtsc() - t;
    ExitCritical();
  }
  printf("bzero(warm): %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);

  /* bcopy a page warm */
  total = 0;
  i = 0;
  while ((total / __sysinfo.si_mhz) < SECONDS * 1000000) {
    i++;
    EnterCritical();
    t = rdtsc();
    bcopy(cp, cp2, NBPG);
    total += rdtsc() - t;
    ExitCritical();
  }
  printf("bcopy(warm): %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);

  /* leave loop in calculation */
  printf("\nThis second set times the loop:\n");

  /* null direct yield */
  total = 0;
  i = 0;
  p = fork();
  if (p == 0) {
    int eidp = pid2envid(getppid());
    while (1) {
      yield(eidp);
    }
  }
  eid = pid2envid(p);
  yield(eid);
  t = rdtsc();
  for (i=1000000; i > 0; i--)
    yield(eid);
  total = rdtsc() - t;
  kill(p, SIGKILL);
  i = 1000000;
  printf("direct yield: %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);

  /* null ipc and ash-ipc calls */
  p = fork();
  if (p == 0) {
    int wkv=1;

    UAREA.u_entipca = 0x2000;
    setup_ash();
    ipc_register(IPC_PING, ipchandler);
    while (1)
      wk_waitfor_value(&wkv, 0, CAP_ROOT);
  }
  yield(pid2envid(p));
  while (sipcout(pid2envid(p), IPC_PING, 0, 0, 0) != 0);
  t = rdtsc();
  for (i=1000000; i > 0; i--)
    assert(sipcout(pid2envid(p), IPC_PING, 0, 0, 0) == 0);
  total = rdtsc() - t;
  i = 1000000;
  printf("ipc: %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);
  /* ash part */
  t = rdtsc();
  for (i=1000000; i > 0; i--)
    assert(__asm_ashipc(0,0,0,0,pid2envid(p)) == 0x0);
  total = rdtsc() - t;
  i = 1000000;
  printf("ashipc: %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);
  kill(p, SIGKILL);

  /* vcopy a word */
  t = rdtsc(); 
  for (i=10000; i > 0; i--)
    assert(sys_vcopyout(&x, geteid(), (u_int)&x, sizeof(x)) == 0);
  total = rdtsc() - t;
  i = 10000;
  printf("vcopy: %qunsec\n",
	 ((total * 1000) / __sysinfo.si_mhz) / (i*1));

  /* procedure call */
  t = rdtsc();
  for (i=1000000; i > 0; i--)
    test_proc();
  total = rdtsc() - t;
  i = 1000000;
  printf("proc call: %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);

  /* null kernel call */
  t = rdtsc();
  for (i=1000000; i > 0; i--)
    sys_null();
  total = rdtsc() - t;
  i = 1000000;
  printf("sys_null: %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);

  /* bzero a page cold */
  t = rdtsc();
  for (i=100; i > 0; i--)
    bzero(cp, NBPG*500);
  total = rdtsc() - t;
  i = 100;
  printf("bzero(cold): %qunsec\n",
	 ((total * 1000) / __sysinfo.si_mhz) / (i*500));

  /* bcopy a page cold */
  t = rdtsc();
  for (i=100; i > 0; i--)
    bcopy(cp, cp2, NBPG*500);
  total = rdtsc() - t;
  i = 100;
  printf("bcopy(cold): %qunsec\n",
	 ((total * 1000) / __sysinfo.si_mhz) / (i*500));

  /* bzero a page cool */
  t = rdtsc(); 
  for (i=10000; i > 0; i--)
    bzero(cp, NBPG*20);
  total = rdtsc() - t;
  i = 10000;
  printf("bzero(cool): %qunsec\n",
	 ((total * 1000) / __sysinfo.si_mhz) / (i*20));

  /* bcopy a page cool */
  t = rdtsc();
  for (i=10000; i > 0; i--)
    bcopy(cp, cp2, NBPG*20);
  total = rdtsc() - t;
  i = 10000;
  printf("bcopy(cool): %qunsec\n",
	 ((total * 1000) / __sysinfo.si_mhz) / (i*20));

  /* bzero a page warm */
  t = rdtsc();
  for (i=1000000; i > 0; i--)
    bzero(cp, NBPG);
  total = rdtsc() - t;
  i = 1000000;
  printf("bzero(warm): %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);

  /* bcopy a page warm */
  t = rdtsc();
  for (i=1000000; i > 0; i--)
    bcopy(cp, cp2, NBPG);
  total = rdtsc() - t;
  i = 1000000;
  printf("bcopy(warm): %qunsec\n", ((total * 1000) / __sysinfo.si_mhz) / i);

  return 0;
}
