
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
#include <a.out.h>
#include <aouthdr.h>
#include <filehdr.h>
#include <scnhdr.h>
#include <stdio.h>
#include <string.h>
#include <sys/defs.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <xuser.h>
#include <sys/mmu.h>
#include <os/osdecl.h>
#include "../../ffs/ffs.h"  /* XXX */
#include <sys/pmap.h>
#include <sys/param.h>
#include <exos/critical.h>

#include <unistd.h>
#include <errno.h>
#include <exos/process.h>
#ifdef SYSV_SHM
#include <sys/shm.h>
#endif

#include <assert.h>
#include <exos/vm.h>

#include <fd/fdstat.h>

#ifdef PRINTFDSTAT
extern void (*pr_fd_statp)(void);
#define proprintf if (pr_fd_statp) printf
#else
#define proprintf if (0) printf
#endif


#undef UNIX_NR_ARGV
//#define UNIX_NR_ARGV 120

extern char **environ;

/* local function declaration */
int map_section(int k, int fd, SCNHDR *shdr, int envid);

static void print_argv(int argc,char **argv) {
  int i;

  fprintf(stderr,"argc: %d\n",argc);
  for (i = 0; i < argc; i++) {
    fprintf(stderr,"argv[%d] = %08x \"%s\"\n",i,(int)argv[i],argv[i]);
  }
  fprintf(stderr,"argv[argc] is %sequal to NULL\n",
	  (argv[argc] == 0) ? "" : "not ");
}

void print_physical(int page) {
  Pte *ptep;

  ptep = &vpt[page >> PGSHIFT];
  fprintf(stderr,"%8d %08x ",(*ptep & ~PGMASK) >> PGSHIFT,*ptep);
}

/* everything is based on this exec, we have an extra argument
   (execonly) to differentiate between the exec and fork_exec
   families.  the difference is that the latter forks and then execs
   the process thereby returning in the parent
   */
