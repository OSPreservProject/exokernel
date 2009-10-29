
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */

#include <exos/conf.h>

#include <exos/vm-layout.h>
#include <stdio.h>
#include <string.h>
#include <xok/defs.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <xok/sys_ucall.h>
#include <xok/mmu.h>
#include <exos/osdecl.h>
#include <fcntl.h>
#include <sys/param.h>
#include <exos/critical.h>
#include <exos/callcount.h>
#include <exos/mallocs.h>
#include <exos/exec.h>
#include <exos/magic.h>

#include <xok/ash.h>

#include <exos/process.h>
#include <exos/exec.h>
#ifdef SYSV_SHM
#include <sys/shm.h>
#endif

#include <assert.h>
#include <exos/vm.h>

#include <fd/fdstat.h>

#include <sys/time.h>
#include <err.h>

#include <paths.h>
#include <a.out.h>

#ifdef PROCD
#include "procd.h"
extern char *__progname;
#endif

/* if you want to be warned if the app is older than the shared library */
#define WARNOLDAPP

#define PATH_LIBEXOS "/usr/lib/libexos.so"


/* magic number for our minimal exos binary type */
#define EXOS_MAGIC 700

extern char **environ;

struct sl_data_struct {
  u_int text_pages;
  u_int data_pages;
  u_int bss_pages;
  long mod_time;
};

static struct sl_data_struct *sl_data = (void*)SHARED_LIBRARY_START_RODATA;

static int __zero_segment (int envid, u_int start, u_int sz);

#define EXEC_EXOS_AOUT	1
#define EXEC_SIMPLE	2
static int
fork_execve0_part2(const char *path, char *const argv[],
		   char * const envptr[], struct Env *e, int fd,
		   u_int flags);

int
__load_sl_image (int fd)
{
  struct exec hdr;
  int full_data_bytes;
  int full_text_bytes;
  int full_bss_bytes;
  int text_fragment;
  int data_fragment;

  /* read a.out headers */
  if (lseek(fd, 0, SEEK_SET) == -1 ||
      read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
    fprintf(stderr, "Invalid executable format.\n");
    return -ENOEXEC;
  }

  full_text_bytes = hdr.a_text + sizeof (hdr);
  full_data_bytes = hdr.a_data;
  full_bss_bytes = hdr.a_bss;
  text_fragment = full_text_bytes % NBPG;
  if (text_fragment) {
    full_text_bytes -= text_fragment;
    full_data_bytes += text_fragment;
  }
  data_fragment = full_data_bytes % NBPG;
  if (data_fragment) full_bss_bytes += NBPG - data_fragment;

  if (full_text_bytes > 0) {
    /* map in text segment */
    if (mmap((void*)SHARED_LIBRARY_START_RODATA, full_text_bytes,
	     PROT_READ | PROT_EXEC,
	     MAP_COPY | MAP_SHARED | MAP_FIXED | MAP_FILE, fd, (off_t)0) !=
	(void*)SHARED_LIBRARY_START_RODATA) {
      fprintf(stderr, "__load_sl_image: could not mmap text (errno = %d)\n",
	      errno);
      return -ENOEXEC;
    }
  }

  if (full_data_bytes > 0) {
    /* map in data segment */
    if (mmap((void*)SHARED_LIBRARY_START_RODATA+full_text_bytes,
	     full_data_bytes, PROT_READ,
	     MAP_COPY | MAP_SHARED | MAP_FIXED | MAP_FILE, fd,
	     (off_t)full_text_bytes) != (void*)SHARED_LIBRARY_START_RODATA +
	full_text_bytes) {
      fprintf(stderr, "__load_sl_image: could not mmap data (errno = %d)\n",
	      errno);
      return -ENOEXEC;
    }
    if (data_fragment) {
      assert(sys_self_mod_pte_range(0, PG_W, 0, SHARED_LIBRARY_START_RODATA +
				    full_text_bytes + full_data_bytes, 1)
	     == 0);
      bzero((void*)SHARED_LIBRARY_START_RODATA + full_text_bytes +
	    full_data_bytes, NBPG - data_fragment);
      assert(sys_self_mod_pte_range(0, 0, PG_W, SHARED_LIBRARY_START_RODATA +
				    full_text_bytes + full_data_bytes, 1)
	     == 0);
    }
  }

  /* bss will be mmaped at startup */

  /* make a private copy of the first page so we can store sl data at the */
  /* beginning.  alternatively, you might want to put it in the uarea */
  /* note: we know the text segment will have at least one page because the */
  /*  shared library is big! */
  assert(sys_self_mod_pte_range(0, PG_COW, PG_RO,
				SHARED_LIBRARY_START_RODATA, 1) == 0);
  sl_data->text_pages = PGNO(full_text_bytes);
  sl_data->data_pages = PGNO(PGROUNDUP(full_data_bytes));
  sl_data->bss_pages = PGNO(PGROUNDUP(full_bss_bytes));
    
  return (0);
}

