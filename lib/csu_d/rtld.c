
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

/*	$NetBSD: rtld.c,v 1.43 1996/01/14 00:35:17 pk Exp $	*/
/*
 * Copyright (c) 1993 Paul Kranenburg
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <sys/sys_ucall.h>
#include <exos/vm.h>
#include <exos/vm-layout.h>
#ifndef MAP_COPY
#define MAP_COPY	MAP_PRIVATE
#endif
#include <err.h>
#include "dlfcn.h"
#include <fcntl.h>
#include <a.out.h>
#include <stab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <sys/defs.h>

#include "ld.h" 

#include <sls_stub.h>

#define dprintf slsprintf
#define d2printf if (0) slsprintf
#define d3printf slskprintf
#define d3printd slskprintd
#define d3printh slskprinth


#ifndef MAP_ANON
#define MAP_ANON	0
#define anon_open() do {					\
  if ((anon_fd = open("/dev/zero", O_RDWR, 0)) == -1) {	        \
	      perror("open: /dev/zero\n");			\
              exit(1);                                          \
  } while (0)
#define anon_close() do {	\
	(void)close(anon_fd);	\
	anon_fd = -1;		\
} while (0)
#else
#define anon_open()
#define anon_close()
#endif

/*
 * Loader private data, hung off <so_map>->som_spd
 */
struct somap_private {
	int		spd_version;
	struct so_map	*spd_parent;
	int		spd_refcount;
	int		spd_flags;
#define RTLD_MAIN	1
#define RTLD_RTLD	2
#define RTLD_DL		4

#ifdef SUN_COMPAT
	long		spd_offset;	/* Correction for Sun main programs */
#endif
};

#define LM_PRIVATE(smp)	((struct somap_private *)(smp)->som_spd)

#ifdef SUN_COMPAT
#define LM_OFFSET(smp)	(LM_PRIVATE(smp)->spd_offset)
#else
#define LM_OFFSET(smp)	(0)
#endif

/* Base address for section_dispatch_table entries */
#define LM_LDBASE(smp)	(smp->som_addr + LM_OFFSET(smp))

/* Start of text segment */
#define LM_TXTADDR(smp)	(smp->som_addr == (caddr_t)0 ? PAGSIZ : 0)

/* Start of run-time relocation_info */
#define LM_REL(smp)	((struct relocation_info *) \
	(smp->som_addr + LM_OFFSET(smp) + LD_REL((smp)->som_dynamic)))

/* Start of symbols */
#define LM_SYMBOL(smp, i)	((struct nzlist *) \
	(smp->som_addr + LM_OFFSET(smp) + LD_SYMBOL((smp)->som_dynamic) + \
		i * (LD_VERSION_NZLIST_P(smp->som_dynamic->d_version) ? \
			sizeof(struct nzlist) : sizeof(struct nlist))))

/* Start of hash table */
#define LM_HASH(smp)	((struct rrs_hash *) \
	((smp)->som_addr + LM_OFFSET(smp) + LD_HASH((smp)->som_dynamic)))

/* Start of strings */
#define LM_STRINGS(smp)	((char *) \
	((smp)->som_addr + LM_OFFSET(smp) + LD_STRINGS((smp)->som_dynamic)))

/* Start of search paths */
#define LM_PATHS(smp)	((char *) \
	((smp)->som_addr + LM_OFFSET(smp) + LD_PATHS((smp)->som_dynamic)))

/* End of text */
#define LM_ETEXT(smp)	((char *) \
	((smp)->som_addr + LM_TXTADDR(smp) + LD_TEXTSZ((smp)->som_dynamic)))

/* PLT is in data segment, so don't use LM_OFFSET here */
#define LM_PLT(smp)	((jmpslot_t *) \
	((smp)->som_addr + LD_PLT((smp)->som_dynamic)))

/* Parent of link map */
#define LM_PARENT(smp)	(LM_PRIVATE(smp)->spd_parent)

static char		__main_progname[] = "main";
static char		*main_progname = __main_progname;
/* static char		us[] = "/usr/libexec/ld.so"; */

char			**dyn_environ;
extern char			*__dyn_progname;
int			errno;

/*static uid_t		uid, euid;
static gid_t		gid, egid; */
static int		careful;

struct so_map		*link_map_head, *main_map;
struct so_map		**link_map_tail = &link_map_head;
struct rt_symbol	*rt_symbol_head;

static char		*ld_library_path;
static char		*ld_preload_path;
static int		no_intern_search;
static int		ld_suppress_warnings;
static int		ld_warn_non_pure_code;

static int		ld_tracing;

static void		*__dlopen __P((char *, int));
static int		__dlclose __P((void *));
static void		*__dlsym __P((void *, char *));
static int		__dlctl __P((void *, int, void *));
static void		__dlexit __P((void));
u_long                   next_so_addr = SHARED_OBJECT_REGION;



static struct ld_entry	ld_entry = {
	__dlopen, __dlclose, __dlsym, __dlctl, __dlexit
};

       void		xprintf __P((char *, ...));
       int		rtld __P((int, struct crt_ldso *, struct _dynamic *));
       void		binder_entry __P((void));
       long		binder __P((jmpslot_t *));
static int		load_subs __P((struct so_map *));
static struct so_map	*map_object __P((struct sod *, struct so_map *));
static struct so_map	*alloc_link_map __P((	char *, struct sod *,
						struct so_map *, caddr_t,
						struct _dynamic *));
static inline void	check_text_reloc __P((	struct relocation_info *,
						struct so_map *,
						caddr_t));
static void		init_maps __P((struct so_map *));
static void		reloc_map __P((struct so_map *));
static void		reloc_copy __P((struct so_map *));
static void		call_map __P((struct so_map *, char *));
static char		*rtfindlib __P((char *, int, int, int *, char *));
static struct nzlist	*lookup __P((char *, struct so_map **, int));
static inline struct rt_symbol	*lookup_rts __P((char *));
static struct rt_symbol	*enter_rts __P((char *, long, int, caddr_t,
						long, struct so_map *));
static void		maphints __P((void));
static void		unmaphints __P((void));

static void		preload __P((char *));
static void		ld_trace __P((struct so_map *));

