/*
 * ExoKernel include files
 */

#include <xok/types.h>
#include <xok/printf.h> 
#include <xok/scheduler.h>
#include <xok/defs.h>
#include <xok/pmap.h>
#include <xok_include/assert.h>
#include <xok_include/errno.h>
#include <xok/sysinfo.h>
#include <xok/pkt.h>
#include <xok/bios32.h>
#include <machine/param.h>
#include <machine/endian.h>
#include <xok/ae_recv.h>
#include <xok/sys_proto.h>
#include <xok_include/string.h>
#include <xok/trap.h>
#include <xok_include/stdarg.h>
#include <xok/pctr.h>
#include <xok/sys_syms.h>


/*
 * Oskit include files 
 */
#include <dev/dev.h>
#include <oskit/machine/types.h>
#include <oskit/types.h>
#include <oskit/error.h>
#include <dev/device.h>

#include "pit_param.h"


/*
 * ------- Constant definitions.
 */

#define TICKS_IN_THE_FUTURE 10000 / si->si_rate

#define NO_TIMER_FLAGS 0

/*
 * ------- Local structures
 */

typedef struct timer_reg {
  struct timer_reg *next;  /* links for the two way linked list */
  struct timer_reg *prev;
  
  void (*func)(void);      /* the registered call back function */
  int freq;                /* how often func should be called - not used */
  int flags;               /* options - not used*/
} timer_reg_t;


/*
 * ------- External Global variables
 */
extern int booting_up;


/*
 * ------- Global data for this module
 */
/*  the head of the register timer list */
static timer_reg_t *timer_reg_list_head = NULL; 


/*
 * ------- external Functions.
 */
extern void bump_jiffies();


/*
 * ------- Local Function declarations
 */
static int add_to_timer_list(timer_reg_t *new_timer);
static void timer_handler();
static timer_reg_t *create_timer_reg();
static int delete_timer_reg(timer_reg_t **timer);
static int remove_timer_from_list(timer_reg_t *current_timer);


/*
 * ------- Functions
 */

void
osenv_timer_init()
{
  /*
   * Initializes the osenv_timer handling structures.
   */
  static int init_called = 0;
  
  if(init_called != 1)
	{
	  timer_reg_list_head = NULL;
	  timeout(timer_handler, NULL,TICKS_IN_THE_FUTURE );
	  init_called = 1;
	}
}


static void timer_handler()
{
  /*
   * The timeout_handler gets called ever hundreth of 
   * a second after bootup (as per the design of the oskit).
   * Once it is called, it scans the list of registered timers
   * and calls the timer function in each one.
   */
  
  timer_reg_t *current_reg;    /* an index to the registered timer list */
  
  /*
   * go to each item in the list and call its
   * callback function.
   */
  for(current_reg = timer_reg_list_head;
		current_reg != NULL;
		current_reg = current_reg->next)
	 {
		assert(current_reg->func != NULL);
		/* call the callback */
		(*current_reg->func)();
	 }
  
  /*
   * setup the next time the timer_handler
   * will be called.
   */
  timeout(timer_handler, NULL, TICKS_IN_THE_FUTURE);
}


static timer_reg_t *create_timer_reg()
{
  /* create_timer_reg()
   * creates and zero's out a timer structure.
   * it handles all the error checking.
   */
  timer_reg_t *new_timer;

  new_timer = (timer_reg_t *) malloc(sizeof(timer_reg_t));
  
  /* check for a failed malloc. */
  assert(new_timer != NULL);
  
  /* zero out the structure. */
  bzero(new_timer, sizeof(timer_reg_t));
  
  return new_timer;
}

static int delete_timer_reg(timer_reg_t **timer)
{
  /*
   * delete the timer. 
   */
  
  assert(*timer != NULL);
  free(*timer);
  *timer = NULL;
  return 0;
}


static int add_to_timer_list(timer_reg_t *new_timer)
{
  assert(new_timer != NULL);
  /*
   * setup new timer.
   */
  new_timer->next = timer_reg_list_head;
  new_timer->prev = NULL;

  /*
   * fix the head node of the list
   */
  if(timer_reg_list_head != NULL)
	{
	  timer_reg_list_head->prev = new_timer;
	}

  /*
   * set head to new item.
   */
  timer_reg_list_head = new_timer;

  return 0;
}