static int
setup_new_stack(char *const argv[], char *const env[], u_int envid,
		u_int *newesp, struct _exos_exec_args *eea)
{
  int len=0, envsize=0, argsize=0, argc=0, envc=0, pages_needed, i;
  void *pages;
  u_int p, padding, reserved_start;
  struct ps_strings *ps;
  char **argvstr, **envstr;

  /* XXX - sanity check ponters */
  if (!argv || !env) return -EFAULT;

  /* figure out how much we'll be putting on the new stack */
  len += sizeof(struct ps_strings);
  len = ALIGN(len);
  len += sizeof(struct _exos_exec_args);
  len = ALIGN(len);
  /* the reserved (mapped, but unused) pages */
  padding = PGROUNDUP(len) - len;
  len += padding;
  len += NBPG * __RESERVED_PAGES;
  reserved_start = len;
  /* size of new args */
  while (argv[argc]) argsize += strlen(argv[argc++])+1;
  /* size of new environment */
  while (env[envc]) envsize += strlen(env[envc++])+1;
  len += envsize + argsize;
  len = ALIGN(len);
  /* for the pointers to the args & envs and their null termination */
  len += (envc + 1 + argc + 1) * sizeof(void*);
  len += sizeof(int); /* the argc */
  /* extra page so child has at least one totally free stack page */
  pages_needed = PGNO(PGROUNDUP(len)) + 1;
  pages = __malloc(pages_needed*NBPG);
  if (!pages) return -ENOMEM;
  p = (u_int)pages + pages_needed*NBPG - len;
  *(int*)p = argc;
  p += sizeof(int);
  argvstr = (char**)p;
  p += (argc+1)*sizeof(char*);
  envstr = (char**)p;
  p += (envc+1)*sizeof(char*);
  for (i=0; i < argc; i++) {
    strcpy((char*)p, argv[i]);
    argvstr[i] = (char*)(USTACKTOP - ((u_int)pages + pages_needed*NBPG - p));
    p += strlen(argv[i])+1;
  }
  argvstr[argc] = NULL;
  for (i=0; i < envc; i++) {
    strcpy((char*)p, env[i]);
    envstr[i] = (char*)(USTACKTOP - ((u_int)pages + pages_needed*NBPG - p));
    p += strlen(env[i])+1;
  }
  envstr[envc] = NULL;
  /* map (via touching) the reserved pages */
  for (i=0; i < __RESERVED_PAGES; i++) {
    *(u_int*)p = 0;
    p += NBPG;
  }
  p += padding;
  /* continue... */
  eea->eea_reserved_pages = __RESERVED_PAGES;
  eea->eea_reserved_first = (char*)(USTACKTOP - reserved_start);
  p = ALIGN(p);
  *(struct _exos_exec_args*)p = *eea;
  p += sizeof(struct _exos_exec_args);
  p = ALIGN(p);
  ps = (struct ps_strings*)p;
  ps->ps_argvstr = (char**)(USTACKTOP - ((u_int)pages + pages_needed*NBPG -
					 (u_int)argvstr));
  ps->ps_nargvstr = argc;
  ps->ps_envstr = (char**)(USTACKTOP - ((u_int)pages + pages_needed*NBPG -
					(u_int)envstr));
  ps->ps_nenvstr = envc;
  /* now map the pages into the new process */
  /* XXX - touch clean page so it'll be mapped */
  *(char*)pages = 0;
  if (__vm_share_region((u_int)pages, pages_needed*NBPG, 0, 0, envid,
			USTACKTOP-pages_needed*NBPG) == -1 ||
      __vm_free_region((u_int)pages, pages_needed*NBPG, 0) == -1) {
    __free(pages);
    return -ENOMEM;
  }
  __free(pages);
  *newesp = USTACKTOP - len;

  return 0;
}


#define error(s) __free((void*)temp_page); fprintf (stderr, s); return -ENOEXEC;
#define errornf(s) fprintf (stderr, s); return -ENOEXEC;


static int
setup_new_stack_simple(char *const argv[], char *const env[], 
                       u_int envid, u_int *newesp)
{
  int len=0, argc=0, envc=0, pages_needed, i;
  void *pages;
  u_int p;
  char **argvstr;
  char **envstr;

  if (!argv || !env) return -EFAULT;

  /* figure out how much we'll be putting on the new stack */

  /* size of new args */
  while (argv[argc]) len += strlen(argv[argc++])+1;
  
  /* size of env vars  */
  while (env[envc]) len += strlen(env[envc++])+1;

  /* for the pointers to the args' null termination */
  len += (argc + 1) * sizeof(void*);
  
  /* for the pointers to the env vars' null termination */
  len += (envc + 1) * sizeof(void*);

  /* leave a space argc */
  len += sizeof(int); 
  
  /* leave a space for argv */
  len += sizeof(char*);
  
  /* leave a space for env */
  len += sizeof(char*);
  
  len = ALIGN(len);

  /* calculate how many pages we need, and always allocate at least 1 page */
  pages_needed = PGNO(PGROUNDUP(len))+1;
  pages = __malloc(pages_needed*NBPG);
  if (!pages) return -ENOMEM;

  /* now we put things on the page */
  p = (u_int)pages + pages_needed*NBPG - len;

  /* first, the argc */
  *(int*)p = argc;
  p += sizeof(int);

  /* then, the argv pointer */
  *(int*)p = (USTACKTOP - ((u_int)pages + pages_needed*NBPG - 
                           (p+sizeof(char*)+sizeof(char*))));
  p += sizeof(char*);

  /* then, the env pointer */
  *(int*)p = (USTACKTOP - ((u_int)pages + pages_needed*NBPG -
      			   (p+sizeof(char*)+(argc+1)*sizeof(char*))));
  p += sizeof(char*);

  /* now, all the pointers to arguments */
  argvstr = (char**)p;
  p += (argc+1)*sizeof(char*);

  /* then, all the pointers to env vars */
  envstr = (char**)p;
  p += (envc+1)*sizeof(char*);

  /* copy in arguments, set up pointers to arguments */
  for (i=0; i < argc; i++) {
    argvstr[i] = (char*)(USTACKTOP - ((u_int)pages + pages_needed*NBPG - p));
    strcpy((char*)p, argv[i]);
    p += strlen(argv[i])+1;
  }
  argvstr[argc] = NULL;

  /* copy in env variables, set up pointers to env vars */
  for (i=0; i < envc; i++) {
    envstr[i] = (char*)(USTACKTOP - ((u_int)pages + pages_needed*NBPG - p));
    strcpy((char*)p, env[i]);
    p += strlen(env[i])+1;
  }
  envstr[envc] = NULL;


  /* now map the pages into the new process */
  *(char*)pages = 0;
  if (__vm_share_region((u_int)pages, pages_needed*NBPG, 0, 0, envid,
			USTACKTOP-pages_needed*NBPG) == -1 ||
      __vm_free_region((u_int)pages, pages_needed*NBPG, 0) == -1) {
    __free(pages);
    return -ENOMEM;
  }
  __free(pages);

  /* dude, this is totally a GCC thing: gcc convention says when starting
   * a program first argument is esp+4. That's the -4 here */
  *newesp = USTACKTOP - len - 4;
  
  return 0;
}


