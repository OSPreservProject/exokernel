#include "sls.h"
#include <errno.h>

#define _ctype_ S_ctype_
#define _C_ctype_ S_C_ctype_
extern const char *S_ctype_;

#define open S_OPEN
#define read S_READ
#define write S_WRITE
#define fseek S_LSEEK
#define dup  S_DUP
#define dup2 S_DUP2
#define close S_CLOSE
#define mmap S_MMAP
#define mprotect S_MPROTECT
#define munmap S_MUNMAP
#define  perror(A) S_PRINTF(A,-1)
#define slskprintf(A) S_PRINTF(A,0)
#define  slsprintf(A) S_PRINTF(A,1)
#define slskprintd(A) S_PRINTD(A,0)
#define  slsprintd(A) S_PRINTD(A,1)
#define slskprinth(A) S_PRINTD(A,2)
#define  slsprinth(A) S_PRINTD(A,3)
#define opendir S_OPENDIR
#define readdir S_READDIR
#define closedir S_CLOSEDIR

#define malloc S_MALLOC
#define free S_FREE
#define calloc S_CALLOC
#define realloc S_REALLOC
#define bzero   S_BZERO

#define strerror S_strerror
#define strsep S_strsep
#define strdup S_strdup
#define strcpy S_strcpy
#define strcmp S_strcmp
#define strncmp S_strncmp
#define __strerror S___strerror
#define strcat S_strcat
#define strchr S_strchr
#define memcpy S_memcpy
#define strtol S_strtol_10
#define getuid S_getuid
#define geteuid S_geteuid
#define getgid S_getgid
#define getegid S_getegid
#define getenv S_getenv