#include "md-static-funcs.c"

/*
 * Called from assembler stub that has set up crtp (passed from crt0)
 * and dp (our __DYNAMIC).
 */
int
rtld(version, crtp, dp)
	int			version;
	struct crt_ldso		*crtp;
	struct _dynamic		*dp;
{
	struct so_debug		*ddp;
	struct so_map		*smp;
	char *a;


	if (S_INIT()) {
	  d2printf("Found SLS Server\n");}
	else {
	  d3printf("ERROR: --------------> Couldn't Find SLS Server\n");
	  exit(0);}
	d2printf("Entering rtld\n");

  
	
	/* Check version */
	if (		version != CRT_VERSION_BSD_2 &&
			version != CRT_VERSION_BSD_3 &&
			version != CRT_VERSION_BSD_4 &&
			version != CRT_VERSION_SUN)
		return -1;

	/* Fixup __DYNAMIC structure */
	(long)dp->d_un.d_sdt += crtp->crt_ba;


	if (version >= CRT_VERSION_BSD_4)
		__dyn_progname = crtp->crt_ldso;

	if (version >= CRT_VERSION_BSD_3)
		main_progname = crtp->crt_prog;

	/* Setup out (private) environ variable */
	dyn_environ = crtp->crt_ep;

	/* Get user and group identifiers */
	careful = 0;
	/*uid = getuid(); euid = geteuid();
	gid = getgid(); egid = getegid();

	careful = (uid != euid) || (gid != egid);

	if (careful) {
	  perror("ERROR: uid != euid or gid != egid : can't unsetenv\n");
	  exit(-1);
	  unsetenv("LD_LIBRARY_PATH");
	  unsetenv("LD_PRELOAD");
	  
	} */

	/* Setup directory search */
	ld_library_path = getenv("LD_LIBRARY_PATH");
	add_search_path(ld_library_path);
	if (getenv("LD_NOSTD_PATH") == NULL)
		std_search_path();

	ld_suppress_warnings = getenv("LD_SUPPRESS_WARNINGS") != NULL;
	ld_warn_non_pure_code = getenv("LD_WARN_NON_PURE_CODE") != NULL;

	no_intern_search = careful || getenv("LD_NO_INTERN_SEARCH") != 0;

	anon_open();

	/*
	 * Init object administration. We start off with a map description
	 * for `main' and `rtld'.
	 */
	smp = alloc_link_map(main_progname, (struct sod *)0,(struct so_map *)0,
					(caddr_t)0, crtp->crt_dp);

	LM_PRIVATE(smp)->spd_refcount++;
	LM_PRIVATE(smp)->spd_flags |= RTLD_MAIN;

	/*  We are no longer relocating ld.so
	    smp = alloc_link_map(us, (struct sod *)0, (struct so_map *)0,
	                         (caddr_t)crtp->crt_ba, dp);
	    LM_PRIVATE(smp)->spd_refcount++;
	    LM_PRIVATE(smp)->spd_flags |= RTLD_RTLD;
	    */
	
	/* Handle LD_PRELOAD's here */
	ld_preload_path = getenv("LD_PRELOAD");

	if (ld_preload_path != NULL)
		preload(ld_preload_path);


	/* Load subsidiary objects into the process address space */
	ld_tracing = (int)getenv("LD_TRACE_LOADED_OBJECTS");
		
	load_subs(link_map_head);

	if (ld_tracing) {
	  ld_trace(link_map_head);
	  exit(0);
	}

	init_maps(link_map_head);

	/* Fill in some field in main's __DYNAMIC structure */
	if (version >= CRT_VERSION_BSD_4)
		crtp->crt_ldentry = &ld_entry;

	else
		crtp->crt_dp->d_entry = &ld_entry;

	crtp->crt_dp->d_un.d_sdt->sdt_loaded = link_map_head->som_next;

	ddp = crtp->crt_dp->d_debug;
	ddp->dd_cc = rt_symbol_head;
	if (ddp->dd_in_debugger) {
		caddr_t	addr = (caddr_t)((long)crtp->crt_bp & (~(PAGSIZ - 1)));

		/* Set breakpoint for the benefit of debuggers */
		if (mprotect(addr, PAGSIZ,
				PROT_READ|PROT_WRITE) == -1) {
			perror("Cannot set breakpoint in mainprogram\n");
			exit(1);
		}
		md_set_breakpoint((long)crtp->crt_bp,
				  (long *)&ddp->dd_bpt_shadow);
		if (mprotect(addr, PAGSIZ, PROT_READ) == -1) {
		  perror("Cannot re-protect breakpoint in mainprogram\n");
		  exit(1);
		}

		ddp->dd_bpt_addr = crtp->crt_bp;
		if (link_map_head)
			ddp->dd_sym_loaded = 1;
	}
	
	/* Close the hints file */
	unmaphints();
	
	slskprintf("close our fd\n");
	/* Close our file descriptor */
	/*(void)close(crtp->crt_ldfd);*/

	anon_close();
	a = (char *)malloc(70);
	strcpy(a,"Allocated ");
	strcat(a,ui2s((unsigned int)(sls_malloc_ptr -
				     BEGIN_TEMP_MALLOC_REGION)));
	strcat(a," bytes of memory, wasted ");
	strcat(a,ui2s((unsigned int)wasted_malloc_space));
	strcat(a,"bytes\n");
	d3printf(a);
        d2printf("Exiting rtld\n");

	return 0;
}


static int
load_subs(smp)
	struct so_map	*smp;
{
  char *wmsg, *n1, *n2;
  /*  printf("    Entering load_subs\n"); */

