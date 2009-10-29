/*
 *  This file contains useful constants.  They can be overridden for 
 *  site-specific uses.
 */
#ifndef __DPF_CONFIG_H__
#define __DPF_CONFIG_H__

/* 
 * If your compiler has inline, removing this definition should improve
 * performance a little bit.
 */
/* #define inline */

/* 
 * Routine to copy user filters into dpf.  Prototype: 
 *	copyin(void *dst, void *src, unsigned nbytes);
 */
#define dpf_copyin memcpy

/* System call annotation.  Mainly meaningless. */
#define SYSCALL

/* Minimum message size (in bytes). */
#define DPF_MINMSG     64

/* Minimum guarenteed message alignment (in bytes). */
#define DPF_MSG_ALIGN 8

/* Maximum number of elements in a DPF filter. */
#define DPF_MAXELEM 256

/* 
 * Maximum number of active DPF filters.  Size + 1 must fit in the data 
 * type defined by fid_t. 
 */
#define DPF_MAXFILT 256
typedef uint8 fid_t; 

/* 
 * Maximum number of instructions required to fab DPF trie.  There is
 * no good reason that clients should have to know this value.
 */
#define DPF_MAXINSTS	2048

/*
 * Percentage of entries in hash table that must be used before dpf will
 * rehash in response to a collision.  Given as an integer.
 */
#define DPF_HT_USAGE 50 /* 50% */

/* Size of initial hash table: larger means less rehashing. */
#define DPF_INITIAL_HTSZ 64

/* Maximum number of instructions per atom. */
#define DPF_MAXCODE 	64

#endif /* __DPF_CONFIG_H__ */
