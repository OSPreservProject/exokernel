#ifndef __STDLIB_H__
#define __STDLIB_H__

#include <xok/types.h>

void     qsort (void *, size_t, size_t, int (*)(const void *, const void *));
int	 rand (void);

#define	RAND_MAX	0x7fffffff

#endif /* __STDLIB_H__ */