    for (; smp; smp = smp->som_next) {
      struct sod	*sodp;
      long		next = 0;
      if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
	continue;
      
      if (smp->som_dynamic) {
	next = LD_NEED(smp->som_dynamic);
      }

      while (next) {
	struct so_map	*newmap;
	
	sodp = (struct sod *)(LM_LDBASE(smp) + next);
	
	if ((newmap = map_object(sodp, smp)) == NULL) {
	  if (!ld_tracing) {
	    
	    char *fmt = (char *)sodp->sod_library;
	    if (fmt) {
	      n1 = ui2s((unsigned int)sodp->sod_major);
	      n2 = ui2s((unsigned int)sodp->sod_minor);
	      wmsg = (char *)malloc(strlen(main_progname) +
				    strlen(sodp->sod_name+LM_LDBASE(smp)) +
				    strlen(n1) +
				    strlen(n2) +
				    12);
	      strcpy(wmsg,main_progname);
	      strcat(wmsg,": lib");
	      strcat(wmsg,sodp->sod_name+LM_LDBASE(smp));
	      strcat(wmsg,".so.");
	      strcat(wmsg,n1);
	      strcat(wmsg,".");
	      strcat(wmsg,n2);
	      strcat(wmsg,"\n");
	      perror(wmsg);
	      free(wmsg);
	      free(n1);
	      free(n2);
	      exit(1);
	    }
	    else {
	      wmsg = (char *)malloc(strlen(main_progname) +
				    strlen(sodp->sod_name+LM_LDBASE(smp)) +
				    5);
	      strcpy(wmsg,main_progname);
	      strcat(wmsg,": ");
	      strcat(wmsg,sodp->sod_name+LM_LDBASE(smp));
	      strcat(wmsg,"\n");
	      perror(wmsg);
	      free(wmsg);
	      exit(1);
	    }
	  }
	  newmap = alloc_link_map(NULL, sodp, smp, 0, 0);
	}
	
	LM_PRIVATE(newmap)->spd_refcount++;
	next = sodp->sod_next;
      }
    }
  
  return 0;
}

void
ld_trace(smp)
	struct so_map	*smp;
{
	char	*fmt1, *fmt2, *fmt, *main_local;
	int	c;

	d2printf("Entering ld_trace \n");

	if ((main_local = getenv("LD_TRACE_LOADED_OBJECTS_PROGNAME")) == NULL)
		main_local = "";

	if ((fmt1 = getenv("LD_TRACE_LOADED_OBJECTS_FMT1")) == NULL)
		fmt1 = "\t-l%o.%m => %p (%x)\n";

	if ((fmt2 = getenv("LD_TRACE_LOADED_OBJECTS_FMT2")) == NULL)
		fmt2 = "\t%o (%x)\n";

	for (; smp; smp = smp->som_next) {
		struct sod	*sodp;
		char		*name, *path;
		char            tmpc[3];
		tmpc[0] = ' ';
		tmpc[1] = '\0';
		tmpc[2] = '\0';
		
		if ((sodp = smp->som_sod) == NULL)
			continue;

		name = (char *)sodp->sod_name;
		if (LM_PARENT(smp))
			name += (long)LM_LDBASE(LM_PARENT(smp));

		if ((path = smp->som_path) == NULL)
			path = "not found";

		fmt = sodp->sod_library ? fmt1 : fmt2;
		while ((c = *fmt++) != '\0') {
			switch (c) {
			default:
			        tmpc[0] = c;
				d2printf(tmpc);
				continue;
			case '\\':
				switch (c = *fmt) {
				case '\0':
					continue;
				case 'n':
					d2printf("\n");
					break;
				case 't':
					d2printf("\t");
					break;
				}
				break;
			case '%':
				switch (c = *fmt) {
				case '\0':
					continue;
				case '%':
				default:
				        tmpc[0] = c;
					d2printf(tmpc);
					break;
				case 'A':
					d2printf(main_local);
					break;
				case 'a':
					d2printf(main_progname);
					break;
				case 'o':
					d2printf(name);
					break;
				case 'm':
					d2printf(ui2s((unsigned int)sodp->sod_major));
					break;
				case 'n':
					d2printf(ui2s((unsigned int)sodp->sod_minor));
					break;
				case 'p':
					d2printf(path);
					break;
				case 'x':
					d2printf(ui2s((unsigned int)smp->som_addr));
					break;
				}
				break;
			}
			++fmt;
		}
	}
}

/*
 * Allocate a new link map for shared object NAME loaded at ADDR as a
 * result of the presence of link object LOP in the link map PARENT.
 */
static struct so_map *
alloc_link_map(path, sodp, parent, addr, dp)
	char		*path;
	struct sod	*sodp;
	struct so_map	*parent;
	caddr_t		addr;
	struct _dynamic	*dp;
{
	struct so_map		*smp;
	struct somap_private	*smpp;
	

	smpp = (struct somap_private *)xmalloc(sizeof(struct somap_private));
	smp = (struct so_map *)xmalloc(sizeof(struct so_map));
	smp->som_next = NULL;
	*link_map_tail = smp;
	link_map_tail = &smp->som_next;

	/*smp->som_sodbase = 0; NOT USED */
	smp->som_write = 0;
	smp->som_addr = addr;
	smp->som_path = path?strdup(path):NULL;
	if (smp->som_path) {
	  d2printf("alloc_link_map for -> ");
	  d2printf(smp->som_path);
	  d2printf("\n");
	}
	smp->som_sod = sodp;
	smp->som_dynamic = dp;

	smp->som_spd = (caddr_t)smpp;

/*XXX*/	if (addr == 0) main_map = smp;

	smpp->spd_refcount = 0;
	smpp->spd_flags = 0;
	smpp->spd_parent = parent;

#ifdef SUN_COMPAT
	smpp->spd_offset =
		(addr==0 && dp && dp->d_version==LD_VERSION_SUN) ? PAGSIZ : 0;
#endif
	return smp;
}


  /*
 * Map object identified by link object SODP which was found
 * in link map SMP.
 */
static struct so_map *
map_object(sodp, smp)
	struct sod	*sodp;
	struct so_map	*smp;
{
  char		*name;
  struct _dynamic	*dp;
  char		*path, *ipath;
  int		fd;
  caddr_t		addr;
  struct exec	hdr;
  int		usehints = 0;
  int           j, i;
  struct so_map	*p;
  
  name = (char *)sodp->sod_name;
  if (smp)
    name += (long)LM_LDBASE(smp);


