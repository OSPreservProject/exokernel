
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

/* paging operations and data structures for storing and retrieveing
   paged pages and disk layout policy of paged out pages
   exports:
   _exos_page_io_page_in
   _exos_page_io_unuse_page
   _exos_page_io_page_clean
   _exos_page_io_shutdown
 */

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <exos/mallocs.h>
#include <exos/page_io.h>
#include <exos/page_replacement.h>
#include <exos/vm.h>
#include <xok/bc.h>
#include <xok/mmu.h>
#include <xok/sys_ucall.h>


/* The template for the paging file */
static char paging_filename[] = "/tmp/paging.XXXXXX";
/* The initial size of the freemap in number of pages */
#define INIT_FREEMAP_SIZE 128
/* The size of a cluster */
#define CLUSTER_MAX 16

struct page_io_info_struct {
  u_int flags;
#define PF_USED      0x01 /* page file used bit */
#define PF_WRITEABLE 0x02 /* if shared, then not writable */

  int wfd, rfd;

  u_int pages_free;
  u_int pages_max;
  u_int *freemap;

  u_int cluster_next;
  u_int cluster_pages_left;

  struct page_io_info_struct *next;
};

static struct page_io_info_struct *page_ci;
static struct page_io_info_struct *page_ciw; /* the writeable file */

static inline u_int get_bit(u_int *map, u_int i) {
  return (map[i / 32] >> (31 - (i % 32))) & 0x1;
}

static inline void set_bit(u_int *map, u_int i, u_int value) {
  if (value)
    /* set */
    map[i / 32] |= 0x1 << (31 - (i % 32));
  else
    /* clear */
    map[i / 32] &= ~(0x1 << (31 - (i % 32)));
}

int
_exos_a_get_page(int fd, off_t opage, struct page_info *pi) {
  if(_exos_self_insert_pte(0, PG_P|PG_U|PG_W, vp2va(pi->vpage), 0, NULL)) {
    return -1;
  }
  if(lseek(fd, opage*NBPG, SEEK_SET) == -1) {
    return -1;
  }
  if(read(fd, (void *) (pi->vpage*NBPG), NBPG) != NBPG) {
    return -1;
  }
  if (pi->flags != PG_P|PG_U|PG_W)
    if(_exos_self_insert_pte(0, (vpt[pi->vpage] & ~PGMASK) | pi->flags,
			     vp2va(pi->vpage), 0, NULL)) {
      return -1;
    }

  return 0;
}

int
_exos_a_put_page(int fd, off_t opage, u_int vpage) {
  if(lseek(fd, opage*NBPG, SEEK_SET) == -1) {
    return -1;
  }
  if(write(fd, (void *) (vpage*NBPG), NBPG) != NBPG) {
    return -1;
  }

  return 0;
}

static void 
print_map(u_int *map, u_int size) {
  int i;

  for(i = 0; i < size; i++) {
    printf("%d ", get_bit(map, i));
    if((i % 32) == 31) {
      printf("\n");
    }
  }
  printf("\n");
}

/* 
 * prints the important information in page_io_info_struct 
 */
static inline void 
print_pioi(struct page_io_info_struct *pioi) {
  printf("pages free %u\n", pioi->pages_free);
  printf("max pages %u\n", pioi->pages_max);

  printf("next cluster search %u\n", pioi->cluster_next);
  printf("number of pages left in this cluster %u\n",
	 pioi->cluster_pages_left);
  printf("freemap:\n");
  print_map(pioi->freemap, pioi->pages_max);
}

/* 
 * doubles the size of the freemap 
 * returns 0 on success 
 */
inline static int 
expand_freemap(struct page_io_info_struct *pioi) {
  pioi->freemap = (u_int*) realloc(pioi->freemap, pioi->pages_max * 2);
  if(pioi->freemap == NULL) {
    return -1;
  }
  bzero(&pioi->freemap[pioi->pages_max], pioi->pages_max);
  pioi->pages_free += pioi->pages_max;
  pioi->pages_max *= 2;
  return 0;
}

