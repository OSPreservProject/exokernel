
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

#include "namei.h"
#include <exos/debug.h>
//#define CACHEDLOOKUP /* if doing cheap/dirty cached lookup see namei_cache.c */

//#define NAMEIDEBUG

#ifdef NAMEIDEBUG
#define nprintf if (1) printf
#else
#define nprintf if (0) printf
#endif

#if 0
#define COUNT_COPY global_ftable->xod_envid++;
#else
#define COUNT_COPY
#endif
#define RELEASE(filp) nprintf("> %3d ",__LINE__); NAMEI_RELEASE((filp),(struct shortfile *)0)
#define ACQUIRE(filp) nprintf("> %3d ",__LINE__); if (CHECKOP((filp),acquire)) DOOP((filp),acquire,(filp))

int namei_lookup(struct file *old_filp, char *name, struct file *new_filp);

/* Traverse Mount Points Functions */
static inline int 
mounttraversalb(struct file *from,struct file **to) {
  int i;
  mounttable_t *mt;
  mount_entry_t *m;
  START(ftable,step9);
  mt = global_ftable->mt;
  m = mt->mentries;
  for (i = 0; i < mt->nr_mentries; i++,m++) {
    if (m->inuse <= 0) continue;
    if (FILPEQ(from,&m->to)) {
      PRSHORTFILP("mounttraversalb: ",from,"\n");
      PRSHORTFILP("matches: ",&m->from,"\n");
      if (to) *to = &m->from;
      STOP(ftable,step9);
      return 0;
    }
  }
  STOP(ftable,step9);
  return -1;
}


static inline int 
mounttraversalf(struct file *from,struct file **to) {
  int i;
  mounttable_t *mt;
  mount_entry_t *m;
  START(ftable,step8);
  mt = global_ftable->mt;
  m = mt->mentries;
  for (i = 0; i < mt->nr_mentries; i++,m++) {
    if (m->inuse <= 0) continue;
    if (FILPEQ(from,&m->from)) {
      PRSHORTFILP("mounttraversalf: ",from,"\n");
      PRSHORTFILP("matches: ",&m->to,"\n");
      if (to) *to = &m->to;
      STOP(ftable,step8);
      return 0;
    }
  }
  STOP(ftable,step8);
  return -1;
}

/* do_namei: namei engine.  assumes non-empty path. 
 * Preconditions: the first directory of the path is passed via cwd, the caller figures
 *   this filp out.
 * Postcontidions: the last element is acquired implicitly by lookup */
