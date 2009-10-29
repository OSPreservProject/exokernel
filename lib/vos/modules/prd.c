
/*
 * Copyright MIT 1999
 */

/* 
 * a simple descriptor for dpf related packet rings: all we can do is 
 * wait on it for read, and read a packet 
 */

#include <stdlib.h>
#include <xok/sys_ucall.h>
#include <vos/fdtypes.h>

#include <vos/fd.h>
#include <vos/wk.h>
#include <vos/net/fast_eth.h>


#define dprintf if (0) printf



struct ringbuf
{
  struct ringbuf *next;
  u_int *pollptr;               /* should point to flag */
  u_int flag;                   /* set by kernel to say we have a packet */
  int n;                        /* should be 1 */
  int sz;                       /* amount of buffer space */
  char *data;                   /* should point to space */
  char space[ETHER_MAX_LEN];    /* actual storage space for packet */
};
#define MAX_RING_SZ     65536


typedef struct
{
  unsigned int ring_sz;
  int ring_id;
  struct ringbuf *ringbuffers;
} prd_state_t;



/* return pktring ID from descriptor */
int
prd_ring_id(int d)
{
  S_T(fd_entry) *fd;
  prd_state_t *s;
  
  if (d >= MAX_FD_ENTRIES || d < 0 || fds[d] == NULL) 
    RETERR(V_BADFD);
  fd = fds[d]->self;
  
  if (fd->type != FD_TYPE_PRD)
    RETERR(V_BADFD);
  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  s = (prd_state_t*)fd->state;
  dprintf("prd_ring_id: returning %d\n", s->ring_id);
  return s->ring_id;
}



/*
 * close state for the final time (ref cnt expired)
 */
static int
prd_close_final (S_T(fd_entry) *fd)
{
  prd_state_t *s;

  if (fd->type != FD_TYPE_PRD)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);
  
  s = (prd_state_t*)fd->state;

  if (s->ring_sz > 0)
  {
    struct ringbuf *rbptr = s->ringbuffers;
    struct ringbuf *rbptr_next = s->ringbuffers->next;

    while(s->ring_sz > 0)
    {
      rbptr_next = rbptr->next;
      free(rbptr);
      rbptr = rbptr_next;
      s->ring_sz--;
    }
  }

  if (s->ring_id >= 0)
  {
    if (sys_pktring_delring (CAP_USER, s->ring_id) < 0)
      printf("prd: cannot delete packet ring %d\n", s->ring_id);
  }

  free(s);
  dprintf("env %d: prd: close_final called\n", getpid());
  return 0;
}


/*
 * close fd 
 */
static int
prd_close (S_T(fd_entry) *fd)
{
  if (fd->type != FD_TYPE_PRD)
    RETERR(V_BADFD);
  if (fd->state == 0L)
    RETERR(V_BADFD);
  return 0;
}


/*
 * open fd
 */
static int
prd_open (S_T(fd_entry) *fd, const char *name, int flags, mode_t mode)
{
  prd_state_t *s;

  if (strncmp(name,"/dev/pktring",12)==0)
  {
    int ring_sz = atoi(&name[12]);
    int i=0;
    struct ringbuf *rbptr;
    struct ringbuf *rbptr_prev = 0L;

    dprintf("opening %s\n", name);

    if (ring_sz < 1 || ring_sz > MAX_RING_SZ)
      RETERR(V_BADFD);
    
    (prd_state_t*)(fd->state) = (prd_state_t*) malloc(sizeof(prd_state_t));
    s = (prd_state_t*)fd->state;
    
    s->ringbuffers = 0L;
    s->ring_sz = 0;
    s->ring_id = -1;

    do {
      rbptr = (struct ringbuf*) malloc(sizeof(struct ringbuf));
  
      if (rbptr == 0L) {
	rbptr_prev->next = s->ringbuffers;
        s->ring_sz = i;
        close(fd->fd);
        RETERR(V_BADFD);
      }
     
      if (s->ringbuffers == 0L)
        s->ringbuffers = rbptr;
  
      rbptr->pollptr = &(rbptr->flag);
      rbptr->n = 1;
      rbptr->sz = ETHER_MAX_LEN;
      rbptr->data = &(rbptr->space[0]);
      rbptr->flag = 0;
  
      if (rbptr_prev != 0L)
        rbptr_prev->next = rbptr;
      rbptr_prev = rbptr;
  
      i++;
    } while(i < ring_sz);
  
    /* fix last entry */
    rbptr->next = s->ringbuffers;

    s->ring_sz = ring_sz;
    s->ring_id =
      sys_pktring_setring(CAP_USER, 0, (struct pktringent *) s->ringbuffers);

    dprintf("xok packetring %d: on descriptor %d, total of %d bufs\n",
      s->ring_id, fd->fd, s->ring_sz);
  }

  else
    RETERR(V_BADFD);
  return fd->fd;
}


