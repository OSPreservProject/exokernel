#ifndef _HANDLER_UTILS_H
#define _HANDLER_UTILS_H

#include "types.h"
#include "descriptors.h"

#define EXC_DE	0
#define EXC_DB	1
#define EXC_NMI	2
#define EXC_BP	3
#define EXC_OF	4
#define EXC_BR	5
#define EXC_UD	6
#define EXC_NM	7
#define EXC_DF	8
/* no 9 */
#define EXC_TSS	10
#define EXC_NP	11
#define EXC_SS	12
#define EXC_GP	13
#define EXC_PF	14
/* no 15 */
#define EXC_MF	16
#define EXC_AC	17
#define EXC_MC	18

#define REGNO_EAX  0
#define REGNO_ECX  1
#define REGNO_EDX  2
#define REGNO_EBX  3
#define REGNO_ESP  4
#define REGNO_EBP  5
#define REGNO_ESI  6
#define REGNO_EDI  7
#define REGNO_ES   8
#define REGNO_CS   9
#define REGNO_SS  10
#define REGNO_DS  11
#define REGNO_FS  12
#define REGNO_GS  13

extern const Bit32u masks[];

u_int get_reg(int r, unsigned int bytes);
void set_reg(int r, Bit32u v, int bytes);
void get_prefixes(const unsigned char *csp);
void get_guest_mode(void);
int load_far_pointer(unsigned char **lina, int seg);
void _set_guest_exc(Bit8u vect, Bit32u erc);
int check_pop_available(u_int esp, u_int bytes);
int check_push_available(u_int esp, u_int bytes);
int check_eip(u_int tmp_cs, u_int tmp_eip);
int emu_set(int dstmem, Bit32u dst, Bit32u src, u_int bytes);
int emu_get(int srcmem, Bit32u src, Bit32u *dst, u_int bytes);
int mov_to_cr(int cr, u_int val, u_int mask);
int simple_iret(u_int tmp_eflags, u_int tmp_cs, u_int tmp_eip);
#ifdef NDEBUG
#define set_guest_exc(v, e) \
  error("guest threw exception #%x,%x at %x:%x, %s:%d\n", v, e, REG(cs), REG(eip), __FILE__,  __LINE__); \
  _set_guest_exc(v, e);
#else
#define set_guest_exc(v, e) \
  dprintf("guest threw exception #%x,%x at %x:%x, %s:%d\n", v, e, REG(cs), REG(eip), __FILE__,  __LINE__); \
  _set_guest_exc(v, e); \
  for(;;);
#endif


#endif