/* 
 * Search for a place to put a new page 
 * returns -1 if a free page on disk cannot be found 
 */
inline static int 
scan_freemap(struct page_io_info_struct *pioi) {
  u_int i;

  if(!pioi->pages_free) {
    return -1;
  }

  /* use clustering to keep pages from scattering all around the disk */
  if(pioi->cluster_pages_left) {
    while(pioi->cluster_next < pioi->pages_max) {
      i = pioi->cluster_next++;
      if(get_bit(pioi->freemap, i)) {
	continue;
      }
      set_bit(pioi->freemap, i, 1);
      pioi->pages_free--;
      pioi->cluster_pages_left--;
      return i;
    }
  }

  /* no space left for the rest of the cluster */
  /* search for any available space */
  pioi->cluster_pages_left = CLUSTER_MAX;
  for(i = 0; i < pioi->pages_max; i++) {
    if(get_bit(pioi->freemap, i)) {
      continue;
    }
    set_bit(pioi->freemap, i, 1);
    pioi->pages_free--;
    pioi->cluster_next = i;
    return i;
  }
  /* should not get here */
  return -1;
}

static int 
page_io_setup_pci(struct page_io_info_struct *pci) {
  char *fn; /* The template for the paging file name */

  fn = (char*) __malloc(strlen(paging_filename)+1);
  if (fn == NULL) {
    return -1;
  }
  strcpy(fn, paging_filename);

  pci->freemap = (u_int*) __malloc(INIT_FREEMAP_SIZE / 8);
  if(pci->freemap == NULL) {
    __free(fn);
    return -1;
  }
  bzero(pci->freemap, INIT_FREEMAP_SIZE / 8);

  pci->wfd = mkstemp(fn);
  pci->rfd = open(fn, O_RDONLY, 0);
  if(pci->wfd == -1 || pci->rfd == -1 || unlink(fn) == -1) {
    __free(fn);
    __free(pci->freemap);
    return -1;
  }
  __free(fn);

  pci->pages_free = pci->pages_max = INIT_FREEMAP_SIZE;
  pci->cluster_next = 0;
  pci->cluster_pages_left = CLUSTER_MAX;
  pci->flags = PF_USED | PF_WRITEABLE;
  pci->next = 0;
  return 0;
}

static int 
page_io_setup(void) {
  page_ci =
    (struct page_io_info_struct*)__malloc(sizeof(struct page_io_info_struct));
  if (page_ci == NULL) {
    return -1;
  }

  if (page_io_setup_pci(page_ci) == -1) {
    return -1;
  }

  page_ciw = page_ci;
  return 0;
}

static int 
page_io_setupw(void) {
  if (!page_ci) return page_io_setup();

  page_ciw =
    (struct page_io_info_struct*)__malloc(sizeof(struct page_io_info_struct));
  if (page_ciw == NULL) {
    return -1;
  }

  if (page_io_setup_pci(page_ciw) == -1) {
    return -1;
  }

  page_ciw->next = page_ci;
  page_ci = page_ciw;
  return 0;
}

/* 
 * closes paging file and performs cleanup operations.
 * returns non-zero if the paging file was not initialized.
 */
static int 
_exos_page_io_shutdown_pci(struct page_io_info_struct *pci) {
  if(!(pci->flags & PF_USED)) {
    return -1;
  }
  __free(pci->freemap);
  if (((pci->flags & PF_WRITEABLE) && (close(pci->wfd))) ||
      close(pci->rfd)) {
    return -1;
  }
  pci->flags = 0;
  return 0;
}

int
_exos_page_io_shutdown(void) {
  int rv = 0;

  while (page_ci) {
    struct page_io_info_struct *pci = page_ci->next;

    if (_exos_page_io_shutdown_pci(page_ci) == -1) rv = -1;
    __free(page_ci);
    page_ci = pci;
  }
  page_ci = page_ciw = 0;

  return rv;
}

