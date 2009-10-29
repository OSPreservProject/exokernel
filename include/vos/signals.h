
#ifndef __VOS_SIGNALS_H__
#define __VOS_SIGNALS_H__

#include <vos/ipc-services.h>
#include <vos/ipc.h>

/* sends a SIGKILL to a process */
#define kill(pid,signo) ipc_msg(IPC_UNIX_SIGNALS,signo,0,0,0L,0,pid,0L)

#endif /* __VOS_SIGNALS_H__ */

