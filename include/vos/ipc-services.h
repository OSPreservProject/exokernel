
#ifndef __VOS_IPC_SERVICES__
#define __VOS_IPC_SERVICES__

#define MIN_IPC_NUMBER		0
#define MIN_APP_IPC_NUMBER	128
#define MAX_IPC_NUMBER		255
#define MAX_IPC_HANDLERS	(MAX_IPC_NUMBER-MIN_IPC_NUMBER+1)

#define IPC_TEST		0
#define IPC_UNIX_SIGNALS	1 	/* abs/signals - unix signals */
#define IPC_MAP_IPCPORT		2	/* abs/ipcport - map ipc port */


#endif

