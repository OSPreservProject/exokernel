#ifndef _VM_86_H_
#define _VM_86_H_

#include <xok/trap.h>
#include "types.h"

struct vm86_struct {
    tfp_vm86 regs;
    u_int err;
    u_int trapno;
    unsigned int cpu_type;
    unsigned char int_byuser[32];	/* 256 bits each: pass control to user */
};

#define VCPU_086		0
#define VCPU_186		1
#define VCPU_286		2
#define VCPU_386		3
#define VCPU_486		4
#define VCPU_586		5
#define VCPU_686		6
#define CPU_086		VCPU_086
#define CPU_186		VCPU_186
#define CPU_286		VCPU_286
#define CPU_386		VCPU_386
#define CPU_486		VCPU_486
#define CPU_586		VCPU_586
#define CPU_686		VCPU_686

#endif
