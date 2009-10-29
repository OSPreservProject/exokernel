#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <exos/nameserv.h>
#include <exos/sls.h>
#include <exos/mallocs.h>
#include <exos/conf.h>

#include <exos/vm-layout.h>
#include <aouthdr.h>
#include <filehdr.h>
#include <scnhdr.h>
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
#include <xok/pmap.h>
#include <sys/param.h>
#include <exos/critical.h>
#include <exos/callcount.h>
#include <exos/mallocs.h>
#include <xok/ash.h>

#include <exos/process.h>
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

static u_int slseid = 0;

static int
S_OPEN(const char *name, int flags, mode_t mode) {
  char nname[_POSIX_PATH_MAX];
  int ret, ipcret;

  if (!slseid) slseid = nameserver_lookup(NS_NM_SLS);
  assert(slseid);
  strcpy(nname, name);
  ret = _ipcout_default(slseid, 5, &ipcret, IPC_SLS, SLS_OPEN, name, flags,
			mode);
  assert(ret == 0);
  return ipcret;
}

/* XXX */
struct _exos_exec_args {
  u_int sls_envid;
  int sls_fd;
  struct ps_strings pss;
};

static int setup_new_stack(const char *path, char *const argv[], char *const env[],
			   u_int envid, u_int *newesp) {
  int len=0, envsize=0, argsize=0, argc=0, envc=0, pages_needed, i;
  void *pages;
  u_int p;
  struct ps_strings *ps;
  struct _exos_exec_args *eea;
  char **argvstr, **envstr;

  /* XXX - sanity check ponters */
  if (!argv || !env) return -EFAULT;

  /* figure out how much we'll be putting on the new stack */
  len += sizeof(struct _exos_exec_args);
  len = ALIGN(len);
  /* size of new args */
  while (argv[argc]) argsize += strlen(argv[argc++])+1;
  /* size of new environment */
  while (env[envc]) envsize += strlen(env[envc++])+1;
  len += envsize + argsize;
  len = ALIGN(len);
  /* for the pointers to the args & envs and their null termination */
  len += (envc + 1 + argc + 1) * sizeof(void*);
  len += sizeof(int); /* the argc */
  /* extra page so child has at least one totally free page */
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
  p = ALIGN(p);
  eea = (struct _exos_exec_args*)p;
kprintf("<<%p %x>>", eea, sizeof(*eea));
  ps = &eea->pss;
  ps->ps_argvstr = (char**)(USTACKTOP - ((u_int)pages + pages_needed*NBPG -
					 (u_int)argvstr));
  ps->ps_nargvstr = argc;
  ps->ps_envstr = (char**)(USTACKTOP - ((u_int)pages + pages_needed*NBPG -
					(u_int)envstr));
  ps->ps_nenvstr = envc;


sys_cputs("<a1>");
  assert((eea->sls_fd = S_OPEN(path, O_RDONLY, 0)) >= 0);
  eea->sls_envid = slseid;


  /* now map the pages into the new process */
  /* XXX - touch clean page so it'll be mapped */
  *(char*)pages = 0;
sys_cputs("<b>");
  if (__vm_share_region((u_int)pages, pages_needed*NBPG, 0, 0, envid,
			USTACKTOP-pages_needed*NBPG) == -1 ||
      __vm_free_region((u_int)pages, pages_needed*NBPG, 0) == -1)
    return -ENOMEM;
  __free(pages);
  *newesp = USTACKTOP - len;

  return 0;
}

static int __zero_segment (int envid, u_int start, u_int sz) {
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
  
int
exec_elt(int fd, const char *path, char *const argv[],
	 char *const envp[], u_int envid, struct Env *e)
{
  struct exec hdr, lhdr;
  int r;
  u_int text_size, data_size, bss_size, overlap_size;
  u_int start_text_addr;
  u_int dynamic, start_text_pg;

  if (strcmp(path, "/bin/ls2")) return 0;

  if (lseek(fd, 0, SEEK_SET) == -1 ||
      read(fd, &hdr, sizeof(hdr)) != sizeof(hdr) ||
      lseek(fd, sizeof(hdr) + hdr.a_text, SEEK_SET) == -1 ||
      read(fd, &dynamic, sizeof(dynamic)) != sizeof(dynamic))
    return 0;

  if (N_GETMAGIC(hdr) != OMAGIC ||
      N_GETMID(hdr) != MID_I386 ||
      N_GETFLAG(hdr) != 0)
    return 0;

  close(fd);
  if (dynamic < SHARED_LIBRARY_START) dynamic = 0;


  fd = open("/bin/exosloader", O_RDONLY, 0);
  assert(fd >= 0);
  if (lseek(fd, 0, SEEK_SET) == -1 ||
      read(fd, &lhdr, sizeof(lhdr)) != sizeof(lhdr) ||
      lseek(fd, sizeof(lhdr) + lhdr.a_text, SEEK_SET) == -1)
    assert(0);
  start_text_addr = 0x70000020;
  start_text_pg = PGROUNDDOWN(start_text_addr);
  text_size = lhdr.a_text + sizeof(lhdr);
  data_size = lhdr.a_data;
  if (text_size % NBPG) {
    data_size += text_size % NBPG;
    text_size = PGROUNDDOWN(text_size);
  }
  bss_size = lhdr.a_bss;
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
	  sys_insert_pte(0, vpt[PGNO(temp_page)],
			 start_text_pg + text_size +
			 PGROUNDDOWN(data_size), 0, envid) != 0) {
	_exos_self_unmap_page(0, temp_page);
	__free((void*)temp_page);
	fprintf(stderr,"Error mmaping text segment\n");
	goto err_close;
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
  if ((r = (u_int)__mmap((void*)start_text_pg, text_size,
		    PROT_READ  | PROT_EXEC, 
		    MAP_FILE | MAP_FIXED | MAP_COPY, fd, (off_t)0, 0,
		    envid))
      != start_text_pg) {
    fprintf(stderr,"Error mmaping text segment %d %d\n", r, errno);
    goto err_close;
  }

  /* mmap the data segment read/write */
  if ((r = (u_int)__mmap((void*)(start_text_pg + text_size), data_size,
		    PROT_READ | PROT_WRITE | PROT_EXEC,
		    MAP_FILE | MAP_FIXED | MAP_COPY,
		    fd, text_size, (off_t)0, envid))
      != start_text_pg + text_size) {
    fprintf(stderr,"Error mmaping data segment %d %d\n", r, errno);
    goto err_close;
  }
  /* zero the bss */
  if (__zero_segment(envid, start_text_pg + text_size + data_size +
		     overlap_size, bss_size)) {
    fprintf(stderr,"Error zeroing bss segment\n");
    goto err_close;
  }

  close(fd);

  /* set start address */
  e->env_tf.tf_eip = start_text_addr;

  /* take care of args, etc */
  if ((r = setup_new_stack(path, argv, envp, envid, &e->env_tf.tf_esp)) < 0)
    goto err;

sys_cputs("===========");
  return 1;

 err_close:
  r = -1;
  close(fd);
 err:
  return r;
}

int
main(int argc, char *argv[])
{
  execv("/bin/ls2", argv);
  printf("<%d>\n", errno);

  return 0;
}
