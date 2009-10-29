#ifndef INT_H
#define INT_H

#include <xok/trap.h>

#include "types.h"
#include "descriptors.h"
#include "handler_utils.h"

struct gate_descr *get_idt_desc(u_int vect);

void int_vector_setup(void);

/*
   Queue to hold all pending hard-interrupts. When an interrupt is
   placed into this queue, it can include a function to be run
   prior to the actuall interrupt being placed onto the DOS stack,
   as well as include a function to be called after the interrupt
   finishes.
*/
extern void *interrupt_function[0x100];

extern int           int_queue_start;
extern int           int_queue_end;
extern unsigned int  check_date;
extern time_t        start_time;
extern unsigned long last_ticks;

#define IQUEUE_LEN 1000
struct int_queue_struct {
  int interrupt;
  int (*callstart) ();
  int (*callend) ();
};
extern struct int_queue_struct int_queue[IQUEUE_LEN];

/*
   This is here to allow multiple hard_int's to be running concurrently.
   Needed for programs that steal INT9 away from DOSEMU.
*/
#define NUM_INT_QUEUE 64
struct int_queue_list_struct {
    struct int_queue_struct int_queue_ptr;
    int int_queue_return_addr;
    u_char in_use;
    struct Trapframe_vm86 saved_regs;
};
extern struct int_queue_list_struct int_queue_head[NUM_INT_QUEUE];

extern int int_queue_running;

void do_int(u_int);
void setup_interrupts(void);
void version_init(void);
void int_queue_run(void);
extern void queue_hard_int(int i, void (*callstart), void (*callend));

#define REVECT		0
#define NO_REVECT	1

extern int can_revector(int i);
void set_host_int(u_int i, const struct gate_descr *gate);
int merge_idt(void);
int irq_disabled(u_int irq);
int do_pe_int(u_int i);
int hardware_irq_number(u_int i);
void disable_irq(u_int irq);


#include <xok/env.h>
static inline void yield_now (int envid) {
  UAREA.u_donate = envid;
  asm ("pushl %0\n"
       "\tpushfl\n"
       "\tjmp _monitor_yield\n"
       :: "g" (&&next));
  goto next; /* get around warning about unused label */
next:
}


#endif