static int
fork_execve0(const char *path, char *const argv_ori[], char * const envptr[], 
	     int execonly) {
  int envid;
  int fd;

  u_int k = 0;
  struct Uenv cu;
  int argc;
  char **argv_tmp,*argv_p,**argv;
  u32 lmagic;
  char *cmagic = (char*)&lmagic;
  u32 entry_point = 0;
  int running_emulator = 0;
  char *emu_path = NULL;
  char **argv_ori_tmp = (char**)argv_ori;
  char **argv_ori_tmp2;
#define FE_PATH (running_emulator ? emu_path : path)
#ifdef PROCESS_TABLE
  int NewPid = 0;
#endif
  char *extra_argv_space = NULL;
#if 0
  {
    extern void pr_fds();
    fprintf(stderr,"FDS BEFORE EXEC PID: %d execonly: %d\n",getpid(),execonly);
    pr_fds;
  }
#endif
//  printf("fork_execve0: path: %s\n",path);
#if 0
  for(argc = 0; argv_ori[argc] ; argc++) 
    kprintf("%d) %s\n",argc,argv_ori[argc]);
#endif

  proprintf("allocate env, and open file\n");
  ISTART(misc,execve);
  ISTART(misc,step1);
  /* fprintf(stderr,"fe_nfs2 %s\n",FE_PATH); */
  if (! (envid = sys_env_alloc (0))) {
    fprintf(stderr,"could not sys_env_alloc\n");
    goto err;
  }

open_binary:
  /* verify executable permission */
  if (access(FE_PATH, X_OK) == -1) {
    /* access will set errno */
    goto fork_execve0_end_error;
  }
  /* open the executable */
  fd = open (FE_PATH, O_RDONLY, 0644);
  if (fd < 0) {
    /* open will set errno */
    /*fprintf(stderr,"1could not open path %s, errno: %d\n",FE_PATH,errno);*/
    goto fork_execve0_end_error;
  }
  STOPP(misc,step1);
  proprintf("read file and open interpreter if necessary\n");
  ISTART(misc,step2);

  /* read in the magic number */
  if (read(fd, cmagic, 2) < 2) {
    errno = ENOEXEC;
    goto fork_execve0_end_error_closefd;
  }

  /* check for interpreter */
  if (cmagic[0] == '#' && cmagic[1] == '!') {
    int intersize;
    char inter[MAXINTERP+1], *interp;

    if ((intersize = read(fd,inter,MAXINTERP)) == -1) {
      errno = ENOEXEC;
      goto fork_execve0_end_error_closefd;
    }
    inter[intersize] = '\n';
    interp = inter;
    
    /* skip spaces */
    while ((*interp == ' ' || *interp == '\t') && 
	   *interp != '\n')
      interp++;
    
    {
      char **v = (char **)argv_ori_tmp;
      while(*v++);
      extra_argv_space = (char *)malloc((char *)v - (char *)argv_ori_tmp + 
					MAXINTERP*(sizeof(char*)));
      if (!extra_argv_space) {
	fprintf(stderr,"execve could not allocate extra argv space for "
		"interpreted file\n");
	goto fork_execve0_end_error_closefd;
      }
    }
    argv = (char **)extra_argv_space;
    *argv++ = interp;

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

      *argv++ = interp;
      while (*interp != ' ' && *interp != '\t' && 
	     *interp != (char)0 && *interp != '\n')
	interp++;
      *interp = (char)0;
    } else {
      *interp = (char)0;
    }
    *argv++ = (char *)FE_PATH;
    argv_ori_tmp2 = argv_ori_tmp;
    argv_ori_tmp2++;
    while(*argv_ori_tmp2 != (char *) 0) {*argv++ = *argv_ori_tmp2++;}
    /* copy the 0 */
    *argv++ = *argv_ori_tmp2++;
    argv = (char **)extra_argv_space;
  
    close(fd);
    /* verify executable permission */
    if (access(argv[0], X_OK) == -1) {
      /* access will set errno */
      goto fork_execve0_end_error;
    }
    fd = open (argv[0], O_RDONLY, 0644);
    if (fd < 0) {
      /* open will set errno */
      /*fprintf(stderr,"2could not open path %s, errno: %d\n",argv[0],errno);*/
      goto fork_execve0_end_error;
    }
    /* read in the magic number (and nesting of interpreters not allowed) */
    if (read(fd, cmagic, 2) < 2 || (cmagic[0] == '#' && cmagic[1] == '!')) {
      errno = ENOEXEC;
      goto fork_execve0_end_error_closefd;
    }
  } else {
    argv = (char **)argv_ori_tmp;
  }

  STOPP(misc,step2);
  proprintf("read more magic and executable headers, check for emulation\n");
  ISTART(misc,step3);

  if (read(fd, &cmagic[2], 2) < 2) {
    errno = ENOEXEC;
    goto fork_execve0_end_error_closefd;
  }

  /* see whether we need to run an emulator */
  if (lmagic != 0700303000 && (lmagic & 0x0000ffff) != 0514) {
    if (running_emulator) {
      fprintf(stderr,"Emulator binary is in unrecognized executable "
	      "format.\n");
      goto fork_execve0_end_error_closefd;
    }
    else
      fprintf(stderr,"Unrecognized executable format; attempting to run " 
	      "emulator.\n");
    close(fd);
    {
      char **v = (char **)argv_ori_tmp;
      while(*v++);
      extra_argv_space = (char *)malloc((char *)v - (char *)argv_ori_tmp +
					2 * sizeof(char*));
      if (!extra_argv_space) {
	fprintf(stderr,"execve could not allocate extra argv space for "
		"copying args for emulator\n");
	goto fork_execve0_end_error_closefd;
      }
    }
    argv = (char **)extra_argv_space;
    running_emulator = 1;
    *argv++ = emu_path = getenv("EMULATOR");
    if (!FE_PATH) {
      fprintf(stderr,"EMULATOR environment variable not set to emulator path; "
	      "cannot run emulator.\n");
      goto fork_execve0_end_error;
    }
    *argv++ = (char *)path;
    argv_ori_tmp2 = argv_ori_tmp;
    argv_ori_tmp2++;
    while(*argv_ori_tmp2 != (char *) 0) {*argv++ = *argv_ori_tmp2++;}
    /* copy the 0 */
    *argv++ = *argv_ori_tmp2++;
    argv = (char **)extra_argv_space;
    argv_ori_tmp = argv;
    goto open_binary;
  }  

  /* check for original ExOS (OpenBSD) format - remove eventually */
  if (lmagic == 0700303000)
    {
      struct exec hdr;
      u_int byte_count, page_count;
      Pte *ptes;
      int i;

#if 0
      fprintf(stderr,"File uses old executable format.\n");
#endif
      /* allocate physical pages and read data into it. */

      /* read a.out headers */
      if (lseek(fd,0,SEEK_SET) == -1 ||
	  read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
	fprintf(stderr,"Invalid executable format.\n");
	errno = ENOEXEC;
	goto fork_execve0_end_error_closefd;
      }
      entry_point = hdr.a_entry;

      /* alloc this much memory */
      byte_count = hdr.a_text + hdr.a_data + hdr.a_bss;
      page_count = PGNO(PGROUNDUP(byte_count));

      /* allocate it in both child and parent areas */
      if ((ptes = malloc(sizeof(Pte) * page_count)) == 0) {
	errno = ENOEXEC;
	goto fork_execve0_end_error_closefd;
      }
      for (i=0; i < page_count; i++)
	ptes[i] = PG_U|PG_W|PG_P;
      STOPP(misc,step3);
      proprintf("allocate childs vm in both parent and child, read text+data "
		"(sz %ld)\n", hdr.a_text + hdr.a_data);
      ISTART(misc,read0);
      if (sys_self_insert_pte_range(k, ptes, page_count, TEMP_REGION) < 0 ||
	  sys_insert_pte_range(k, &vpt[PGNO(TEMP_REGION)], page_count, 
			       UTEXT, k, envid) < 0 ||
	  read(fd, (void*)TEMP_REGION, hdr.a_text + hdr.a_data) != 
	  hdr.a_text + hdr.a_data) {
	fprintf (stderr,"Binary file invalid or corrupt.\n");
	for (i=0; i < page_count; i++)
	  ptes[i] = 0;
	sys_self_insert_pte_range(k, ptes, page_count, TEMP_REGION);
	sys_insert_pte_range(k, ptes, page_count, UTEXT, k, envid);
	free(ptes);
	errno = ENOEXEC;
	goto fork_execve0_end_error_closefd;
      }

      STOPP(misc,read0);
      /* zero the bss */
      proprintf("bzero bss  (sz %ld)\n",hdr.a_bss);
      ISTART(misc,read1);
      bzero((void*)(TEMP_REGION + hdr.a_text + hdr.a_data), hdr.a_bss);

      STOP(misc,read1);
      PRNAME(misc,read1);
      proprintf("unmap TEMP_REGION (sz %d pages)\n", page_count);
      ISTART(misc,step4);
      /* unmap the used TEMP_REGION */
      for (i=0; i < page_count; i++)
	ptes[i] = 0;
      sys_self_insert_pte_range(k, ptes, page_count, TEMP_REGION);
      free(ptes);
    }
  else if ((lmagic & 0x0000ffff) == 0514) /* check for ExOS COFF format */
    {
      FILHDR hdr;
      AOUTHDR opthdr;
      SCNHDR shdr;
      int s;

      /* allocate physical pages and read data into it. */
      /* read file headers */
      if (lseek(fd,0,SEEK_SET) == -1 ||
	  read(fd, &hdr, FILHSZ) != FILHSZ ||
	  hdr.f_opthdr != sizeof(opthdr) ||
	  !(hdr.f_flags & F_EXEC) ||
	  read(fd, &opthdr, hdr.f_opthdr) != hdr.f_opthdr) {
	fprintf(stderr,"Invalid executable format.\n");
	errno = ENOEXEC;
	goto fork_execve0_end_error_closefd;
      }
      entry_point = opthdr.entry;

      /* ensure ZMAGIC */
      if (opthdr.magic != ZMAGIC) {
	fprintf(stderr,"Exec cannot read non-ZMAGIC COFF executables.\n");
	errno = ENOEXEC;
	goto fork_execve0_end_error_closefd;
      }

      STOPP(misc,step3);
      proprintf("read in and map/zero COFF sections then unmap\n");
      ISTART(misc,read0);
      for (s=0; s < hdr.f_nscns; s++)
	if (read(fd, &shdr, SCNHSZ) != SCNHSZ ||
	    map_section(k, fd, &shdr, envid) == -1) {
	  fprintf(stderr,"Invalid executable format.\n");
	  errno = ENOEXEC;
	  goto fork_execve0_end_error_closefd;
	}
      STOPP(misc,read0);
      proprintf("nothing\n");
      ISTART(misc,read1);
      STOP(misc,read1);
      PRNAME(misc,read1);
      proprintf("nothing\n");
      ISTART(misc,step4);
    } else {
      fprintf(stderr, "Unknown file format.\n");
      errno = ENOEXEC;
      goto fork_execve0_end_error_closefd;
    }

  STOPP(misc,step4);

  close (fd);

  proprintf("Allocate stack for child\n");
  ISTART(misc,step5);

  /* allocate stack space for child */
  if (sys_insert_pte (k, PG_U|PG_W|PG_P, USTACKTOP-NBPG, k, envid) < 0) {
    fprintf(stderr,"sys_insert_pte failed\n");
    errno = ENOEXEC;
    goto fork_execve0_end_error;
  }

  STOPP(misc,step5);
  proprintf("ExecuteOnExecHandlers\n");
  ISTART(misc,step6);
  if (ExecuteOnExecHandlers(k,envid,execonly) == -1) {
    fprintf(stderr,"cleanup code not done yet\n");
    assert(-1);
  }
  STOPP(misc,step6);
  proprintf("Process table stuff\n");
  ISTART(misc,step7);