  if (sodp->sod_library) {
    usehints = 1;
  again:
 

    if (smp == NULL || no_intern_search ||
	LD_PATHS(smp->som_dynamic) == 0) {
      ipath = NULL;
    } else {
      ipath = LM_PATHS(smp);
      add_search_path(ipath);
    }
    
    path = rtfindlib(name, sodp->sod_major, sodp->sod_minor, &usehints, ipath);


    if (ipath) remove_search_path(ipath);
    

    if (path == NULL) {
      errno = ENOENT;
      return NULL;
    }
  } else {
    if (careful && *name != '/') {
      errno = EACCES;
      return NULL;
    }
    path = name;

  }
  
  /* Check if already loaded */
  for (p = link_map_head; p; p = p->som_next)
    if (p->som_path && strcmp(p->som_path, path) == 0)
      break;
  
  if (p != NULL)
    return p;
  if ((fd = open(path, O_RDONLY, 0)) == -1) {
    if (usehints) {
      usehints = 0;
      goto again;
    }
    return NULL;
  }
  
  if (read(fd, (char *)&hdr, sizeof(hdr)) != sizeof(hdr)) {
    (void)close(fd);
    /*errno = x;*/
    d3printf("Couldn't read enough of the file\n");
    return NULL;
  }
  
  if (N_BADMAG(hdr)) {
    d3printf("Bad Magic # of the header\n");
    (void)close(fd);
    errno = EFTYPE;
    return NULL;
  }
  if ((addr = mmap((char *)next_so_addr, hdr.a_text,
		   PROT_READ,
		   MAP_COPY, fd, 0)) != (caddr_t)next_so_addr) {
    d3printf("Couldn't mmap the text segment the file at next_so_addr\n");
    (void)close(fd);
    return NULL;
  }
  
  next_so_addr += hdr.a_text;
  if ((next_so_addr % NBPG) != 0) {
    next_so_addr += NBPG - (next_so_addr % NBPG);
  }
  
  for (i = 0; i < (hdr.a_text/NBPG); i++) {
    j = sys_self_insert_pte(0,PG_U|PG_W|PG_P, (PGNO(next_so_addr) + i)<<PGSHIFT);
    if (j < 0) {
      d3printf("Failed to insert pte for data segment page :\n");
      d3printf(ui2s((unsigned int)i));
      d3printf(" at address ");
      d3printf(ui2s((unsigned int)((PGNO(next_so_addr) + i)<<PGSHIFT)));
      return NULL;
    }

  }
  if (fseek(fd,(hdr.a_text),SEEK_SET) == -1) {
    d3printf("Could not fseek to the start of the data segment\n");
    (void)close(fd);
    return NULL;
  } 
  slskprintf(" Data segment : \n");
  slskprintd(hdr.a_data);
  slskprintf(" bytes to 0x");
  slskprinth((int)next_so_addr);
  slskprintf("...\n");
  if (read(fd,(char *)next_so_addr,hdr.a_data) != hdr.a_data) {
    d3printf("Could not read the data segment...\n");
    (void)close(fd);
    return NULL;
  }

  next_so_addr += hdr.a_data;
  if ((next_so_addr % NBPG) != 0) {
    next_so_addr += NBPG - (next_so_addr % NBPG);
  }

  /*  for (i = 0; i < (hdr.a_bss/NBPG); i++) {
    j = sys_self_insert_pte(0,PG_U|PG_W|PG_P, PGNO(next_so_addr) + i);
    if (j < 0) {
      d3printf("Failed to insert pte for 0x%x (page %d of data)\n",
	     (PGNO(next_so_addr) + i)<<PGSHIFT,i);
      return NULL;
    }
  } */


  bzero((char *)next_so_addr,hdr.a_bss);
  
  next_so_addr += hdr.a_bss;
  if ((next_so_addr % NBPG) != 0) {
    next_so_addr += NBPG - (next_so_addr % NBPG);
  }
  (void)close(fd);
  /* Assume _DYNAMIC is the first data item */
  dp = (struct _dynamic *)(addr+hdr.a_text);
  
  /* Fixup __DYNAMIC structure */
  (long)dp->d_un.d_sdt += (long)addr;
  return alloc_link_map(path, sodp, smp, addr, dp);
}

void
init_maps(head)
	struct so_map	*head;
{
	struct so_map	*smp;
	d2printf("Entering init_maps\n");
	/* Relocate all loaded objects according to their RRS segments */
	for (smp = head; smp; smp = smp->som_next) {
		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;
		reloc_map(smp);
	}

	/* Copy any relocated initialized data. */
	for (smp = head; smp; smp = smp->som_next) {
		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;
		reloc_copy(smp);
	}

	/* Call any object initialization routines. */
	for (smp = head; smp; smp = smp->som_next) {
		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;
		call_map(smp, ".init");
		call_map(smp, "__init");
	}
}

static inline void
check_text_reloc(r, smp, addr)
	struct relocation_info	*r;
	struct so_map		*smp;
	caddr_t			addr;
{
	char	*sym;

	if (addr >= LM_ETEXT(smp))
		return;

	if (RELOC_EXTERN_P(r))
		sym = LM_STRINGS(smp) +
				LM_SYMBOL(smp, RELOC_SYMBOL(r))->nz_strx;
	else
		sym = "";

	if (ld_warn_non_pure_code && !ld_suppress_warnings) {
	  slskprintf("warning: non pure code in ");
	  slskprintf(smp->som_path);
	  slskprintf(" at ");
	  slskprintf(sym);
	  slskprintf("\n");

	}
	if (smp->som_write == 0 &&
	    mprotect(smp->som_addr + LM_TXTADDR(smp),
		     LD_TEXTSZ(smp->som_dynamic),
		     PROT_READ|PROT_WRITE) == -1) {
	  slskprintf("Cannot enable writes to ");
	  slskprintf(main_progname);
	  slskprintf(":");
	  slskprintf(smp->som_path);
	  slskprintf("\n");
	  exit(1);
	}
	
	smp->som_write = 1;
}

static void
reloc_map(smp)
	struct so_map		*smp;
{
	struct _dynamic		*dp = smp->som_dynamic;
	struct relocation_info	*r = LM_REL(smp);
	struct relocation_info	*rend = r + LD_RELSZ(dp)/sizeof(*r);
	long			symbolbase = (long)LM_SYMBOL(smp, 0);
	char			*stringbase = LM_STRINGS(smp);
	int symsize		= LD_VERSION_NZLIST_P(dp->d_version) ?
					sizeof(struct nzlist) :
					sizeof(struct nlist);
	char *wmsg;

