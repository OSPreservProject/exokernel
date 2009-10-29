#ifndef _TSS_H_
#define _TSS_H_

#include "types.h"

/* Task state segment format (as described by the Pentium architecture
 * book). */
struct Ts {
    u_int    ts_link;         /* Old ts selector */
    u_int    ts_esp0;         /* Stack pointers and segment selectors */
    u_short  ts_ss0;          /* after an increase in privilege level */
    u_short _ts_ss0;
    u_int    ts_esp1;
    u_short  ts_ss1;
    u_short _ts_ss1;
    u_int ts_esp2;
    u_short  ts_ss2;
    u_short _ts_ss2;
    u_int    ts_cr3;          /* Page directory base */
    u_int    ts_eip;          /* Saved state from last task switch */
    u_int    ts_eflags;
    u_int    ts_eax;          /* More saved state (registers) */
    u_int    ts_ecx;
    u_int    ts_edx;
    u_int    ts_ebx;
    u_int    ts_esp;
    u_int    ts_ebp;
    u_int    ts_esi;
    u_int    ts_edi;
    u_short  ts_es;         /* Even more saved state (segment selectors) */
    u_short _ts_es;
    u_short  ts_cs;
    u_short _ts_cs;
    u_short  ts_ss;
    u_short _ts_ss;
    u_short  ts_ds;
    u_short _ts_ds;
    u_short  ts_fs;
    u_short _ts_fs;
    u_short  ts_gs;
    u_short _ts_gs;
    u_short  ts_ldt;
    u_short _ts_ldt;
    u_short  ts_iomb;       /* I/O map base address */
    u_short  ts_t;          /* Trap on task switch */
};

#endif