/* lookups of dots are passed down */
static int 
do_namei(struct file *cwd, 
	 const char *path, struct file *result_filp, char *result_name,
	 int *followlinks, int followlastlink)
{
    int error;
    char next[NAME_MAX +1];
    struct file old_space, new_space,tmp_traversal_space;
    struct file *old_filp = &old_space;
    struct file *new_filp = &new_space;
    struct file *tmp_traversal_filp = &tmp_traversal_space;
    struct file *tmp_swap_filp;
    struct shortfile release = {0,0};

    old_filp->flock_state = FLOCK_STATE_UNLOCKED;
    new_filp->flock_state = FLOCK_STATE_UNLOCKED;
    tmp_traversal_filp->flock_state = FLOCK_STATE_UNLOCKED;

    DPRINTF(SYSHELP_LEVEL,("do_namei: path: %s fl: %d fll: %d\n",path,*followlinks,followlastlink));

    nprintf("do_namei: path: \"%s\" fl: %d fll: %d\n",
	    path,*followlinks,followlastlink);

    /* If we have followed too many symlinks, error ELOOP */
    if (*followlinks > LINK_MAX) {
      errno = ELOOP;
      return -1;
    }

    ASSERTOP(cwd,lookup, directory with no lookup);

    *old_filp = *cwd;
    COUNT_COPY;
    ACQUIRE(old_filp);

    /* old_filp contains the directory we start at, and
     * new_filp will contain the name we just traversed.
     * when we return to top of loop, we copy new_filp into old_filp 
     * when we exit the loop, the resulting filp will be in new_filp */
    
    while(*path) {
      nprintf("path before : \"%s\" \n",path); 
      path = getNextComponent(path,next);

      nprintf("NEXT: \"%s\" path[0]: %d \n",next,*path);
      /* printf("path: \"%20s\"     next: \"%s\"\n",path,next); */
      
      if (EQDOTDOT(next)) {
	/* check traversal of /.. */
	if (FILPEQ(old_filp,__current->root)) {
	  next[1] = (char)0;	/* make it . */
	} 
	else 
	/* check traversal of /mnt/.. takes you back to / */
	  if (mounttraversalb(old_filp,&tmp_traversal_filp) == 0) {
	    RELEASE(old_filp);
	    COUNT_COPY;
	    *old_filp = *tmp_traversal_filp;
	    ACQUIRE(old_filp);
	    PRSHORTFILP("NEW POINT",old_filp,"\n");
	    /* fall through, old_filp points to /mnt, and next is ..
	     * so the later operations will handle the lookup of .. */
	  }
      }

      if (EQDOT(next)) {
	nprintf("in namei shortcircuiting . path: \"%s\"\n",path);
      	tmp_swap_filp = old_filp;
	old_filp = new_filp;
	new_filp = tmp_swap_filp;
	goto check_forward_mount;
      }

      if (!(S_ISDIR(old_filp->f_mode))) {
	nprintf("old_filp: ENOTDIR\n");
	errno = ENOTDIR;
	RELEASE(old_filp);
	return -1;
      }

      ASSERTOP(old_filp,lookup,no lookup operation in traversal of a path);
      
#ifdef CACHEDLOOKUP
      error = namei_lookup(old_filp, next, new_filp);
#else
      /* Do the actual lookup */
      nprintf("#OLD0: %s %d %08x\n",next,old_filp->f_ino, old_filp->f_mode);
      error = DOOP(old_filp,lookup,(old_filp,next,new_filp));
      nprintf("#OLD1: %s %d %08x\n",next,old_filp->f_ino, old_filp->f_mode);
#endif /* namei_lookup */

      if (error) {
	RELEASE(old_filp);
	nprintf("LOOKUP old_filp %s failed errno: %s\n",next,strerror(errno));
	return -1;
      }

      PRSHORTFILP("LOOKUP     ",old_filp," ");
      nprintf("#NAME: %p %s %d %08x\n",new_filp,next,new_filp->f_ino, new_filp->f_mode);
      nprintf("#OLD2: %p %s %d %08x\n",old_filp,next,old_filp->f_ino, old_filp->f_mode);
      PRSHORTFILP("  RETURNED:",new_filp,"\n");

      /* here you want to check if new_filp is a symbolic link, it is
	 you want to call readlink, then return namei on the new path we
	 got from readlink.  followlink will be an integer denoting how
	 many links we have followed. if 0 means we dont follow links. */
      
      if (S_ISLNK(new_filp->f_mode)) {
	char linkpath[PATH_MAX + 1];
	struct file *link_filp;
	nprintf("%d) SYMLINK new_filp %d is symlink remaining path: \"%s\" fll %d\n",
	       *followlinks,new_filp->f_ino,path,followlastlink);
	/* if this is the last path element, and we dont followlastlink */
	if (*path == 0 && followlastlink == 0) {
	  RELEASE(old_filp);
	  break;
	}

	ASSERTOP(new_filp,readlink,no readlink on a symlink filp);
	
	nprintf("3#NAME: %p %s %d %08x\n",new_filp,next,new_filp->f_ino, new_filp->f_mode);
	if ((error = DOOP(new_filp,readlink,(new_filp,linkpath,PATH_MAX))) == -1) {
	  RELEASE(old_filp);
	  RELEASE(new_filp);
	  return -1;
	}
	if (error >= 0) linkpath[error] = 0;

	nprintf("5#NAME: %p %s %d %08x\n",new_filp,next,new_filp->f_ino, new_filp->f_mode);
	RELEASE(new_filp);
	nprintf("6#NAME: %p %s %d %08x\n",new_filp,next,new_filp->f_ino, new_filp->f_mode);

	nprintf("%d) following symlink path: %s\n",*followlinks,linkpath);

	/* since new_filp is a symlink it could only have come from
	   a lookup */
	(*followlinks)++;

	if (linkpath[0] == '/') link_filp = (__current->root); 
	else link_filp = old_filp;


	error = do_namei(link_filp,
			 linkpath,
			 new_filp, 
			 result_name,
			 followlinks,
			 followlastlink);
	
	RELEASE(old_filp);

	nprintf("%d) error: %d PATH LEFT: %s, from %d, %x\n",
		*followlinks,error,path,new_filp->f_ino, new_filp->f_mode);
	if (error) {
	  nprintf("DO_NAMEI failed errno: %s\n",strerror(errno));
	  return -1;
	}


	goto next;
	//return error;
      } 

      RELEASE(old_filp);
    check_forward_mount:
      if (mounttraversalf(new_filp,&tmp_traversal_filp) == 0) {
	nprintf("traversing mount point ->, remaining: \"%s\"\n",path);
	RELEASE(new_filp);
	COUNT_COPY;
	*new_filp = *tmp_traversal_filp;
	ACQUIRE(new_filp);
#ifdef NAMEIDEBUG
	PRSHORTFILP("NEW POINT",new_filp,"\n");
#endif
	next[0] = '.'; next[1] = 0;
      }
    next:
      if (*path) {
	/* efficient way of doing: *old_filp = *new_filp, */
	tmp_swap_filp = old_filp;
	old_filp = new_filp;
	new_filp = tmp_swap_filp;
      }
    }
    
#ifdef NAMEIDEBUG
    nprintf("#LAST NAME: %s %d %08x\n",next,new_filp->f_ino, new_filp->f_mode);
#endif
    COUNT_COPY;
    *result_filp = *new_filp;
    if (result_name) {
      strcpy(result_name,next);
    }
    return 0;
}

/* namei - translates path to a locked file object, if followlinks is
 set, then namei follows links.  */