	d2printf("Entering reloc_map\n");

	if (LD_PLTSZ(dp))
		md_fix_jmpslot(LM_PLT(smp),
				(long)LM_PLT(smp), (long)binder_entry);

	for (; r < rend; r++) {
		char	*sym;
		caddr_t	addr = smp->som_addr + r->r_address;

		check_text_reloc(r, smp, addr);

		if (RELOC_EXTERN_P(r)) {
			struct so_map	*src_map = NULL;
			struct nzlist	*p, *np;
			long	relocation = md_get_addend(r, addr);

			if (RELOC_LAZY_P(r))
				continue;

			p = (struct nzlist *)
				(symbolbase + symsize * RELOC_SYMBOL(r));

			if (p->nz_type == (N_SETV + N_EXT))
				src_map = smp;

			sym = stringbase + p->nz_strx;

			np = lookup(sym, &src_map, 0/*XXX-jumpslots!*/);
			if (np == NULL){
			  wmsg = "Undefined symbol _ in _:_\n";
			  wmsg = (char *)malloc(strlen(wmsg) +
						strlen(sym) +
						strlen(main_progname) +
						strlen(smp->som_path));
			  strcpy(wmsg,"Undefined symbol ");
			  strcat(wmsg,sym);
			  strcat(wmsg," in ");
			  strcat(wmsg,main_progname);
			  strcat(wmsg,":");
			  strcat(wmsg,smp->som_path);
			  strcat(wmsg,"\n");
			  perror(wmsg);
			  free(wmsg);
			  exit(1);
			}
			/*
			 * Found symbol definition.
			 * If it's in a link map, adjust value
			 * according to the load address of that map.
			 * Otherwise it's a run-time allocated common
			 * whose value is already up-to-date.
			 */
			relocation += np->nz_value;
			if (src_map)
				relocation += (long)src_map->som_addr;

			if (RELOC_PCREL_P(r))
				relocation -= (long)smp->som_addr;

			if (RELOC_COPY_P(r) && src_map) {
				(void)enter_rts(sym,
					(long)addr,
					N_DATA + N_EXT,
					src_map->som_addr + np->nz_value,
					np->nz_size, src_map);
				continue;
			}
			md_relocate(r, relocation, addr, 0);

		} else {
			md_relocate(r,
#ifdef SUN_COMPAT
				md_get_rt_segment_addend(r, addr)
#else
				md_get_addend(r, addr)
#endif
					+ (long)smp->som_addr, addr, 0);
		}

	}

	if (smp->som_write) {
		if (mprotect(smp->som_addr + LM_TXTADDR(smp),
				LD_TEXTSZ(smp->som_dynamic),
				PROT_READ) == -1) {
		  wmsg = "Cannot diable writes to _:_\n";
		  wmsg = (char *)malloc(strlen(wmsg) +
					strlen(main_progname) +
					strlen(smp->som_path));
		  strcpy(wmsg,"Cannot diable writes to ");
		  strcat(wmsg,main_progname);
		  strcat(wmsg,":");
		  strcat(wmsg,smp->som_path);
		  strcat(wmsg,"\n");
		  perror(wmsg);
		  free(wmsg);
		  exit(1);
		}
		smp->som_write = 0;
	}
}

static void
reloc_copy(smp)
	struct so_map		*smp;
{
	struct rt_symbol	*rtsp;	
	for (rtsp = rt_symbol_head; rtsp; rtsp = rtsp->rt_next)
		if ((rtsp->rt_smp == NULL || rtsp->rt_smp == smp) &&
				rtsp->rt_sp->nz_type == N_DATA + N_EXT) {
			bcopy(rtsp->rt_srcaddr, (caddr_t)rtsp->rt_sp->nz_value,
							rtsp->rt_sp->nz_size);
		}
}

static void
call_map(smp, sym)
	struct so_map		*smp;
	char			*sym;
{
	struct so_map		*src_map = smp;
	struct nzlist		*np;

	np = lookup(sym, &src_map, 1);
	if (np)
		(*(void (*) __P((void)))(src_map->som_addr + np->nz_value))();
}

/*
 * Run-time common symbol table.
 */

#define RTC_TABSIZE		57
static struct rt_symbol 	*rt_symtab[RTC_TABSIZE];

/*
 * Compute hash value for run-time symbol table
 */
static inline int
hash_string(key)
	char *key;
{
	register char *cp;
	register int k;

	cp = key;
	k = 0;
	while (*cp)
		k = (((k << 1) + (k >> 14)) ^ (*cp++)) & 0x3fff;

	return k;
}

/*
 * Lookup KEY in the run-time common symbol table.
 */

static inline struct rt_symbol *
lookup_rts(key)
	char *key;
{
	register int			hashval;
	register struct rt_symbol	*rtsp;

	/* Determine which bucket.  */

	hashval = hash_string(key) % RTC_TABSIZE;

	/* Search the bucket.  */

	for (rtsp = rt_symtab[hashval]; rtsp; rtsp = rtsp->rt_link)
		if (strcmp(key, rtsp->rt_sp->nz_name) == 0)
			return rtsp;

	return NULL;
}

static struct rt_symbol *
enter_rts(name, value, type, srcaddr, size, smp)
	char		*name;
	long		value;
	int		type;
	caddr_t		srcaddr;
	long		size;
	struct so_map	*smp;
{
	register int			hashval;
	register struct rt_symbol	*rtsp, **rpp;

	/* Determine which bucket */
	hashval = hash_string(name) % RTC_TABSIZE;

	/* Find end of bucket */
	for (rpp = &rt_symtab[hashval]; *rpp; rpp = &(*rpp)->rt_link)
		;

	/* Allocate new common symbol */
	rtsp = (struct rt_symbol *)malloc(sizeof(struct rt_symbol));
	rtsp->rt_sp = (struct nzlist *)malloc(sizeof(struct nzlist));
	rtsp->rt_sp->nz_name = strdup(name);
	rtsp->rt_sp->nz_value = value;
	rtsp->rt_sp->nz_type = type;
	rtsp->rt_sp->nz_size = size;
	rtsp->rt_srcaddr = srcaddr;
	rtsp->rt_smp = smp;
	rtsp->rt_link = NULL;

