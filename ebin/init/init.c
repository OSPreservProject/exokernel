#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <xuser.h>
#include <sys/mmu.h>
#include <sys/sysinfo.h>

#if 0

static int ipc1in (int, int, int, int, u_int) __attribute__ ((regparm (3)));
static int
ipc1in (int a1, int a2, int a3, int a4, u_int envid)
{
  printf ("env 0x%x: IPC from env 0x%x: %d, %d, %d, %d\n",
	  geteid (), envid, a1, a2, a3, a4);
  return (0xbad00bad);
}
void handler () {
  printf ("handler!\n");
}

void
send_pkt (void *pkt, u_int len)
{
  while (__sysinfo.si_smc[0].txb_inuse == __sysinfo.si_smc[0].txb_cnt)
    ;
#if 0
  printf ("Sending packet with length %d in buffer %d.\n",
	  len, __sysinfo.si_smc[0].txb_new);
#endif
  asm ("pushl %0; popl %%es" :: "i" (GD_ED0));
  bcopy (pkt, NULL, len);
  asm ("pushl %0; popl %%es" :: "i" (GD_UD));
  sys_ed0xmit (len);
}

#endif

int
main (int argc, char **argv)
{
  printf ("I'm a user!\n");
  printf ("   (in environment 0x%x)\n", geteid ());

#if 0
  {
    u_int eid;
    eid = fork ();
    printf ("   fork = %d; EID = 0x%x\n", eid, geteid ());
    printf (".. goodbye from environment 0x%x\n", geteid ());
  }
#endif
  for (;;)
    ;
  return (0);
}