#ifdef PROCESS_TABLE
  if (execonly) {
    /* because true exec */
    NewPid = getpid();
    /* XXX -- this locking is ... up. I should really come up with a better
       convention as to what expects to be called with things locked and
       what doesn't */
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
#endif  

#ifdef PROCESS_TABLE
  cu = u;
  if (!execonly) {
    AddProcEntry (&cu, (char *)FE_PATH, (char **)argv, NewPid, UAREA.pid);
    if ((cu.parent_slot = GetChildSlot (NewPid)) < 0) {
      errno = ENOEXEC;
      goto fork_execve0_end_error;
    }
  } else {
    /* TO TOM: what do we do this for?  */
    strncpy (UAREA.name, argv[0], U_NAMEMAX-1);
    UAREA.name[U_NAMEMAX-1] = '\0';
  }
  /* XXX -- on an exec we'll forget to unref our children's pids */
  /* TO TOM: shouldnt this clearchildinfo be at the top */
  ClearChildInfo (&cu);
  strncpy (cu.name, FE_PATH, U_NAMEMAX-1);
  cu.name[U_NAMEMAX-1] = '\0';
  cu.u_chld_state_chng = 0;
#endif /* PROCESS_TABLE */
  cu.u_in_critical = 0;
  cu.u_status = U_RUN;
  cu.u_entprologue = entry_point;
  cu.u_next_timeout = 0;
  cu.u_in_pfault = 0;
  cu.u_revoked_pages = 0;
  cu.u_donate = -1;

  STOPP(misc,step7);
  proprintf("Argv and Environment copying\n");
  ISTART(misc,step8);


//  printf("ENTERING ARGV COPY\n");
  /* ----------------------------------------------------------------------------- */
  /* allocate argv space for child */
  {
    char *buf = (char *)ARGV_START_LOCAL;
    char **bufv = (char **)ARGV_START_LOCAL;
    int len,i;

    argc = 0;
    while (argv[argc] != 0) {
      //printf("argv[%d] = %p\n",argc,argv[argc]);
      argc++;}
    
//    printf("argv: %p, argc: %d\n",argv,argc);

    /* allocate pages */
    {
      Pte *ptes;

      if ((ptes = malloc(sizeof(Pte) * NRARGVPG)) == 0) {
	return -1;
      }
      for (i=0; i < NRARGVPG; i++)
	ptes[i] = PG_U|PG_W|PG_P;
      
      if (sys_self_insert_pte_range(k, ptes, NRARGVPG, ARGV_START_LOCAL) < 0 ||
	  sys_insert_pte_range(k, &vpt[PGNO(ARGV_START_LOCAL)], NRARGVPG, 
			       ARGV_START, k, envid) < 0) {
	fprintf(stderr,"sys_insert_pte failed\n");
	for (i=0; i < NRARGVPG; i++)
	  ptes[i] = 0;
	sys_self_insert_pte_range(k, ptes, NRARGVPG, ARGV_START_LOCAL);
	sys_insert_pte_range(k, ptes, NRARGVPG, ARGV_START, k, envid);
	free(ptes);
	errno = ENOEXEC;
	goto fork_execve0_end_error_closefd;
      }
      free(ptes);
    }

    /* copy the args */
    buf += (argc + 1) * sizeof(char *);
    for (i = 0; i < argc; i++) {
//      fprintf(stderr,"argv[%d] ",i);
      len = strlen(argv[i]) + 1;
//      fprintf(stderr,"length %d\n",len);
      
      if ((int)(buf + len) > ARGV_START_LOCAL + NRARGVPG*NBPG) {
	kprintf("Argv too large truncating\n");
	break;
      }

      bufv[i] = buf - (ARGV_START_LOCAL - ARGV_START);
//      fprintf(stderr,"copied argument %d: %p %s (len %d) to %p bufv: %p\n",i,argv[i],argv[i],len,buf,bufv);
      memcpy(buf, argv[i],len);
      buf += len;
    }
    bufv[argc] = (char *)0;

    
  }
//  printf("DONE ARGV COPY\n");

  /* ----------------------------------------------------------------------------- */

#if 0
  /* COPY ARGUMENTS */
  argc = 0;
  iptr = (int *)ARGV_START_LOCAL;
  argv_p = (char *)(ARGV_START_LOCAL + NBPG);
  while(*argv != (char *)0) {
    strcpy(argv_p,*argv);
    iptr[argc] = strlen(*argv) + 1;
    argv_p += strlen(*argv) + 1;
    /*     fprintf(stderr,"%d len %d \"%s\"  ",argc,cu.u_argv_lengths[argc],*argv); */
    argc++;
    argv++;
  }
  iptr[argc] = -1;
  /*   fprintf(stderr,"ARGC %d\n",argc); */
#endif
#if 0
  argc = 0;
  argv_p = (char *)&cu.u_argv_space;
  while(*argv != (char *)0) {
    strcpy(argv_p,*argv);
    cu.u_argv_lengths[argc] = strlen(*argv) + 1;
    argv_p += strlen(*argv) + 1;
    /*     fprintf(stderr,"%d len %d \"%s\"  ",argc,cu.u_argv_lengths[argc],*argv); */
    argc++;
    argv++;
    if (argc == (UNIX_NR_ARGV - 1)) {
      fprintf(stderr,"argc (%d) is greater than maximum allowed (%d), truncating.\n",
	     argc,UNIX_NR_ARGV - 1);
      break;
    }
    if ((int)argv_p > (int)&cu.u_argv_space[0] + UNIX_ARGV_SIZE) {
      fprintf(stderr,"too much data in argv (%d) max is %d\n",
	     (int)argv_p - (int)&cu.u_argv_space[0],UNIX_ARGV_SIZE);
      break;
    }
  }
  cu.u_argv_lengths[argc] = -1;
#endif

  /* COPY ENVIRONMENT */
  argc = 0;
  (char * const *)argv_tmp = envptr;
  argv_p = (char *)&cu.u_env_space;
  if (argv_tmp)
  while(*argv_tmp != (char *)0) {
    strcpy(argv_p,*argv_tmp);
    cu.u_env_lengths[argc] = strlen(*argv_tmp) + 1;
    argv_p += strlen(*argv_tmp) + 1;
    /* fprintf(stderr,"%d len %d \"%s\"  ",argc,cu.u_env_lengths[argc],*argv_tmp); */
    argc++;
    argv_tmp++;
    if (argc == (UNIX_NR_ENV - 1)) {
      fprintf(stderr,"envc (%d) is greater than maximum allowed (%d), truncating.\n",
	     argc,UNIX_NR_ENV - 1);
      break;
    }
    if ((int)argv_p > (int)&cu.u_env_space[0] + UNIX_ENV_SIZE) {
      fprintf(stderr,"too much data in envp (%d) max is %d (%s)\n",
	     (int)argv_p - (int)&cu.u_env_space[0],UNIX_ENV_SIZE, __FILE__);
      break;
    }
  }
  cu.u_env_lengths[argc] = -1;
  /* fprintf(stderr,"ENVC %d\n",argc); */

  if (sys_wru (0, envid, &cu) < 0) {
    fprintf (stderr,"sys_wru failed\n");
    errno = ENOEXEC;
    goto fork_execve0_end_error;
  }
  STOPP(misc,step8);

  STOP(misc,execve);
  PRNAME(misc,execve);
  {
    extern void pr_fd_stat(void);
    pr_fd_stat();
  }

  /*fprintf(stderr,"allocating quantum\n");*/
  if (sys_quantum_alloc (k, -1, 0, envid) < 0) {
    fprintf (stderr,"could not alloc quantum\n");
    errno = ENOEXEC;
    goto fork_execve0_end_error;
  }

  if (execonly) {
    ProcessFreeQuanta(__envid);
    sys_env_free (0, __envid);
  }

  return (NewPid);

fork_execve0_end_error_closefd:
  close(fd);
fork_execve0_end_error:
  ProcessFreeQuanta(__envid);
  sys_env_free (k, envid);
err:
  if (extra_argv_space != NULL) free(extra_argv_space);
  return -1;
}

