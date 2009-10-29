#ifndef _TICK_H_

#define _TICK_H_

#include <xok/sysinfo.h>

int ae_gettick(void);
int ae_getrate(void);

static inline u_quad_t __usecs2ticks (u_quad_t usecs) {
  return (usecs / __sysinfo.si_rate);
}

static inline u_quad_t __ticks2usecs (u_quad_t ticks) {
  return (ticks * __sysinfo.si_rate);
}

#endif