/*
 * verify state
 */
static int
prd_verify(S_T(fd_entry) *fd)
{
  if (fd->type != FD_TYPE_PRD)
    RETERR(V_BADFD);
  if (fd->state == 0L)
    RETERR(V_BADFD);
  return 0;
}


/*
 * duplicate fd and state
 */
static int
prd_incref(S_T(fd_entry) *fd, u_int new_envid)
{
  if (fd->type != FD_TYPE_PRD)
    RETERR(V_BADFD);
  if (fd->state == 0L)
    RETERR(V_BADFD);
  return 0;
}



/*
 * read buffer from spipe
 */
static int
prd_read (S_T(fd_entry) *fd, void *buffer, int nbyte)
{
  prd_state_t *s;

  if (fd->type != FD_TYPE_PRD)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);

  if (nbyte < ETHER_MAX_LEN)
    RETERR(V_INVALID);
  
  s = (prd_state_t*)fd->state;

  while (s->ringbuffers->flag == 0)
    asm volatile ("" ::: "memory");

  memmove(buffer, &(s->ringbuffers->space[0]), ETHER_MAX_LEN);
  s->ringbuffers->flag = 0;
  s->ringbuffers = s->ringbuffers->next;
  return ETHER_MAX_LEN;
}



/* 
 * return 1 if this descriptor is ready for read/write/except
 */
static int
prd_select (S_T(fd_entry) *fd, int rw)
{
  prd_state_t *s;

  if (fd->type != FD_TYPE_PRD)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);

  s = (prd_state_t*)fd->state;

  if (rw == SELECT_WRITE)
    return 0;

  else
    return (s->ringbuffers->flag != 0);
}



/* 
 * build a wk pred that wakes up if descriptor is read for read/write/except.
 * Returns the number of terms added on.
 */
static int always_one = 1;
static int
prd_select_pred (S_T(fd_entry) *fd, int rw, struct wk_term *wk)
{
  prd_state_t *s;

  if (fd->type != FD_TYPE_PRD)
    RETERR(V_BADFD);
  
  if (fd->state == 0L)
    RETERR(V_BADFD);

  s = (prd_state_t*)fd->state;
  dprintf("select_pred called\n");

  if (rw == SELECT_WRITE)
    /* sleep forever */
    return wk_mkcmp_neq_pred(wk, &always_one, 1, CAP_USER);

  else
    return wk_mkcmp_neq_pred(wk, &s->ringbuffers->flag, 0, CAP_USER);
}



static fd_op_t const prd_ops = 
{
  prd_verify,
  prd_incref,

  prd_open,
  prd_read,
  NULL,
  NULL, /* readv */
  NULL, /* writev */
  NULL, /* lseek */
  prd_select,
  prd_select_pred,
  NULL, /* ioctl */
  NULL, /* fcntl */
  NULL, /* flock */
  prd_close,
  prd_close_final,
  NULL, /* dup */
  NULL, /* fstat */
  NULL, /* socket */
  NULL, /* bind */
  NULL, /* connect */
  NULL, /* accept */
  NULL, /* listen */
  NULL, /* sendto */
  NULL  /* recvfrom */
};



/* call this to initialize spipe fd stuff */
void
prd_init()
{
  register_fd_ops(FD_TYPE_PRD, &prd_ops);
}

