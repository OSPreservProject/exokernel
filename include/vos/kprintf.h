
/*
 * Copyright 1999 MIT
 */

#ifndef	__VOS_KPRINTF_H__
#define	__VOS_KPRINTF_H__

#include <machine/ansi.h>
#include <xok/types.h>

/* Add'l functions for those wanting prints to console... */
int    kprintf (const char *, ...) __attribute__((format (printf, 1, 2)));
int    ksprintf (char *, const char *, ...) __attribute__((format (printf, 2, 3)));
int    kvsprintf (char *, const char *, _BSD_VA_LIST_);


int    printf (const char *, ...) __attribute__((format (printf, 1, 2)));

#endif /* __VOS_KPRINTF_H__ */

