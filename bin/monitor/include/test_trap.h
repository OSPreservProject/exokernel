#ifndef _TEST_TRAP_H
#define _TEST_TRAP_H

#include "types.h"

void cr3_test(u_int handler);
void lidt_test(u_int handler);
void lgdt_test(u_int handler);
void hlt_test(u_int handler);
void trap_test(void);


#endif
