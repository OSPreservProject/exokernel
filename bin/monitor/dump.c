/* no globals */

#include <xok/env.h>
#include <xok/sys_ucall.h>

#include <stdlib.h>

#include "iodev.h"
#include "config.h"
#include "debug.h"
#include "memoryemu.h"
#include "pagetable.h"
#include "emu.h"
#include "cpu.h"
#include "port.h"
#include "int.h"
#include "memutil.h"


void show_cregs(void)
{
    int i;
    for(i=0; i<5; i++)
	printf("cr%d 0x%08x  ", i, vmstate.cr[i]);
    printf("\n");
}


void show_regs(char *file, int line)
{
    u_int peip = 9;  /* bytes to dump before eip */
    int i, wordsize;
    unsigned char *sp;
    unsigned char *cp;
    Bit32u pte;

#if 0
    if (verify_lina_range((u_int)REGS, ~PG_W, sizeof(*REGS)) != 0) {
	printf("failed to display registers\n");
	return;
    }
#endif

    if (IS_PE) {
	wordsize = 4;
    } else {
	wordsize = 2;
    }
    cp = (unsigned char *)make_lina(REG(cs), REG(eip));
    sp = (unsigned char *)make_lina(REG(ss), REG(esp));

    if (file) {
	printf("state as of: file=%s, line=%d\n", file, line);
    }
    printf("eax 0x%08x  ebx 0x%08x  ecx 0x%08x  edx 0x%08x  eip 0x%08x\n",
	REG(eax), REG(ebx), REG(ecx), REG(edx), REG(eip));
    printf("edi 0x%08x  esi 0x%08x  ebp 0x%08x  esp 0x%08x  ss  0x%08x\n",
	REG(edi), REG(esi), REG(ebp), REG(esp), REG(ss ));
    printf("cs  0x%08x  ds  0x%08x  es  0x%08x  fs  0x%08x  gs  0x%08x\n",
	REG(cs ), REG(ds ), REG(es ), REG(fs ), REG(gs ));
    printf("eflags 0x%08x  ", REG(eflags));
    for (i = (1 << 0x14); i > 0; i = (i >> 1)) {
	if (i & 0x80808080)
	    printf(" ");
	printf((REG(eflags) & i) ? "1" : "0");
    }
    printf("  ");

    /* display vflags symbolically...the #f "stringizes" the macro name */
#define PFLAG(f)  if (REG(eflags)&(f)) printf(#f" ")
    PFLAG(CF);
    PFLAG(PF);
    PFLAG(AF);
    PFLAG(ZF);
    PFLAG(SF);
    PFLAG(TF);
    PFLAG(IF);
    PFLAG(DF);
    PFLAG(OF);
    PFLAG(NT);
    PFLAG(RF);
    PFLAG(VM);
    PFLAG(AC);
    PFLAG(VIF);
    PFLAG(VIP);
    printf("iopl %u\n", GUEST_IOPL);

    printf("stack [%x] ", (u_int)sp);
    for (i = 0; (u_int)sp+i*wordsize < 0xfffffffc && i < 4; i++) {
	if (get_host_pte((u_int)sp+(i*wordsize), &pte)!=0 || !(pte & PG_P))
	    printf("(not present) ");
	else {
	    if (wordsize==4)
		printf("0x%08x ", *((Bit32u*)sp+i));
	    else
		printf("0x%04x ", *((Bit16u*)sp+i));
	}
    }
    printf("...\n");

    if ((u_int)cp < peip)
	peip = (u_int)cp;
    printf("code  [%x] ", (u_int)(cp-peip));
    for (i=-peip; i<9; i++) {
	if (i==0)
	    printf("-> ");
	if (0 /* !cp || get_host_pte((u_int)cp+i, &pte)!=0 || !(pte & PG_P) */) {
	} else {
	    printf("%02x ", *(cp+i));
	}
    }
    printf("[%x]\n", (u_int)(cp+9));
}


void show_ints(int min, int max)
{
    int i, b;

    max = (max - min) / 3;
    for (i = 0, b = min; i <= max; i++, b += 3) {
	debug("%02x| %04x:%04x->%05x    ", b, ISEG(b), IOFF(b),
		 IVEC(b));
	debug("%02x| %04x:%04x->%05x    ", b + 1, ISEG(b + 1), IOFF(b + 1),
		 IVEC(b + 1));
	debug("%02x| %04x:%04x->%05x\n", b + 2, ISEG(b + 2), IOFF(b + 2),
		 IVEC(b + 2));
    }
}


#define DEBUG_STACK_SIZE 5
static Bit32u debug_flag_stack[DEBUG_STACK_SIZE];
static int debug_flag_ptr = -1;
void push_debug_flags(void)
{
    ASSERT(debug_flag_ptr < DEBUG_STACK_SIZE && debug_flag_ptr >= -1);
    debug_flag_ptr ++;
    debug_flag_stack[debug_flag_ptr] = debug_flags;
}

void pop_debug_flags(void)
{
    ASSERT(debug_flag_ptr <= DEBUG_STACK_SIZE && debug_flag_ptr >= 0);
    debug_flags = debug_flag_stack[debug_flag_ptr];
    debug_flag_ptr --;
}


#ifndef NDEBUG
extern char *assert_file;
extern u_int assert_line;

void _Assert(char *strFile, unsigned int uLine)
{
    static int in_assert = 0;

    in_assert ++;
    if (in_assert > 1) {
	dprintf("recursive assertion!\n");
	abort();
    }
    dprintf("\nAssertion failed: %s, line %u\n", strFile, uLine);
    _leaveemu(ERR_ASSERT, strFile, uLine);
}
#endif


void _leaveemu(int sig, char *strFile, unsigned int uLine)
{
    void teardown(void);
    static int in_leaveemu = 0;

    in_leaveemu ++;
    if (in_leaveemu > 1) {
	printf("recursive leaveemu!\n");
	abort();
    }
    fflush(NULL);
    dprintf("leaveemu(%d) called from %s:%d; shutting down\n", sig, strFile, uLine);
    fflush(NULL);
    if (sig && sig!=ERR_CONFIG) {
	int c;
	show_regs(0, 0);
	printf("start monitor debugger? y/n\n");
	c = getchar();
	if (c=='y')
	    debugger();
    }
    teardown();
    exit(sig);
}