int
fork_execve(const char *path, char *const argv[], char *envptr[]) {
  return fork_execve0(path,  argv, envptr,0);
}

int
execve(const char *path, char *const argv[], char * const envptr[]) {
  return fork_execve0(path,  argv, envptr,1);
}


int
fork_and_exec(const char *path, char * argv[]) {
  return fork_execve0(path,argv,environ,0);
}


int
fork_execv(const char *file, char *const argv[]) {
  return fork_execve0(file,argv,environ,0);
}
int
execv(const char *file, char *const argv[]) {
  return fork_execve0(file,argv,environ,1);
}

int
fork_execvp0(const char *file, char *const argv[], int execonly) {
  if (strchr (file, '/') == NULL)
    {
      char *path, *p;
      struct stat st;
      size_t len;
      uid_t uid;
      gid_t gid;
      int ngroups;
#define NGROUPS_MAX 32		/* need to define this in an include file */
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
 
  return fork_execve0(file,argv,environ,execonly);
}

int
fork_execvp(const char *file, char *const argv[]) {
  return fork_execvp0(file, argv, 0);
}

int
execvp(const char *file, char *const argv[]) {
  return fork_execvp0(file, argv, 1);
}


int
fork_execl(const char *path, const char *arg, ...) {
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
 
  return fork_execve0 (path, (char **) argv, environ,0);
}

int
execl(const char *path, const char *arg, ...) {
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
 
  return fork_execve0 (path, (char **) argv, environ,1);
}

int
fork_execlp(const char *file, ...) {
  const char *argv[1024];
  unsigned int i;
  va_list args;
 
  va_start (args, file);
 
  i = 0;
  do
    argv[i] = va_arg (args, const char *);
  while (argv[i++] != NULL);
 
  va_end (args);
 
  return fork_execvp (file, (char **) argv);
}

int
execlp(const char *file, const char *arg, ...) {
  const char *argv[1024];
  unsigned int i;
  va_list args;
 
  va_start (args, arg);
 
  i = 0;
  argv[i++] = arg;
  do
    argv[i] = va_arg (args, const char *);
  while (argv[i++] != NULL);
 
  va_end (args);
 
  return execvp (file, (char **) argv);
}

int
fork_execle(const char *path, const char *arg, ...) {
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
 
  return fork_execve0(path, (char *const *) argv, (char **)envp,0);
}

int
execle(const char *path, const char *arg, ...) {
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
 
  return fork_execve0(path, (char *const *) argv, (char **)envp,1);
}


int
fork_exect(const char *path, char *const argv[], char *const envp[]) {
  return fork_execve0(path, argv, (char **)envp,0);
}

int
exect(const char *path, char *const argv[], char *const envp[]) {
  return fork_execve0(path,argv, (char **)envp,1);
}

#define MS_TEXT    0
#define MS_DATA    1
#define MS_BSS     2
#define MS_UNKNOWN 3
int
map_section(int k, int fd, SCNHDR *shdr, int envid) {
  u_int page_count;
  Pte *ptes;
  int i, retval = 0, type;
  off_t curloc = lseek(fd, 0, SEEK_CUR);
  u32 start, zero_start, len;

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
  if ((ptes = malloc(sizeof(Pte) * page_count)) == 0) {
    return -1;
  }
  for (i=0; i < page_count; i++)
    ptes[i] = PG_U|PG_W|PG_P;
  
  if (sys_self_insert_pte_range(k, ptes, page_count, TEMP_REGION) < 0 ||
      sys_insert_pte_range(k, &vpt[PGNO(TEMP_REGION)], page_count, 
			   start, k, envid) < 0 ||
      (type != MS_BSS &&
       (lseek(fd, shdr->s_scnptr, SEEK_SET) != shdr->s_scnptr ||
	read(fd, (void*)TEMP_REGION, shdr->s_size) != shdr->s_size ||
	lseek(fd, curloc, SEEK_SET) != curloc))) {
    retval = -1;
  }
  if (type == MS_BSS) {
    zero_start = TEMP_REGION;
    len = page_count * NBPG;
  } else {
    zero_start = TEMP_REGION + shdr->s_size;
    len = NBPG - (zero_start & PGMASK);
  }
  bzero((void*)zero_start, len);
  if (type == MS_TEXT)
    mprotect((void*)TEMP_REGION, page_count*NBPG, PROT_READ);
  for (i=0; i < page_count; i++)
    ptes[i] = 0;
  sys_self_insert_pte_range(k, ptes, page_count, TEMP_REGION);
  if (retval == -1)
    sys_insert_pte_range(k, ptes, page_count, start, k, envid);
  return retval;
}