	/* Link onto linear list as well */
	rtsp->rt_next = rt_symbol_head;
	rt_symbol_head = rtsp;

	*rpp = rtsp;

	return rtsp;
}


/*
 * Lookup NAME in the link maps. The link map producing a definition
 * is returned in SRC_MAP. If SRC_MAP is not NULL on entry the search is
 * confined to that map. If STRONG is set, the symbol returned must
 * have a proper type (used by binder()).
 */
static struct nzlist *
lookup(name, src_map, strong)
	char		*name;
	struct so_map	**src_map;	/* IN/OUT */
	int		strong;
{
	long			common_size = 0;
	struct so_map		*smp;
	struct rt_symbol	*rtsp;

	if ((rtsp = lookup_rts(name)) != NULL)
		return rtsp->rt_sp;

	/*
	 * Search all maps for a definition of NAME
	 */
	for (smp = link_map_head; smp; smp = smp->som_next) {
	  int		buckets;
	  long		hashval;
	  struct rrs_hash	*hp;
	  char		*cp;
	  struct	nzlist	*np;
	  
	  /* Some local caching */
	  long		symbolbase;
	  struct rrs_hash	*hashbase;
	  char		*stringbase;
	  int		symsize;
	  
	  if (*src_map && smp != *src_map)
	    continue;
	  
	  if ((buckets = LD_BUCKETS(smp->som_dynamic)) == 0)
	    continue; 
	  
	  if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
	    continue;
	  
	restart:
	  /*
	   * Compute bucket in which the symbol might be found.
	   */
	  for (hashval = 0, cp = name; *cp; cp++)
	    hashval = (hashval << 1) + *cp;
	  
	  hashval = (hashval & 0x7fffffff) % buckets;
	  
	  hashbase = LM_HASH(smp);
	  hp = hashbase + hashval;
	  if (hp->rh_symbolnum == -1)
	    /* Nothing in this bucket */
	    continue;
	  
	  symbolbase = (long)LM_SYMBOL(smp, 0);
	  stringbase = LM_STRINGS(smp);
	  symsize	= LD_VERSION_NZLIST_P(smp->som_dynamic->d_version)?
	    sizeof(struct nzlist) :
	    sizeof(struct nlist);
	  while (hp) {
	    np = (struct nzlist *)
	      (symbolbase + hp->rh_symbolnum * symsize);
	    cp = stringbase + np->nz_strx;
	    if (strcmp(cp, name) == 0)
	      break;
	    if (hp->rh_next == 0)
	      hp = NULL;
	    else
	      hp = hashbase + hp->rh_next;
	  }
	  if (hp == NULL)
	    /* Nothing in this bucket */
	    continue;
	  
	  /*
	   * We have a symbol with the name we're looking for.
	   */
	  if (np->nz_type == N_INDR+N_EXT) {
	    /*
	     * Next symbol gives the aliased name. Restart
	     * search with new name and confine to this map.
	     */
	    name = stringbase + (++np)->nz_strx;
	    *src_map = smp;
	    goto restart;
	  }
	  
	  if (np->nz_value == 0)
	    /* It's not a definition */
	    continue;
	  
	  if (np->nz_type == N_UNDF+N_EXT && np->nz_value != 0) {
	    if (np->nz_other == AUX_FUNC) {
	      /* It's a weak function definition */
	      if (strong)
		continue;
	    } else {
	      /* It's a common, note value and continue search */
	      if (common_size < np->nz_value)
		common_size = np->nz_value;
	      continue;
	    }
	  }
	  
	  *src_map = smp;
	  return np;
	}
	
	if (common_size == 0)
	  /* Not found */
		return NULL;
	
	/*
	 * It's a common, enter into run-time common symbol table.
	 */
	rtsp = enter_rts(name, (long)calloc(1, common_size),
			 N_UNDF + N_EXT, 0, common_size, NULL);
	
	
	return rtsp->rt_sp;
}


/*
 * This routine is called from the jumptable to resolve
 * procedure calls to shared objects.
 */
long
binder(jsp)
	jmpslot_t	*jsp;
{
	struct so_map	*smp, *src_map = NULL;
	long		addr;
	char		*sym;
	struct nzlist	*np;
	int		index;
	char            *wmsg;

	/*
	 * Find the PLT map that contains JSP.
	 */
	for (smp = link_map_head; smp; smp = smp->som_next) {
		if (LM_PLT(smp) < jsp &&
			jsp < LM_PLT(smp) + LD_PLTSZ(smp->som_dynamic)/sizeof(*jsp))
			break;
	}

	if (smp == NULL){
	  perror("Call to binder from unknown location \n");
	  exit(1);
	}

	index = jsp->reloc_index & JMPSLOT_RELOC_MASK;

	/* Get the local symbol this jmpslot refers to */
	sym = LM_STRINGS(smp) +
		LM_SYMBOL(smp,RELOC_SYMBOL(&LM_REL(smp)[index]))->nz_strx;

	np = lookup(sym, &src_map, 1);
	if (np == NULL) {
	  wmsg = "Undefined symbol _ called from _:_\n";
	  wmsg = (char *)malloc(strlen(wmsg) +
				strlen(sym) +
				strlen(main_progname) +
				strlen(smp->som_path));
	  strcpy(wmsg,"Undefined symbol ");
			  strcat(wmsg,sym);
			  strcat(wmsg," called from ");
			  strcat(wmsg,main_progname);
			  strcat(wmsg,":");
			  strcat(wmsg,smp->som_path);
			  strcat(wmsg,"\n");
			  perror(wmsg);
			  free(wmsg);
			  exit(1);
	}
	/* Fixup jmpslot so future calls transfer directly to target */
	addr = np->nz_value;
	if (src_map)
		addr += (long)src_map->som_addr;

	md_fix_jmpslot(jsp, (long)jsp, addr);

	return addr;
}


static int			hfd;
static long			hsize;
static struct hints_header	*hheader;
static struct hints_bucket	*hbuckets;
static char			*hstrtab;
static char			*hint_search_path = "";