/* load an EXOS_MAGIC binary */
int
__do_simple_load (int fd, struct Env *e)
{
  // struct Uenv cu;

  u_int start_text_addr, start_text_pg;
  struct exec hdr;
  u_int text_size, data_size, bss_size, overlap_size;
  u_int envid = e->env_id;


  /* read a.out headers */
  if (lseek(fd, 0, SEEK_SET) == -1 ||
      read(fd, &hdr, sizeof(hdr)) != sizeof(hdr) ||
      lseek(fd, sizeof(hdr) + hdr.a_text, SEEK_SET) == -1 ||
      read(fd, &start_text_addr, sizeof(start_text_addr)) != 
          sizeof(start_text_addr)) 
  {
    errornf("Invalid executable format.\n");
  }

  start_text_pg = PGROUNDDOWN(start_text_addr);
  text_size = hdr.a_text + sizeof(hdr);
  data_size = hdr.a_data;
  if (text_size % NBPG) {
    data_size += text_size % NBPG;
    text_size = PGROUNDDOWN(text_size);
  }
  bss_size = hdr.a_bss;
  
  
  if (!(data_size % NBPG))
    overlap_size = 0;
  else
  {
    /* read in the page that contains both bss and inited data */
    u_int temp_page;
     
    temp_page = (u_int)__malloc(NBPG);
    overlap_size = NBPG;
   
    if (temp_page == 0 || 
	lseek(fd, text_size + PGROUNDDOWN(data_size), SEEK_SET) == -1 ||
        read(fd, (void*)temp_page, data_size % NBPG) != data_size % NBPG ||
        _exos_insert_pte
	  (0, vpt[PGNO(temp_page)], start_text_pg + text_size +
	   PGROUNDDOWN(data_size), 0, envid, 0, NULL) != 0) 
    {
      _exos_self_unmap_page(0, temp_page);
      error("Error mmaping text segment\n");
    }
    
    bzero((void*)temp_page + (data_size % NBPG),
          NBPG - (data_size % NBPG));
    _exos_self_unmap_page(0, temp_page);
    __free((void*)temp_page);
    bss_size -= NBPG - (data_size % NBPG);
    bss_size = PGROUNDUP(bss_size);
    data_size = PGROUNDDOWN(data_size);

  }


  /* mmap the text segment readonly */
  if ((u_int)__mmap((void*)start_text_pg, text_size, PROT_READ  | PROT_EXEC, 
		    MAP_FILE | MAP_FIXED | MAP_COPY, fd, (off_t)0, 0, envid)
	!= start_text_pg) 
  {
    errornf("Error mmaping text segment\n");
  }

  /* mmap the data segment read/write */
  if ((u_int)__mmap((void*)(start_text_pg + text_size), data_size,
		    PROT_READ | PROT_WRITE | PROT_EXEC,
		    MAP_FILE | MAP_FIXED | MAP_COPY,
		    fd, text_size, (off_t)0, envid)
	!= start_text_pg + text_size) 
  {
    errornf("Error mmaping data segment\n");
  }

#if 0 /* we set up a stack page later on when setting up arguments */
  /* allocate a stack page */
  if (_exos_insert_pte (0, PG_U|PG_W|PG_P, USTACKTOP-NBPG, 0, envid, 0,
			NULL) < 0) 
  {
    errornf("could not allocate stack\n");
  }
#endif

  /* set the entry point */
  assert(e->env_id == envid);
  e->env_tf.tf_eip = start_text_addr;

  return 1;
}



static int
is_exos_aout(int fd)
{
  struct exec hdr;
  u_int dynamic;

  if (lseek(fd, 0, SEEK_SET) == -1 ||
      read(fd, &hdr, sizeof(hdr)) != sizeof(hdr) ||
      lseek(fd, sizeof(hdr) + hdr.a_text, SEEK_SET) == -1 ||
      read(fd, &dynamic, sizeof(dynamic)) != sizeof(dynamic))
    return 0;

  if (N_GETMAGIC(hdr) != OMAGIC ||
      N_GETMID(hdr) != MID_I386 ||
      N_GETFLAG(hdr) != 0)
    return 0;

  return 1;
}