/*
 * writes the page pointed to by vpage to disk.
 * automatically opens the pagefile if necessary.
 * returns 0 iff it was successful.
 */
int
_exos_page_io_page_clean(u_int vpage,  struct _exos_page_io_data *epid) {
  if (!epid || !isvamapped(vp2va(vpage))) {
    return -1;
  }

  /* make sure pagefile is initialized */
  if(!page_ciw) {
    if(page_io_setupw()) {
      return -1;
    }
  }

  /* need a new location on disk */
  if (!(epid->flags & EPID_USED) || epid->pci != page_ciw) {
    /* mark old location free if moving to new file */
    if (epid->flags & EPID_USED && _exos_page_io_unuse_page(epid)) {
      return -1;
    }
    /* expand freemap if needed */
    if(!page_ciw->pages_free) {
      if(expand_freemap(page_ciw)) {
	/* could not allocate more memory for freemap */
	return -1;
      }
    }

    /* find a free page on disk */
    epid->index = scan_freemap(page_ciw);
    if(epid->index < 0) {
      /* should not happen */
      return -1;
    }
    epid->pci = page_ciw;
  } else {
    /* use a previously used location on disk */
    epid->flags &= ~EPID_USED;
    if(epid->index >= page_ciw->pages_max) {
      /* trying to use a page that is not in the map */
      return -1;
    }
    if(!get_bit(page_ciw->freemap, epid->index)) {
      /* trying to use an unused free page */
      return -1;
    }
  }
  
  /* write page to disk asynchronously */
  if(_exos_a_put_page(page_ciw->wfd, epid->index, vpage)) {
    return -1;
  }

  epid->flags |= EPID_USED;

  return 0;
}

/*
 * the page on disk pointed to by epid is freed.
 * returns 0 iff it successfully freed that page from disk.
 */
int
_exos_page_io_unuse_page(struct _exos_page_io_data *epid) {
  struct page_io_info_struct *pci;
 
  if(!page_ci || !epid || !(epid->flags & EPID_USED) ||
     !(epid->pci->flags & PF_USED)) {
    /* epid/pagefile is not initialized */
    return -1;
  }

  /* this epid will no longer be used */
  epid->flags &= ~EPID_USED;

  pci = epid->pci;

  if(epid->index >= pci->pages_max) {
    /* trying to free a page that is not paged */
    return -1;
  }
  if(!get_bit(pci->freemap, epid->index)) {
    /* trying to free an already free page */
    return -1;
  }
  set_bit(pci->freemap, epid->index, 0);
  pci->pages_free++;
  /* close file if only readable and nothing left in it */
  if (pci->pages_free == pci->pages_max &&
      !(pci->flags & PF_WRITEABLE)) {
    struct page_io_info_struct *pci2 = page_ci;
    /* cleanup linked list */
    if (pci2 == pci)
      page_ci = pci2->next;
    else
      do {
	if (pci2->next == pci) {
	  pci2->next = pci->next;
	  break;
	}
	pci2 = pci2->next;
	if (!pci2) return -1;
      } while (1);
    if (_exos_page_io_shutdown_pci(pci)) {
      return -1;
    }
    __free(pci);
  }

  return 0;
}

/* 
 * reads the page pointed to by epid from disk to the page pointed
 *  to by vpage.
 * returns 0 iff it succeeds.
 */
int 
_exos_page_io_page_in(struct _exos_page_io_data *epid, struct page_info *pi) {
  if(!page_ci || !epid || !(epid->flags & EPID_USED) ||
     !(epid->pci->flags & PF_USED)) {
    /* epid/pagefile is not initialized */
    return -1;
  }

  /* bring the page in */
  if(_exos_a_get_page(epid->pci->rfd, epid->index, pi)) {
    return -1;
  }

  return 0;
}