#define HINTS_VALID (hheader != NULL && hheader != (struct hints_header *)-1)

static void
maphints()
{
	caddr_t		addr;

	if (1) {
	  hheader = (struct hints_header *)-1;
		return;
	}


	if ((hfd = open(_PATH_LD_HINTS, O_RDONLY, 0)) == -1) {
		hheader = (struct hints_header *)-1;
		return;
	}
	d2printf("mmapping hints \n");
	hsize = PAGSIZ;
	addr = mmap(0, hsize, PROT_READ, MAP_COPY, hfd, 0);
	d2printf("done mmapping hints \n");

	if (addr == (caddr_t)-1) {
		close(hfd);
		hheader = (struct hints_header *)-1;
		return;
	}

	hheader = (struct hints_header *)addr;
	if (HH_BADMAG(*hheader)) {
		munmap(addr, hsize);
		close(hfd);
		hheader = (struct hints_header *)-1;
		return;
	}

	if (hheader->hh_version != LD_HINTS_VERSION_1 &&
	    hheader->hh_version != LD_HINTS_VERSION_2) {
		munmap(addr, hsize);
		close(hfd);
		hheader = (struct hints_header *)-1;
		return;
	}

	if (hheader->hh_ehints > hsize) {
		if (mmap(addr+hsize, hheader->hh_ehints - hsize,
				PROT_READ, MAP_COPY|MAP_FIXED,
				hfd, hsize) != (caddr_t)(addr+hsize)) {

			munmap((caddr_t)hheader, hsize);
			close(hfd);
			hheader = (struct hints_header *)-1;
			return;
		}
	}

	hbuckets = (struct hints_bucket *)(addr + hheader->hh_hashtab);
	hstrtab = (char *)(addr + hheader->hh_strtab);
	if (hheader->hh_version >= LD_HINTS_VERSION_2)
		hint_search_path = hstrtab + hheader->hh_dirlist;
}

static void
unmaphints()
{

	if (HINTS_VALID) {
		munmap((caddr_t)hheader, hsize);
		close(hfd);
		hheader = NULL;
	}
}

int
hinthash(cp, vmajor, vminor)
	char	*cp;
	int	vmajor, vminor;
{
	int	k = 0;

	while (*cp)
		k = (((k << 1) + (k >> 14)) ^ (*cp++)) & 0x3fff;

	k = (((k << 1) + (k >> 14)) ^ (vmajor*257)) & 0x3fff;
	if (hheader->hh_version == LD_HINTS_VERSION_1)
		k = (((k << 1) + (k >> 14)) ^ (vminor*167)) & 0x3fff;

	return k;
}

#undef major
#undef minor

static char *
findhint(name, major, minor, prefered_path)
	char	*name;
	int	major, minor;
	char	*prefered_path;
{
	struct hints_bucket	*bp;

	bp = hbuckets + (hinthash(name, major, minor) % hheader->hh_nbucket);

	while (1) {
		/* Sanity check */
		if (bp->hi_namex >= hheader->hh_strtab_sz) {
		  perror("Bad name index in findhint\n");
		  break;
		}
		if (bp->hi_pathx >= hheader->hh_strtab_sz) {
		  perror("Bad path index in findhint\n");
		  break;
		}

		if (strcmp(name, hstrtab + bp->hi_namex) == 0) {
			/* It's `name', check version numbers */
			if (bp->hi_major == major &&
				(bp->hi_ndewey < 2 || bp->hi_minor >= minor)) {
					if (prefered_path == NULL ||
					    strncmp(prefered_path,
						hstrtab + bp->hi_pathx,
						strlen(prefered_path)) == 0) {
						return hstrtab + bp->hi_pathx;
					}
			}
		}

		if (bp->hi_next == -1)
			break;

		/* Move on to next in bucket */
		bp = &hbuckets[bp->hi_next];
	}

	/* No hints available for name */
	return NULL;
}

static char *
rtfindlib(name, major, minor, usehints, ipath)
	char	*name;
	int	major, minor;
	int	*usehints;
	char	*ipath;
{
	register char	*cp;
	int		realminor;

	d2printf("  rtfinding : ");
	d2printf(name);
	d2printf(".");
	d2printf(ui2s((unsigned int)major));
	d2printf(".");
	d2printf(ui2s((unsigned int)minor));
	d2printf("\n");

	if (hheader == NULL)
		maphints();

	if (!HINTS_VALID || !(*usehints))
		goto lose;

	/* NOTE: `ipath' may reside in a piece of read-only memory */
	if (ld_library_path || ipath) {
		/* Prefer paths from some explicit LD_LIBRARY_PATH */
		register char	*lpath;
		char		*dp;

		dp = lpath = concat(ld_library_path ? ld_library_path : "",
				    (ld_library_path && ipath) ? ":" : "",
				    ipath ? ipath : "");


		while ((cp = strsep(&dp, ":")) != NULL) {
			cp = findhint(name, major, minor, cp);
			if (cp) {
				free(lpath);
				return cp;
			}
		}
		free(lpath);

		/*
		 * Not found in hints; try directory search now, before
		 * we get a spurious hint match below (i.e. a match not
		 * on one of the paths we're supposed to search first.
		 */
		realminor = -1;
		cp = (char *)findshlib(name, &major, &realminor, 0);
		if (cp && realminor >= minor)
			return cp;
	}


	/* No LD_LIBRARY_PATH or lib not found in there; check default */
	cp = findhint(name, major, minor, NULL);
	if (cp)
		return cp;

lose:
	

	/* No hints available for name */
	*usehints = 0;
	realminor = -1;
	add_search_path(hint_search_path);


	cp = (char *)findshlib(name, &major, &realminor, 0);
	remove_search_path(hint_search_path);


	if (cp) {
		if (realminor < minor && !ld_suppress_warnings)
		  perror("Warning: Expected a different minor revision number\n");
		return cp;
	}
	return NULL;
}