static int
exec_exos_aout(int fd, const char *path, char *const argv[],
	       char *const envp[], struct Env *e, u_int flags)
{
  u_int envid = e->env_id;
  struct exec hdr;
  u_int dynamic;
  int r;
  struct _exos_exec_args eea;

  if (lseek(fd, 0, SEEK_SET) == -1 ||
      read(fd, &hdr, sizeof(hdr)) != sizeof(hdr) ||
      lseek(fd, sizeof(hdr) + hdr.a_text, SEEK_SET) == -1 ||
      read(fd, &dynamic, sizeof(dynamic)) != sizeof(dynamic))
    return 0;

  if (N_GETMAGIC(hdr) != OMAGIC ||
      N_GETMID(hdr) != MID_I386 ||
      N_GETFLAG(hdr) != 0)
    return 0;

  if (!(flags & _EXEC_USE_FD)) {
    close(fd);
    fd = -1;
  } else if (lseek(fd, 0, SEEK_SET) == -1)
    return 0;

  if (dynamic < SHARED_LIBRARY_START) dynamic = 0;
  if (dynamic == 0 && (flags & _EXEC_SHLIB_ONLY)) {
    r = -EINVAL;
    goto err;
  }

  /* if static, then read in entire program... */
  if (!dynamic) {
    u_int start_text_addr;

    if (flags & _EXEC_USE_FD)
      start_text_addr = __load_prog_fd(fd, 1, envid);
    else
      start_text_addr = __load_prog(path, argv[0], 1, envid);
    if (!start_text_addr) {
      r = -ENOEXEC;
      goto err;
    }
    /* set start address */
    e->env_tf.tf_eip = start_text_addr;
  }
  /* if dynamic, then make sure shared library is available */
  else if (dynamic) {
    struct stat sb;

    /* If flag so indicates, then require RO copy and use in mem version */
    if (!(flags & _EXEC_SHLIB_ONLY)) {
      /* if the shared library is not already in memory, or if it's old */
      /* then we need to (re)read it in */
      if (stat(PATH_LIBEXOS, &sb)) {
	r = -errno;
	kprintf("error stat'ing sl: %d\n", errno);
	goto err;
      }
      /* if we don't have a RO copy or it's out of date... */
      if (!isvamapped(SHARED_LIBRARY_START_RODATA) ||
	  memcmp(&sl_data->mod_time, &sb.st_mtimespec,
		 sizeof(sb.st_mtimespec))) {
	int slfd;

	/* if it's out of date, then unmap it */
	if (isvamapped(SHARED_LIBRARY_START_RODATA))
	  munmap((void*)SHARED_LIBRARY_START_RODATA,
		 (sl_data->text_pages + sl_data->data_pages) * NBPG);
	slfd = open (PATH_LIBEXOS, O_RDONLY);
	if (slfd < 0) {
	  r = -errno;
	  kprintf("could not open shared library " PATH_LIBEXOS "\n");
	  goto err;
	}
	if ((r = __load_sl_image (slfd)) < 0) {
	  kprintf("could not load library\n");
	  close(slfd);
	  goto err;
	}
	*((struct timespec*)&sl_data->mod_time) = sb.st_mtimespec;
	assert(sys_self_mod_pte_range(0, PG_RO, PG_W,
				      SHARED_LIBRARY_START_RODATA, 1) == 0);
	close (slfd);
      }
    } else {
      if (!isvamapped(SHARED_LIBRARY_START_RODATA)) {
	r = -EINVAL;
	goto err;
      }
    }

    /* share the text region read only/exec into child */
    if (__vm_share_region(SHARED_LIBRARY_START_RODATA,
			  sl_data->text_pages * NBPG, 0, 0,
			  envid, SHARED_LIBRARY_START)) {
      kprintf ("__vm_share_region failed for text region\n");
      r = -ENOEXEC;
      goto err;
    }

    /* share the read only data region into child cow */
    if (__vm_share_region(SHARED_LIBRARY_START_RODATA +
			  sl_data->text_pages * NBPG,
			  sl_data->data_pages * NBPG, 0, 0, envid,
			  SHARED_LIBRARY_START + sl_data->text_pages * NBPG)) {
      kprintf ("__vm_share_region failed for cow data region\n");
      r = -ENOEXEC;
      goto err;
    }
    if (sys_mod_pte_range(0, PG_COW, PG_RO | PG_SHARED, SHARED_LIBRARY_START +
			  sl_data->text_pages * NBPG, sl_data->data_pages, 0,
			  envid) < 0) {
      kprintf ("sys_mod_pte_range failed for cow data region\n");
      r = -ENOEXEC;
      goto err;
    }

    /* share the RO copy of the shared library */
    if (__vm_share_region(SHARED_LIBRARY_START_RODATA,
			  (sl_data->text_pages + sl_data->data_pages) * NBPG,
			  0, 0, envid, SHARED_LIBRARY_START_RODATA)) {
      kprintf ("__vm_share_region failed for read only text/data region\n");
      r = -ENOEXEC;
      goto err;
    }

    /* demand alloc bss for sl or no? */    
    if (getenv("NO_BSS_DEMAND_ALLOC")) {
      /* zero the bss */
      if (__zero_segment (envid, SHARED_LIBRARY_START +
			  (sl_data->text_pages + sl_data->data_pages) *
			  NBPG, sl_data->bss_pages * NBPG) < 0) {
	kprintf("could not create bss segment\n");
	r = -ENOEXEC;
	goto err;
      }
     } /* otherwise the fault handler will deal with it */
    
    /* set start address */
    e->env_tf.tf_eip = SHARED_LIBRARY_START + sizeof(struct exec);
  }

  /* take care of args, etc */
  if (flags & _EXEC_USE_FD)
    eea.eea_prog_fd = fd;
  else
    eea.eea_prog_fd = -1;
  if ((r = setup_new_stack(argv, envp, envid, &e->env_tf.tf_esp, &eea)) < 0)
    goto err;

  return 1;

 err:
  return r;
}
  

static int
exec_interp(int fd, int assume_sh, const char *path, char *const argv[],
	    char *const envp[], struct Env *e, u_int flags)
{
  // u_int envid = e->env_id;
  char *ipath;
  char **newv = (char **)argv;
  char *extra_argv_space;
  int ret;

  if (assume_sh == 0) {
    int intersize;
    char *interp;
    char cmagic[2];
    char inter[MAXINTERP+1];

    /* read in first two bytes */
    if (lseek(fd, 0, SEEK_SET) == -1 ||
	read(fd, cmagic, 2) < 2)
      return 0;

    /* check for interpreter */
    if (cmagic[0] != '#' || cmagic[1] != '!') return 0;

    if ((intersize = read(fd, inter, MAXINTERP)) == -1) {
      if (!(flags & _EXEC_USE_FD)) close(fd);
      ret = -errno;
      goto err;
    }
    inter[intersize] = '\n';
    interp = inter;

    /* skip spaces */
    while ((*interp == ' ' || *interp == '\t') &&
	   *interp != '\n')
      interp++;

    ipath = interp;
    while(*newv++);
    extra_argv_space = (char *)__malloc((char *)newv - (char *)argv + 
					3 * (sizeof(char*)));
    if (!extra_argv_space) {
      ret = -ENOMEM;
      if (!(flags & _EXEC_USE_FD)) close(fd);
      goto err;
    }

    newv = (char **)extra_argv_space;
    *newv++ = interp;

    while (*interp != ' ' && *interp != '\t' && 
	   *interp != (char)0 && *interp != '\n')
      interp++;
    
    if (*interp != 0 && *interp != '\n') {
      /* more arguments, we only copy one more */
      *interp++ = 0;
      /* skip spaces */
      while ((*interp == ' ' || *interp == '\t') && 
	     *interp != '\n')
	interp++;

      *newv++ = interp;
      while (*interp != ' ' && *interp != '\t' && 
	     *interp != (char)0 && *interp != '\n')
	interp++;
      *interp = (char)0;
    } else
      *interp = (char)0;

    *newv++ = (char*)path;
    argv++;
    while(*argv != (char *) 0)
      *newv++ = *argv++;
    /* copy the 0 */
    *newv = *argv;

    argv = (char **)extra_argv_space;
  } else {
    ipath = "/bin/sh";
    while(*newv++);
    extra_argv_space = (char *)__malloc((char *)newv - (char *)argv +
					2 * (sizeof(char*)));
    if (!extra_argv_space) {
      ret = -ENOMEM;
      if (!(flags & _EXEC_USE_FD)) close(fd);
      goto err;
    }
    newv = (char **)extra_argv_space;
    *newv++ = ipath;
    *newv++ = (char*)path;
    argv++;
    while(*argv != (char *) 0)
      *newv++ = *argv++;
    /* copy the 0 */
    *newv = *argv;

    argv = (char **)extra_argv_space;
  }

  if (!(flags & _EXEC_USE_FD)) close(fd);

  ret = fork_execve0_part2(ipath, argv, envp, e, -1, flags);

  __free(extra_argv_space);

  return ret;

 err:
  return ret;
}

