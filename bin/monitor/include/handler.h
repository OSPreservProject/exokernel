#ifndef _HANDLER_H_
#define _HANDLER_H_

#include "types.h"
#include "handler_utils.h"

void exc_C_handler_header(u_int trapno, u_int err);
void return_to_guest(void);


#endif
