
/*
 * Copyright 1999 MIT
 */


#ifndef _VOS_VM_LAYOUT_H_
#define _VOS_VM_LAYOUT_H_
#ifndef EXOS_COMPAT_MODE

#include <xok/mmu.h>
#include <xok/env.h>

#define PAGESIZE NBPG


/* 
 * temporary page region 
 */
#define TMP_START 0x10000000
#define TMP_SIZE  0x10000000
#define TMP_END   (TMP_START+TMP_SIZE)

#define FORK_TEMP_PG 	TMP_START
#define COW_TEMP_PG 	(FORK_TEMP_PG + PAGESIZE)


/* 
 * private region 
 */
#define PRIVATE_START TMP_END
#define PRIVATE_SIZE  0x10000000
#define PRIVATE_END   (PRIVATE_START+PRIVATE_SIZE)

#define XIO_SOCKBUF_START	PRIVATE_START
#define XIO_SOCKBUF_SZ		(512*PAGESIZE)
#define XIO_SOCKBUF_END		(XIO_SOCKBUF_START+XIO_SOCKBUF_SZ)


/* 
 * sbuf region 
 */
#define SBUF_WINDOW_START PRIVATE_END
#define SBUF_WINDOW_SIZE  0x10000000 
#define SBUF_WINDOW_END   (SBUF_WINDOW_START+SBUF_WINDOW_SIZE)



/* 
 * generic shared memory region 
 */
#define GEN_SHARED_START SBUF_WINDOW_END
#define GEN_SHARED_SZ    0x20000000
#define GEN_SHARED_END   (GEN_SHARED_START+GEN_SHARED_SZ)

/* global readable */
#define UINFO_REGION		GEN_SHARED_START
#define UINFO_REGION_SZ		(1*PAGESIZE)

/* global writable */
#define CONSOLE_REGION   	(UINFO_REGION+UINFO_REGION_SZ)
#define CONSOLE_REGION_SZ 	(2*PAGESIZE)

/* global readable */
#define IPTABLE_REGION		(CONSOLE_REGION+CONSOLE_REGION_SZ)
#define IPTABLE_REGION_SZ	(1*PAGESIZE)

/* global readable */
#define ARPTABLE_REGION		(IPTABLE_REGION+IPTABLE_REGION_SZ)
#define ARPTABLE_REGION_SZ	(1*PAGESIZE)

/* global readable */
#define PORTS_REGION		(ARPTABLE_REGION+ARPTABLE_REGION_SZ)
#define PORTS_REGION_SZ		(65*PAGESIZE)


/* 
 * dynamic memory region 
 */
#define DMEM_START   GEN_SHARED_END
#define MAX_DMEM_PGS (PGNO(0x3000000)-1)


#endif /* EXOS_COMPAT_MODE */
#endif /* _VOS_VM_LAYOUT_H_ */