inline int 
nameio(struct file *relative,
      const char *path, 
      struct file *result_filp, 
      char *result_path,
      int followlastlink) 
{
  int error;
  int followlinks = 1;
  
  START(ftable,namei);

//#ifdef NAMEIDEBUG
#if 0
  printf ("namei: entering path: \"%s\" fl %d\n",path,followlastlink);
#endif
  DPRINTF(SYSHELP_LEVEL,("namei: entering path: %s\n",path));
  
  assert(__current);
  if (CHECKOP(__current->root, lookup) == 0 ||
      CHECKOP(__current->cwd, lookup) == 0) {
    errno = ENOENT;
    return -1; 
  }
#if 0
  ASSERTOP(__current->root,lookup,BAD ROOT);
  ASSERTOP(__current->cwd,lookup, BAD CWD);
#endif
  
  if (strlen(path) > PATH_MAX) { 
    errno = ENAMETOOLONG;
    return -1; 
  }
  
  /* according to POSIX NULL pathnames are invalid */
  if (*path == (char)NULL) {
    /* should fix this at some point, it requires also fixing
       getnextcomponent */
    path = ".";
  }
#ifdef NAMEIDEBUG
  printf("namei rel: %p path: %s fl: %d\n",
	 relative, path, followlinks);
#endif
  if (path[0] == '/') relative = (__current->root); 
  else if (relative == NULL) relative = (__current->cwd);

  error = do_namei(relative,path, result_filp,result_path, 
		   &followlinks,followlastlink);

#ifdef NAMEIDEBUG
  if (!error) {
    //printf("\n"); pr_filp(result_filp,"NAMEI RETURNS"); printf("\n");
    printf("NAMEI RETURNS: ino: %d mode: %x\n",result_filp->f_ino, result_filp->f_mode);
  } else {
    printf("namei error: %d, errno: %d %s\n",error, errno,strerror(errno));
  }
#endif
  STOP(ftable,namei);
  return error;
}

int 
namei(struct file *relative, const char *path,  struct file *result_filp, 
      int followlinks) {
  return nameio(relative,path,result_filp,NULL,followlinks);
}

int 
namei_release(struct file *filp,int force) {
  START(ftable,namei_rel);
#ifdef NAMEIDEBUG
  PRSHORTFILP("NAMEI_RELEASE:",filp,"");
  printf(" fr:%d\n",force);
#endif
#if 0
  if (filp == __current->root) {
    printf("Trying to release root filp\n");
    goto done;
  }
#endif
  /* ignore do not release for now */
  if (1 || !(filp->f_flags & O_DONOTRELEASE) || force) {
    if (CHECKOP(filp,release)) {
      DOOP(filp,release,(filp));
    }
  }
done:
  STOP(ftable,namei_rel);
  return 0;
}

#if 0
/*
  Pseudo code:
  two procedures/interfaces: 
  1) to look for an item, like what stat, chdir, would use.
     takes a path --> filp
  2) to open an iterm, like what open would use.
     takes a path --> filp of directory, name (possibly different
     in the case of open "/foo" where foo is --> bar, then
     we return filp of "/" and name of "bar"

Old namei: 
  if path is absolute, relative to root, else relative to cwd.
  while(path) {
    get next path element;
    check if not a directory
    check if null, if so remap to '.'
    if equal to '..' 
       if we are in /, return /
       if we are in mountpoint return true parent
    assure lookup op
    lookup path element.
    on error return.
    if new element is a symlink read the symlink.
    call do_namei on new path.
    if element we are on is a mount point, remap to mounted element
  }
  return last element

New namei:
  if path is absolute, relative = root else relative = cwd
  while(path) {
    element = get next path;
    new = lookup element from relative.
    if error return
    if new is not dir



You traverse by:
  Lookup
  Readlink
  Mount point crossing.
    */
#endif


#if 0
int 
nameio(struct file *relative,
       const char *path, 
       struct file *result_filp, 
       char *result_path,
       int followlinks) {
  int error;
  char path0[PATH_MAX];

  DPRINTF(SYSHELP_LEVEL,("namei: entering path: %s\n",path));
  
  assert(__current);
  ASSERTOP(__current->root,lookup,BAD ROOT);
  ASSERTOP(__current->cwd,lookup, BAD CWD);
  
  if (strlen(path) > PATH_MAX) { 
    errno = ENAMETOOLONG;
    return -1; 
  }
  
  /* according to POSIX NULL pathnames are invalid */
  if (*path == (char)NULL) {
    /* should fix this at some point, it requires also fixing
       getnextcomponent */
    path = ".";
  }
#ifdef NAMEIDEBUG
  printf("nameio rel: %p path: %s fl: %d\n",
	 relative, path, followlinks);
#endif
  path0[0] = 0;
  if (relative == NULL) {
    error = do_namei(__current->cwd,path, result_filp,path0, 1,followlinks);
  } else {
    PRSHORTFILP("DO_NAMEI with relative:",relative,"");
    error = do_namei(relative,path, result_filp, path0,1,followlinks);
  }
  if (!error) {
    pr_filp(result_filp,"NAMEI RETURNS");
  } else {
#ifdef NAMEIDEBUG
    printf("namei error: %d, errno: %d %s\n",error, errno,
	   strerror(errno));
#endif
  }
  return error;

}
#endif
