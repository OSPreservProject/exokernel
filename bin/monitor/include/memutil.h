#ifndef _MEMUTIL_H_
#define _MEMUTIL_H_

#include "types.h"

int verify_lina_range(u_int lina, Bit32u flags, u_int bytes);
int verify_lina(u_int va, Bit32u flags);

int set_memory_lina(u_int lina, Bit32u val, int bytes);
int set_memory(u_int sel, u_int off, Bit32u val, int bytes);
Bit32u make_lina(Bit32u sel, Bit32u offset);

int get_memory_lina(u_int lina, Bit32u *result, int bytes);
int get_memory(u_int sel, u_int off, Bit32u *result, int bytes);

#endif
