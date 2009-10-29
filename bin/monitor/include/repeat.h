#ifndef _REPEAT_H_
#define _REPEAT_H_

#include "types.h"
#include "handler_utils.h"

typedef int (*repeat_setup_func_t)(unsigned char **lina, u_int *arg1, u_int *arg2);
typedef int (*repeat_emu_func_t  )(unsigned char **lina, u_int  arg1, u_int  arg2);

int emu_s_lods(unsigned char **lina, u_int *bytes_mask, u_int *delta);
int emu_e_lods(unsigned char **lina, u_int bytes_mask, u_int delta);
int emu_lods(unsigned char *lina);

int emu_s_cmps(unsigned char **lina, u_int *bytes_mask, u_int *delta);
int emu_e_cmps(unsigned char **lina, u_int bytes_mask, u_int delta);
int emu_cmps(unsigned char *lina);

int emu_s_movs(unsigned char **lina, u_int *bytes_mask, u_int *delta);
int emu_e_movs(unsigned char **lina, u_int bytes_mask, u_int delta);
int emu_movs(unsigned char *lina);

int emu_s_stos(unsigned char **lina, u_int *bytes, u_int *delta);
int emu_e_stos(unsigned char **lina, u_int bytes, u_int delta);
int emu_stos(unsigned char *lina);

int emu_s_ins(unsigned char **lina, u_int *bytes, u_int *delta);
int emu_e_ins(unsigned char **lina, u_int bytes, u_int delta);

int emu_s_outs(unsigned char **lina, u_int *bytes, u_int *delta);
int emu_e_outs(unsigned char **lina, u_int bytes, u_int delta);

int once  (unsigned char **lina, repeat_setup_func_t setup_func, repeat_emu_func_t emu_func);
int repeat(unsigned char **lina, repeat_setup_func_t setup_func, repeat_emu_func_t emu_func);





#endif
