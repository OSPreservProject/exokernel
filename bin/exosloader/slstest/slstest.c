#include <exos/cap.h>
#include <exos/ipc.h>
#include <exos/nameserv.h>
#include <exos/sls.h>
#include <exos/ubc.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <xok/env.h>
#include <xok/kqueue.h>
#include <xok/pmap.h>
#include <xok/sys_ucall.h>

static u_int slseid = 0;

static int
S_OPEN(char *name, int flags, mode_t mode) {
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

static int
S_CLOSE(int fd) {
  int ret, ipcret;

  if (!slseid) slseid = nameserver_lookup(NS_NM_SLS);
  assert(slseid);
  ret = _ipcout_default(slseid, 5, &ipcret, IPC_SLS, SLS_CLOSE, fd, 0, 0);
  assert(ret == 0);
  return ipcret;
}

static int
S_NEWPAGE(u_int vpte) {
  int ret, ipcret;

  if (!slseid) slseid = nameserver_lookup(NS_NM_SLS);
  assert(slseid);
  ret = _ipcout_default(slseid, 5, &ipcret, IPC_SLS, SLS_NEWPAGE, vpte, 0, 0);
  assert(ret == 0);
  return ipcret;
}

/* sls vcopy destination */
struct vc_info {
  struct {
    int fd;
    u_quad_t blk;
  } to;
  struct {
    u32 dev;
    u_quad_t blk;
  } from;
};

static int
S_READINSERT(u_long vcopyinfo) {
  int ret, ipcret;

  if (!slseid) slseid = nameserver_lookup(NS_NM_SLS);
  assert(slseid);
  ret = _ipcout_default(slseid, 5, &ipcret, IPC_SLS, SLS_READINSERT, vcopyinfo,
			0, 0);
  assert(ret == 0);
  return ipcret;
}

int main() {
  int fd;
  int *i = (int*)0x70000000;
  struct vc_info vci;
  struct bc_entry *bce;
  struct Xn_name *xn;
  struct Xn_name xn_nfs;
  char buf[100];

  fd = S_OPEN("/etc/hosts", O_RDONLY, 0);
  assert(fd >= 0);
  assert(S_NEWPAGE(0x70000000 | PG_W | PG_P | PG_U) == 0);
  *i = 77;
  printf("<%d>\n", *i);
  vci.to.fd = fd;
  vci.to.blk = 0;
  assert(S_READINSERT((u_long)&vci) == 0);
  bce = __bc_lookup64(vci.from.dev, vci.from.blk);
  assert(bce);
  if (bce->buf_dev > MAX_DISKS) {
    xn_nfs.xa_dev = bce->buf_dev;
    xn_nfs.xa_name = 0;
    xn = &xn_nfs;
  } else {
    xn = &__sysinfo.si_pxn[bce->buf_dev];
  }

  /* map the page */
  if (sys_self_bc_buffer_map(xn, CAP_ROOT,
			     ppnf2pte(bce->buf_pp, PG_P|PG_W|PG_U),
			     0x70001000) < 0)
    assert(0);
  memcpy(buf, (char*)0x70001000, 99);
  buf[99] = 0;
  printf("%s\n\n", buf);
  assert(S_CLOSE(fd) == 0);
  printf("DONE!\n");

  return 0;
}