void
preload(paths)
	char		*paths;
{
	struct so_map	*nsmp;
	struct sod	*sodp;
	char		*cp, *dp;

	dp = paths = strdup(paths);
	if (dp == NULL) {
	  perror("preload: out of memory");
	  exit(1);
	}

	while ((cp = strsep(&dp, ":")) != NULL) {
	  if ((sodp = (struct sod *)malloc(sizeof(struct sod))) == NULL) {
	    perror("preload: out of memory\n");
	    perror(cp);
	    exit(1);
	    return;
	  }
	  
	  sodp->sod_name = (long)strdup(cp);
	  sodp->sod_library = 0;
	  sodp->sod_major = sodp->sod_minor = 0;
	  
	  if ((nsmp = map_object(sodp, 0)) == NULL) {
	    perror("preload : cannon map object\n");
	    perror(cp);
	    exit(1);
	  }
		LM_PRIVATE(nsmp)->spd_refcount++;
	}
	free(paths);
	return;
}

/*  It doesn't look like these structures are ever used...hmm...

static struct somap_private dlmap_private = {
		0,
		(struct so_map *)0,
		0,
#ifdef SUN_COMPAT
		0,
#endif
};

static struct so_map dlmap = {
	(caddr_t)0,
	"internal",
	(struct so_map *)0,
	(struct sod *)0,
	(caddr_t)0,
	(u_int)0,
	(struct _dynamic *)0,
	(caddr_t)&dlmap_private
}; */
static int dlerrno;

/*
 * Populate sod struct for dlopen's call to map_obj
 */
void
build_sod(name, sodp)
	char		*name;
	struct sod	*sodp;
{
	unsigned int	tuplet;
	int		major, minor;
	char		*realname, *tok, *etok, *cp;

	/* default is an absolute or relative path */
	sodp->sod_name = (long)strdup(name);    /* strtok is destructive */
	sodp->sod_library = 0;
	sodp->sod_major = sodp->sod_minor = 0;

	realname = 0;

	/* asking for lookup? */
	if (strncmp((char *)sodp->sod_name, "lib", 3) != 0)
		return;

	/* skip over 'lib' */
	cp = (char *)sodp->sod_name + 3;

	/* dot guardian */
	if ((strchr(cp, '.') == NULL) || (*(cp+strlen(cp)-1) == '.'))
		return;

	/* default */
	major = minor = -1;

	/* loop through name - parse skipping name */
	for (tuplet = 0; (tok = strsep(&cp, ".")) != NULL; tuplet++) {
		switch (tuplet) {
		case 0:
			/* removed 'lib' and extensions from name */
			realname = tok;
			break;
		case 1:
			/* 'so' extension */
			if (strcmp(tok, "so") != 0)
				goto backout;
			break;
		case 2:
			/* major version extension */
			major = strtol(tok, &etok, 10);
			if (*tok == '\0' || *etok != '\0')
				goto backout;
			break;
		case 3:
			/* minor version extension */
			minor = strtol(tok, &etok, 10);
			if (*tok == '\0' || *etok != '\0')
				goto backout;
			break;
		/* if we get here, it must be weird */
		default:
			goto backout;
		}
	}
	cp = (char *)sodp->sod_name;
	sodp->sod_name = (long)strdup(realname);
	free(cp);
	sodp->sod_library = 1;
	sodp->sod_major = major;
	sodp->sod_minor = minor;
	return;

backout:
	free((char *)sodp->sod_name);
	sodp->sod_name = (long)strdup(name);
}

static void *
__dlopen(name, mode)
	char	*name;
	int	mode;
{
	struct sod	*sodp;
	struct so_map	*smp;

	/*
	 * A NULL argument returns the current set of mapped objects.
	 */
	if (name == NULL) {
		LM_PRIVATE(link_map_head)->spd_refcount++;
		return link_map_head;
	}

	if ((sodp = (struct sod *)malloc(sizeof(struct sod))) == NULL) {
		dlerrno = ENOMEM;
		return NULL;
	}

	build_sod(name, sodp);

	if ((smp = map_object(sodp, 0)) == NULL) {
		dlerrno = errno;
		return NULL;
	}

	if (LM_PRIVATE(smp)->spd_refcount++ > 0)
		return smp;

	if (load_subs(smp) != 0)
		return NULL;

	LM_PRIVATE(smp)->spd_flags |= RTLD_DL;
	init_maps(smp);
	return smp;
}

static int
__dlclose(fd)
	void	*fd;
{
	struct so_map	*smp = (struct so_map *)fd;

	if (--LM_PRIVATE(smp)->spd_refcount != 0)
		return 0;

	/* Dismantle shared object map and descriptor */
	call_map(smp, "__fini");
#if 0
	unmap_object(smp);
	free(smp->som_sod->sod_name);
	free(smp->som_sod);
	free(smp);
#endif

	return 0;
}

static void *
__dlsym(fd, sym)
	void	*fd;
	char	*sym;
{
	struct so_map	*smp = (struct so_map *)fd, *src_map = NULL;
	struct nzlist	*np;
	long		addr;

	/*
	 * Restrict search to passed map if dlopen()ed.
	 */
	if (LM_PRIVATE(smp)->spd_flags & RTLD_DL)
		src_map = smp;

	np = lookup(sym, &src_map, 1);
	if (np == NULL)
		return NULL;

	/* Fixup jmpslot so future calls transfer directly to target */
	addr = np->nz_value;
	if (src_map)
		addr += (long)src_map->som_addr;

	return (void *)addr;
}

static int
__dlctl(fd, cmd, arg)
	void	*fd, *arg;
	int	cmd;
{
	switch (cmd) {
	case DL_GETERRNO:
		*(int *)arg = dlerrno;
		return 0;
	default:
		dlerrno = EOPNOTSUPP;
		return -1;
	}
	return 0;
}

static void
__dlexit()
{
	struct so_map	*smp;

	/* Call any object initialization routines. */
	for (smp = link_map_head; smp; smp = smp->som_next) {
		if (LM_PRIVATE(smp)->spd_flags & RTLD_RTLD)
			continue;
		call_map(smp, ".fini");
	}
}

/* void
   xprintf(char *fmt, ...)
   {
   char buf[256];
   va_list	ap;
   
   va_start(ap, fmt);
   vsprintf(buf, fmt, ap);
   (void)write(1, buf, strlen(buf));
   va_end(ap);
   }
   
   */