static int remove_timer_from_list(timer_reg_t *current_timer)
{
  /* check for null timer and empty list */
  assert(current_timer != NULL);

  if(timer_reg_list_head == NULL)
	{
	  /* trying to remove node from empty list */
	  return -1;
	}

  /*
   * check to see if we are deleting the head
   * node of the list.
   */
  if(current_timer == timer_reg_list_head)
	{
	  timer_reg_list_head = current_timer->next;
	}

  if(current_timer->prev != NULL)
	{
	  /* make sure there is a node before us. */
	  current_timer->prev->next = current_timer->next;
	}
  if(current_timer->next != NULL)
	{
	  current_timer->next->prev = current_timer->prev;
	}
  current_timer->prev = NULL;
  current_timer->next = NULL;
  return 0;
}

void osenv_timer_register(void (*func)(void), int freq)
{
  /*
   * osenv_timer_register adds a new timer item to 
   * the registered timer list.  The new timer's callback
   * function (func) will get called when timer_handler
   * is called by the Exokernel.
   */

  timer_reg_t *new_timer;    /* a pointer to the new timer struct */

  
  /* 
   * this must have been called once before 
   * the timer can be used.
   */
  osenv_timer_init();

  /* check input data */
  assert(func != NULL);
  
  /*
   * the oskit right now only supports timers
   * that run at 100 Hz... so thats all I'm
   * supporting either.
   */
  assert(freq == 100);

  /*
   * add timer to the list.
   * first make a new timer, then set it,
   * then add it.
   */
  new_timer = create_timer_reg();
  
  new_timer->func = func;
  new_timer->freq = freq;
  new_timer->flags = NO_TIMER_FLAGS;
	  
  add_to_timer_list(new_timer);

  if(booting_up)
	{
	  int i;
	  delay(TICKS_IN_THE_FUTURE);

	  /* artificially update jiffies */
	  for(i = 0; i < TICKS_IN_THE_FUTURE; i++)
		{
		  bump_jiffies();
		}
	  func();
	}
}

void
osenv_timer_unregister(void (*func)(void), int freq)
{
  /*
   * osenv_timer_unregister:
   * find the timer_reg with the callback
   * the same as func and freq the same as freq and
   * remove it from the list.
   */
  
  timer_reg_t *current_timer;
  
  /*
   * search the list for the timer to delete.
   */

  for(current_timer = timer_reg_list_head;
	  current_timer != NULL;
	  current_timer = current_timer->next)
	{
	  if(current_timer->func == func && 
		 current_timer->freq == freq)
		{
		  /*
		   * we have found the timer to be deleted,
		   * so stop looking.
		   */
		  break;
		}
	}
  
  remove_timer_from_list(current_timer);
  delete_timer_reg(&current_timer);
}

void
osenv_timer_shutdown()
{
  timer_reg_t *current_timer;
 
  printf("called osenv_timer_shutdown\n");

  /*
   * delete every timer in the list.
   */

  for(current_timer = timer_reg_list_head;
	  current_timer != NULL; 
	  )
	{	  
	  timer_reg_t *timer_to_delete = current_timer;
	  current_timer = current_timer->next;
	  delete_timer_reg(&timer_to_delete);
	}
  
  /*
   * don't dangle the head.
   */
  timer_reg_list_head = NULL;
}

/*
 * Spin for a small amount of time, specified in nanoseconds,
 * without blocking or enabling interrupts.
 */
void
osenv_timer_spin(long nanosec)
{
	int prev_val = osenv_timer_pit_read();

	while (nanosec > 0) {
		int val = osenv_timer_pit_read();
		if (val > prev_val)
			prev_val += TIMER_VALUE;
		nanosec -= (prev_val - val) * PIT_NS;
		prev_val = val;
	}
}


#if 0 /* 
	   * this version of timer register could be used if
	   * if timer.c in oskit/linux/dev is gutted and
	   * each call just calls one of these functions.
	   */
void osenv_timer_register(void (*func)(void), int freq)
{
  if(!booting_up)
	{
	  /*
	   * this is regular operation mode... so
	   * just use a timeout.
	   */
	  void (*new_func) (void *);
	  
	  new_func = (void *) func;
	  timeout(new_func, NULL, freq);
	}else{
	  /*
	   * a timeout during bootup... just delay
	   * for the set amount of time and then call the
	   * callback function.
	   */
	  printf("delaying %d then calling callback\n", TICKS_IN_THE_FUTURE);
	  delay(TICKS_IN_THE_FUTURE);

	  /*
	   * update jiffies so OS-kit device drivers will know
	   * that time has passed.
	   */
	  /*
	   * could do 1 / freq ?
	   */
	  func();
	}
}
#endif