static int
exec_simple(int fd, const char *path, char *const argv[],
	    char *const envp[], struct Env *e, u_int flags)
{
  struct exec hdr;
  int r;

  if (lseek(fd, 0, SEEK_SET) == -1 ||
      read(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
    return 0;

  if (N_GETMAGIC(hdr) == EXOS_MAGIC) 
  {
    int ret;

    ret = __do_simple_load (fd, e);
    if (ret < 0) {
      fprintf (stderr, "Could not load EXOS_MAGIC executable\n");
      if (!(flags & _EXEC_USE_FD)) close(fd);
      return -ENOEXEC;
    }
   
    if ((r = setup_new_stack_simple(argv,envp,e->env_id,&e->env_tf.tf_esp)<0))
      return r;
    return 1;
  }

  return 0;
}

static int
is_emul_openbsd(int fd)
{
  struct exec hdr;
  int layout, os;

  if (lseek(fd, 0, SEEK_SET) == -1 ||
      read(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
    return 0;

  os = check_magic(hdr.a_midmag, &layout);

  if (os == BF_AOUT_386BSD ||
      (os == BF_AOUT_OPENBSD &&
       layout != BF_OMAGIC)) /* XXX - reserved for exos */
    return 1;

  return 0;
}

static int
exec_emul_openbsd(int fd, const char *path, char *const argv[],
		  char *const envp[], struct Env *e, u_int flags)
{
  // u_int envid = e->env_id;
  char **newv = (char**) argv;
  int ret;
  char *extra_argv_space;

  if (!is_emul_openbsd(fd)) return 0;

  if (!(flags & _EXEC_USE_FD)) close(fd);

  while(*newv++);
  extra_argv_space = (char *)__malloc((char *)newv - (char *)argv +
				      2 * sizeof(char*));
  if (!extra_argv_space) {
    fprintf(stderr,"dynamic loader could not allocate extra argv space "
	    "for copying args for emulator\n");
    ret = -ENOMEM;
    goto exec_emul_openbsd_done;
  }

  newv = (char **)extra_argv_space;
  *newv = getenv("EMULATOR");
  if (!newv[0]) newv[0] = "/bin/emulate";
  newv++;

  *newv++ = (char *)path;
  argv++;
  while(*argv != (char *) 0)
    *newv++ = *argv++;
  /* copy the 0 */
  *newv = *argv;

  argv = (char **)extra_argv_space;

  ret = fork_execve0_part2(argv[0], argv, envp, e, -1, flags);

  __free(extra_argv_space);

 exec_emul_openbsd_done:
  return ret;
}

static int
fork_execve0_part2(const char *path, char *const argv[],
		   char * const envptr[], struct Env *e, int fd,
		   u_int flags)
{
  int r;

  /* verify executable permission */
  if (fd == -1 && access(path, X_OK) == -1)
    /* access will set errno */
    return -errno;

  if (fd == -1) {
    fd = open(path, O_RDONLY);
    if (fd < 0) return -errno;
  }

  do {
    r = exec_exos_aout(fd, path, argv, envptr, e, flags);
    if (r < 0) goto err;
    if (r > 0) return EXEC_EXOS_AOUT;
    r = exec_interp(fd, 0, path, argv, envptr, e, flags);
    if (r < 0) goto err;
    if (r > 0) return r;
    r = exec_simple(fd, path, argv, envptr, e, flags);
    if (r < 0) goto err;
    if (r > 0) return EXEC_SIMPLE;
    r = exec_emul_openbsd(fd, path, argv, envptr, e, flags);
    if (r < 0) goto err;
    if (r > 0) return r;
    r = exec_interp(fd, 1, path, argv, envptr, e, flags);
    if (r < 0) goto err;
  } while (0);

  assert(0);
 err:
  return r;
}

/* everything is based on this exec, we have an extra flag
   (_EXEC_EXECONLY) to differentiate between the exec and fork_exec
   families.  the difference is that the latter forks and then execs
   the process thereby returning in the parent
   */
static int
fork_execve0(const char *path, char *const argv[], char * const envptr[],
	     int fd, u_int flags)
{
  u_int k = 0;
  int target_cpu = 0;
  struct Uenv cu;
  int NewPid = 0;
  int envid = 0; /* XXX -- init to supress warning */
  char **old_argv = (char **)argv;
  int r, exec_format;
  struct Env e;

  OSCALLENTER(OSCALL_execve);
  /* verify executable permission */
  if (!(flags & _EXEC_USE_FD) && access(path, X_OK) == -1) {
    /* access will set errno */
    r = -errno;
    goto err;
  }

  if (!(envid = sys_env_alloc (0, &r))) {
    fprintf(stderr,"could not sys_env_alloc\n");
    r = -ENOEXEC;
    goto err;
  }

  e = __envs[envidx(envid)];
  if ((exec_format = fork_execve0_part2(path, argv, envptr, &e, fd, flags)) < 0)
    goto err_env_free;

  /* write environment */
  if ((r = sys_wrenv (k, envid, &e)) < 0) {
    kprintf ("sys_wrenv failed\n");
    r = -ENOEXEC;
    goto err;
  }

  if (ExecuteOnExecHandlers(k, envid, flags & _EXEC_EXECONLY) == -1) {
    fprintf(stderr,"cleanup code not done yet\n");
    assert(-1);
  }

#ifdef PROCESS_TABLE
  if (flags & _EXEC_EXECONLY) {
    /* because true exec */
    NewPid = getpid();
  
    dlockputs(__PROCD_LD,"fork_execve0 get lock ");
    EXOS_LOCK(PROCINFO_LOCK);
    dlockputs(__PROCD_LD,"... got lock\n");
    EnterCritical (); 

    ProcChangeEnv(NewPid,envid);
    
    EXOS_UNLOCK(PROCINFO_LOCK);
    dlockputs(__PROCD_LD,"fork_execve0 release lock\n");
    ExitCritical (); 

  } else {
    /* because we are forking */
    NewPid = AllocateFreePid (envid);
  }

  cu = UAREA;
  if (!execonly) {
    AddProcEntry (&cu, (char *)path, (char **)argv, NewPid, UAREA.pid);
    if ((cu.parent_slot = GetChildSlot (NewPid)) < 0) {
      r = -ENOEXEC;
      goto err_env_free;
    }
  } else {
    /* TO TOM: what do we do this for?
    strncpy (UAREA.name, (char*)(((u_int)argv[0]) + ARGV_START_LOCAL - ARGV_START),
	     U_NAMEMAX-1);
    UAREA.name[U_NAMEMAX-1] = '\0'; */
  }
  /* XXX -- on an exec we'll forget to unref our children's pids */
  /* TO TOM: shouldnt this clearchildinfo be at the top */
  ClearChildInfo (&cu);
#else
#ifdef PROCD
  if (flags & _EXEC_EXECONLY) 
  {
    NewPid = getpid();
    /* only register us if we are of the right format */ 
    if (exec_format == EXEC_EXOS_AOUT)
      proc_exec(envid);
  } else {
    NewPid = proc_fork(envid);
  }
  cu = UAREA;
  //kprintf("NewPid %d envid: %d %s proc_%s: (%d) -> %d\n",
	    //NewPid,__envid,execonly ? "exec" : "fork",path,envid,NewPid);
#else
  NewPid = envid;
#endif /* PROCD */
#endif /* PROCESS_TABLE */

  strncpy (cu.name, path, U_NAMEMAX-1);
  cu.name[U_NAMEMAX-1] = '\0';
  cu.u_fpu = 0;
  cu.u_in_critical = 0;
  cu.u_status = U_RUN;
  cu.u_entprologue = e.env_tf.tf_eip;
  cu.u_entepilogue = e.env_tf.tf_eip;
  cu.u_entfault = 0;
  cu.u_entipc1 = 0;
  cu.u_entipc2 = 0;
  cu.u_ppc = 0;
#ifdef PROCESS_TABLE
  cu.u_chld_state_chng = 0;
#endif /* PROCESS_TABLE */
  cu.u_next_timeout = 0;
  cu.u_in_pfault = 0;
  cu.u_donate = -1;
  cu.u_yield_count = 0;
  cu.u_epilogue_count = 0;
  cu.u_epilogue_abort_count = 0;

  STOPP(misc,step7);
  ISTART(misc,step8);

  cu.u_start_kern_call_counting = 0;

  if ((r = sys_wru (0, envid, &cu)) < 0) {
    fprintf (stderr,"sys_wru failed\n");
    r = -ENOEXEC;
    goto err_env_free;
  }

  target_cpu = 0;

#ifdef __SMP__
  if (strncmp(path,"_cpu",4)==0 && strlen(path)>4) 
  {
    target_cpu = path[4]-'0';
    if (target_cpu<0 || target_cpu>sys_get_num_cpus()-1) 
      target_cpu = 0;
  }
#endif

  /* we can't do this until all the child state is setup */
  if ((r = sys_quantum_alloc (k, -1, target_cpu, envid)) < 0) {
    fprintf (stderr,"could not alloc quantum\n");
    r = -ENOEXEC;
    goto err_env_free;
  }

  /* if we're doing a true unix exec and not a combined fork/spawn
     we need to destroy ourselves since our child should have
     replaced us. */

  if (flags & _EXEC_EXECONLY) 
  {
    /* if not an exos format, should not expect normal child behaviors, so we
     * just quit from procd to avoid any hanging...
     */
    if (exec_format == EXEC_SIMPLE)
      proc_exit(0);

    /* we die anyways, so free quantum and env */
    ProcessFreeQuanta(__envid);
    sys_env_free (0, __envid);
  }

  if (old_argv != argv) __free((char**)argv);
  OSCALLEXIT(OSCALL_execve);
  return (NewPid);

err_env_free:
  ProcessFreeQuanta(__envid);
  sys_env_free (k, envid);

err:
  if (old_argv != argv) __free((char**)argv);
  errno = -r;
  OSCALLEXIT(OSCALL_execve);
  return -1;
}

int
fork_execve(const char *path, char *const argv[], char *envptr[])
{
  return fork_execve0(path, argv, envptr, -1, 0);
}

int
execve(const char *path, char *const argv[], char * const envptr[])
{
  return fork_execve0(path, argv, envptr, -1, _EXEC_EXECONLY);
}

int
__exos_execve(const char *path, char *const argv[], char * const envptr[],
	      int fd, u_int flags)
{
  return fork_execve0(path, argv, envptr, fd, flags);
}

int
fork_and_exec(const char *path, char * argv[])
{
  return fork_execve0(path, argv, environ, -1, 0);
}

int
fork_execv(const char *file, char *const argv[])
{
  return fork_execve0(file, argv, environ, -1, 0);
}

int
fork_execvp0(const char *file, char *const argv[], u_int flags)
{
  if (strchr (file, '/') == NULL)
    {
      char *path, *p;
      struct stat st;
      size_t len;
      uid_t uid;
      gid_t gid;
      int ngroups;
      gid_t groups[NGROUPS_MAX];
      char *name;
 
      path = getenv ("PATH");
      if (path == NULL)
        {
          /* There is no `PATH' in the environment.
             The default search path is the current directory
             followed by the path `confstr' returns for `_CS_PATH'.  
	     We will just search current directory */
	  
	  path = ".";
        }
 
      len = strlen (file) + 1;
      name = alloca (strlen (path) + len);
      uid = geteuid ();
      gid = getegid ();
      ngroups = getgroups (sizeof (groups) / sizeof (groups[0]), groups);
      p = path;
      do
        {
          path = p;
          p = strchr (path, ':');
          if (p == NULL)
            p = strchr (path, '\0');
 
          if (p == path)
            /* Two adjacent colons, or a colon at the beginning or the end
               of `PATH' means to search the current directory.  */
            (void) memcpy (name, file, len);
          else
            {
              /* Construct the pathname to try.  */
              (void) memcpy (name, path, p - path);
              name[p - path] = '/';
              (void) memcpy (&name[(p - path) + 1], file, len);
            }
          if (stat (name, &st) == 0 && S_ISREG (st.st_mode))
            {
              int bit = S_IXOTH;
              if (st.st_uid == uid)
                bit = S_IXUSR;
              else if (st.st_gid == gid)
                bit = S_IXGRP;
              else
                {
                  register int i;
                  for (i = 0; i < ngroups; ++i)
                    if (st.st_gid == groups[i])
                      {
                        bit = S_IXGRP;
                        break;
                      }
                }
              if (st.st_mode & bit)
                {
                  file = name;
                  break;
                }
            }
        }
      while (*p++ != '\0');
    }
 
  return fork_execve0(file, argv, environ, -1, flags);
}

int
fork_execvp(const char *file, char *const argv[])
{
  return fork_execvp0(file, argv, 0);
}

int
fork_execl(const char *path, const char *arg, ...)
{
  const char *argv[1024];
  unsigned int i;
  va_list args;
 
  argv[0] = arg;
 
  va_start (args, arg);
  i = 1;
  do
    {
      argv[i] = va_arg (args, const char *);
    } while (argv[i++] != NULL);
  va_end (args);
 
  return fork_execve0(path, (char **) argv, environ, -1, 0);
}

int
fork_execlp(const char *file, ...)
{
  const char *argv[1024];
  unsigned int i;
  va_list args;
 
  va_start (args, file);
 
  i = 0;
  do
    argv[i] = va_arg (args, const char *);
  while (argv[i++] != NULL);
 
  va_end (args);
 
  return fork_execvp(file, (char **)argv);
}

int
fork_execle(const char *path, const char *arg, ...)
{
  const char *argv[1024], *const *envp;
  unsigned int i;
  va_list args;
 
  va_start(args, arg);
  argv[0] = arg;
  i = 1;
  do
    {
      argv[i] = va_arg(args, const char *);
    } while (argv[i++] != NULL);
 
  envp = va_arg(args, const char *const *);
  va_end(args);
 
  return fork_execve0(path, (char *const *)argv, (char **)envp, -1, 0);
}

int
fork_exect(const char *path, char *const argv[], char *const envp[])
{
  return fork_execve0(path, argv, (char **)envp, -1, 0);
}

int
exect(const char *path, char *const argv[], char *const envp[])
{
  return fork_execve0(path, argv, (char **)envp, -1, _EXEC_EXECONLY);
}

#if 0
#include <aouthdr.h>
#include <filehdr.h>
#include <scnhdr.h>
#define MS_TEXT    0
#define MS_DATA    1
#define MS_BSS     2
#define MS_UNKNOWN 3
int
map_section(int k, int fd, SCNHDR *shdr, int envid)
{
  u_int page_count;
  Pte *ptes;
  int i, retval = 0, type;
  off_t curloc = lseek(fd, 0, SEEK_CUR);
  u32 start, zero_start, len;
  u_int temp_pages, num_completed, num_completed2;

  if (!strcmp(shdr->s_name, ".text"))
    type = MS_TEXT;
  else if (!strcmp(shdr->s_name, ".data"))
    type = MS_DATA;
  else if (!strcmp(shdr->s_name, ".bss"))
    type = MS_BSS;
  else
    {
      type = MS_UNKNOWN;
      return 0;
    }

  page_count = PGNO(PGROUNDUP(shdr->s_size));
  if (type == MS_BSS) {
    start = PGROUNDUP(shdr->s_vaddr);
    if (start != shdr->s_vaddr) page_count--;
  }
  else
    start = shdr->s_vaddr;
  if ((ptes = __malloc(sizeof(Pte) * page_count)) == 0) {
    return -1;
  }
  for (i=0; i < page_count; i++)
    ptes[i] = PG_U|PG_W|PG_P;
  
  temp_pages = (u_int)__malloc(page_count * NBPG);
  num_completed = 0;
  num_completed2 = 0;
  if (temp_pages == 0 ||
      _exos_self_insert_pte_range(k, ptes, page_count, temp_pages,
				  &num_completed, 0, NULL) < 0 ||
      _exos_insert_pte_range(k, &vpt[PGNO(temp_pages)], page_count, 
			     start, &num_completed2, k, envid, 0, NULL) < 0 ||
      (type != MS_BSS &&
       (lseek(fd, shdr->s_scnptr, SEEK_SET) != shdr->s_scnptr ||
	read(fd, (void*)temp_pages, shdr->s_size) != shdr->s_size ||
	lseek(fd, curloc, SEEK_SET) != curloc))) {
    if (temp_pages) __free((void*)temp_pages);
    retval = -1;
  }
  if (type == MS_BSS) {
    zero_start = temp_pages;
    len = page_count * NBPG;
  } else {
    zero_start = temp_pages + shdr->s_size;
    len = NBPG - (zero_start & PGMASK);
  }
  bzero((void*)zero_start, len);
  if (type == MS_TEXT)
    mprotect((void*)temp_pages, page_count*NBPG, PROT_READ);
  for (i=0; i < page_count; i++)
    ptes[i] = 0;
  num_completed = 0;
  _exos_self_insert_pte_range(k, ptes, page_count, temp_pages, &num_completed,
			      0, NULL);
  if (retval == -1) {
    num_completed2 = 0;
    _exos_insert_pte_range(k, ptes, page_count, start, &num_completed2, k,
			   envid, 0, NULL);
  }

  if (temp_pages) __free((void*)temp_pages);
  __free(ptes);
  return retval;
}
#endif

static int
__zero_segment (int envid, u_int start, u_int sz)
{
  u_int temp_pages;

  assert (!(start & PGMASK)); 

  temp_pages = (u_int)__malloc(PGROUNDUP(sz));
  if (temp_pages == 0) return -1;

  /* alloc pages for this segment and map into our address space writeable */
  if (__vm_alloc_region (temp_pages, sz, 0, PG_P|PG_U|PG_W) < 0) {
    __free((void*)temp_pages);
    return -1;
  }
  /* and map them into the other address space */
  if (__vm_share_region (temp_pages, sz, 0, 0, envid, start) < 0) {
    __free((void*)temp_pages);
    return -1;
  }
  /* zero the pages */
  bzero ((void *)temp_pages, sz);
  /* and remove our mapping of the pages */
  if (__vm_free_region (temp_pages, sz, 0) < 0) {
    __free((void*)temp_pages);
    return -1;
  }

  __free((void*)temp_pages);
  return 0;
}  
  
u_int
__load_prog(const char *path, const char *arg0, int _static, u_int envid)
{
  int fd;
  u_int ret;

 open_binary:
  /* open the executable */
  fd = open (path, O_RDONLY, 0644);
  if (fd < 0) {
    /* open will set errno */
    fprintf(stderr,"shlib could not open executable %s, errno: %d\n", path,
	    errno);
    return 0;
  }

  /* emulator or interpretor */
  if (is_emul_openbsd(fd) || !is_exos_aout(fd)) {
    close(fd);
    path = arg0;
    goto open_binary;
  }

  ret = __load_prog_fd(fd, _static, envid);

  close(fd);

  return ret;
}

u_int
__load_prog_fd(int fd, int _static, u_int envid)
{
  u_int start_text_addr;
  struct exec hdr;
  u_int text_size, data_size, bss_size, overlap_size;
  u_int dynamic, start_text_pg;

  /* read a.out headers */
  if (lseek(fd, 0, SEEK_SET) == -1 ||
      read(fd, &hdr, sizeof(hdr)) != sizeof(hdr) ||
      lseek(fd, sizeof(hdr) + hdr.a_text, SEEK_SET) == -1 ||
      read(fd, &dynamic, sizeof(dynamic)) != sizeof(dynamic) ||
      read(fd, &start_text_addr, sizeof(start_text_addr)) !=
      sizeof(start_text_addr)) {
    fprintf(stderr,"Invalid executable format.\n");
    errno = ENOEXEC;
    goto err;
  }
  start_text_pg = PGROUNDDOWN(start_text_addr);
  text_size = hdr.a_text + sizeof(hdr);
  data_size = hdr.a_data;
  if (text_size % NBPG) {
    data_size += text_size % NBPG;
    text_size = PGROUNDDOWN(text_size);
  }
  bss_size = hdr.a_bss;
  if (_static) {
    if (!(data_size % NBPG))
      overlap_size = 0;
    else
      {
	/* read in the page that contains both bss and inited data */
	u_int temp_page;
	
	temp_page = (u_int)__malloc(NBPG);
	overlap_size = NBPG;
	if (temp_page == 0 ||
	    lseek(fd, text_size + PGROUNDDOWN(data_size),
		  SEEK_SET) == -1 ||
	    read(fd, (void*)temp_page, data_size % NBPG) !=
	    data_size % NBPG ||
	    _exos_insert_pte(0, vpt[PGNO(temp_page)],
			     start_text_pg + text_size +
			     PGROUNDDOWN(data_size), 0, envid, 0, NULL) != 0) {
	  _exos_self_unmap_page(0, temp_page);
	  __free((void*)temp_page);
	  fprintf(stderr,"Error mmaping text segment\n");
	  goto err;
	}
	bzero((void*)temp_page + (data_size % NBPG),
	      NBPG - (data_size % NBPG));
	_exos_self_unmap_page(0, temp_page);
	__free((void*)temp_page);
	bss_size -= NBPG - (data_size % NBPG);
	bss_size = PGROUNDUP(bss_size);
	data_size = PGROUNDDOWN(data_size);
      }
    /* mmap the text segment readonly */
    if ((u_int)__mmap((void*)start_text_pg, text_size,
		      PROT_READ  | PROT_EXEC, 
		      MAP_FILE | MAP_FIXED | MAP_COPY, fd, (off_t)0, 0,
		      envid)
	!= start_text_pg) {
      fprintf(stderr,"Error mmaping text segment\n");
      goto err;
    }
    /* mmap the data segment read/write */
    if ((u_int)__mmap((void*)(start_text_pg + text_size), data_size,
		      PROT_READ | PROT_WRITE | PROT_EXEC,
		      MAP_FILE | MAP_FIXED | MAP_COPY,
		      fd, text_size, (off_t)0, envid)
	!= start_text_pg + text_size) {
      fprintf(stderr,"Error mmaping data segment\n");
      goto err;
    }
  } else {
    /* if dynamic... */
    u_int mflags;
    if (!(data_size % NBPG))
      overlap_size = 0;
    else
      {
	/* read in the page that contains both bss and inited data */
	overlap_size = NBPG;
	if (_exos_self_insert_pte(0, PG_P | PG_W | PG_U,
				  start_text_pg + text_size +
				  PGROUNDDOWN(data_size), 0, NULL) < 0 ||
	    lseek(fd, text_size + PGROUNDDOWN(data_size),
		  SEEK_SET) == -1 ||
	    read(fd, (void*)(start_text_pg + text_size +
			     PGROUNDDOWN(data_size)),
		 data_size % NBPG) != data_size % NBPG) {
	  fprintf(stderr,"Error mmaping text segment\n");
	  goto err;
	}
	bzero((void*)(start_text_pg + text_size + data_size),
	      NBPG - (data_size % NBPG));
	bss_size -= NBPG - (data_size % NBPG);
	bss_size = PGROUNDUP(bss_size);
	data_size = PGROUNDDOWN(data_size);
      }
    /* mmap the text segment readonly */
    mflags = MAP_FILE | MAP_FIXED;
    if (getenv("NO_DEMAND_LOAD"))
      mflags |= MAP_COPY;
    else
      mflags |= MAP_SHARED;
    if ((u_int)mmap((void*)start_text_pg, text_size,
		    PROT_READ | PROT_EXEC, 
		    mflags, fd, (off_t)0) != start_text_pg) {
      fprintf(stderr,"Error mmaping text segment\n");
      goto err;
    }
    /* mmap the data segment read/write */
    if (!(mflags & MAP_COPY)) mflags = MAP_FILE | MAP_FIXED | MAP_PRIVATE;
    if ((u_int)mmap((void*)(start_text_pg + text_size), data_size,
		    PROT_READ | PROT_WRITE | PROT_EXEC, mflags, fd,
		    (off_t)text_size) != start_text_pg + text_size) {
      fprintf(stderr,"Error mmaping data segment: %d\n", errno);
      goto err;
    }
    /* mmap the bss as demand zero'd */
    if ((u_int)mmap((void*)(start_text_pg + text_size + data_size +
			    overlap_size),
		    bss_size, PROT_READ | PROT_WRITE | PROT_EXEC,
		    MAP_ANON | MAP_FIXED | MAP_PRIVATE,
		    (off_t)-1, 0) !=
	start_text_pg + text_size + data_size + overlap_size) {
      fprintf(stderr,"Error mmaping bss\n");
      goto err;
    }
  }

  return start_text_addr;

err:
  return 0;
}
