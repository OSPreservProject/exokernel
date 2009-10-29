#ifndef _TASKS_H_
#define _TASKS_H_

#include "types.h"
#include "descriptors.h"
#include "handler_utils.h"

int load_tr(Bit16u selector);
int switch_tasks(Bit16u selector, struct seg_descr *tss);
int gate(unsigned char **lina, Bit16u selector, Bit32u offset);

#endif
