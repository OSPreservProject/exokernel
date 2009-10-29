
#ifndef __PW_THREADS_H__
#define __PW_THREADS_H__

extern int pw_thread_create(void (*start)(), unsigned short);
extern void pw_thread_die();
extern void pw_thread_exit();
extern int pw_thread_id();

#endif

