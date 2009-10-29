
#ifndef	_KPRINTF_H_
#define	_KPRINTF_H_

#include <machine/ansi.h>

/* Add'l functions for those wanting prints to console... */
int    kprintf (const char *, ...) __attribute__((format (printf, 1, 2)));
int    ksprintf (char *, const char *, ...) __attribute__((format (printf, 2, 3)));
int    kvsprintf (char *, const char *, _BSD_VA_LIST_);

#endif /* _KPRINTF_H_ */
