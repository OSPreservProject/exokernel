#ifndef _I386_H
#define _I386_H

int iopl(int i);
int i386_get_ioperm(unsigned long *i);
int i386_set_ioperm(unsigned long *i);

#endif
