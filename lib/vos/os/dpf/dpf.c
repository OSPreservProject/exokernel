
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <xok/sys_ucall.h>
#include <dpf/dpf.h>
#include <dpf/dpf-internal.h>

#include <vos/dpf/dpf.h>
#include <vos/net/fast_ip.h>
#include <vos/net/iptable.h>
#include <vos/cap.h>
#include <vos/assert.h>
#include <vos/errno.h>


#undef UDP_FRAG
#define dprintf if (0) kprintf


int 
dpf_delete_binding(u_int filterid, u_int envid)
{
  dprintf("deleting dpf for env %d\n",envid);
  return sys_dpf_delete(CAP_DPF, filterid, CAP_USER, envid);
}


int
dpf_bind_udpfilter(unsigned short port, unsigned char *ip_src, 
                   int ring_id, u_int envid)
{
  unsigned short src_port;
  int fid;

  dprintf("inserting dpf for env %d\n",envid);

  assert(port > 0);
  src_port = htons(port);
  dprintf("making udp filter: port %d, htons %d\n", port, src_port);

  if(bcmp(ip_src, IPADDR_ANY, 4) == 0) 
  {
#ifdef UDP_FRAG
    struct dpf_ir filter1;
    struct dpf_ir filter2;
#endif
    struct dpf_ir filter3;

#ifdef UDP_FRAG
    dpf_begin(&filter1);  /* must be done before use */
    dpf_eq16(&filter1, 12, 0x8);
    dpf_eq8(&filter1, 14, 0x45);
    dpf_meq8(&filter1, 20, 0x20, 0x20);
    dpf_eq8(&filter1, 23, 0x11);
    dpf_eq16(&filter1, 36, src_port);
    dpf_actioneq16(&filter1, 38);
#endif

    dpf_begin(&filter3);  /* must be done before use */
    dpf_eq16(&filter3, 12, 0x8);
    dpf_eq8(&filter3, 14, 0x45);
    dpf_meq16(&filter3, 20, 0x2000, 0x00);
    dpf_eq8(&filter3, 23, 0x11);
    dpf_eq16(&filter3, 36, src_port);

#ifdef UDP_FRAG
    dpf_begin(&filter2);  /* must be done before use */
    dpf_eq16(&filter2, 12, 0x8);
    dpf_eq8(&filter2, 14, 0x45);
    dpf_stateeq16(&filter2, 18, 0);
    dpf_eq8(&filter2, 23, 0x11);
#endif
    
    fid = sys_dpf_insert(CAP_DPF, CAP_DPF, &filter3, ring_id, CAP_USER, envid);
   
#ifdef UDP_FRAG
    fid = sys_dpf_insert(CAP_DPF, CAP_DPF, &filter3, ring_id, CAP_USER, envid);
    filter2.version = fid; /* XXX - BAD HACK */
    filter1.version = fid; /* XXX - BAD HACK */
    fid2 = sys_dpf_insert(CAP_DPF, CAP_DPF, &filter1, ring_id, CAP_USER, envid);
    assert(fid == fid2);
    fid2 = sys_dpf_insert(CAP_DPF, CAP_DPF, &filter2, ring_id, CAP_USER, envid);
    assert(fid == fid2);
#endif
  } 
  
  else 
  {
    struct dpf_ir filter1;
#ifdef UDP_FRAG
    struct dpf_ir filter2;
    int fid2;
#endif

#ifdef UDP_FRAG
    dpf_begin(&filter1);  /* must be done before use */
    dpf_eq16(&filter1, 12, 0x8);
    dpf_eq8(&filter1, 14, 0x45);
    dpf_meq16(&filter1, 20, 0x2000, 0x2000);
    dpf_eq8(&filter1, 23, 0x11);
    dpf_eq8(&filter1, 30, ip_src[0]);
    dpf_eq8(&filter1, 31, ip_src[1]);
    dpf_eq8(&filter1, 32, ip_src[2]);
    dpf_eq8(&filter1, 33, ip_src[3]);
    dpf_eq16(&filter1, 36, src_port);
    dpf_actioneq16(&filter1, 38);

    dpf_begin(&filter2);  /* must be done before use */
    dpf_eq16(&filter2, 12, 0x8);
    dpf_eq8(&filter2, 14, 0x45);
    dpf_stateeq16(&filter2, 20, 0);
    dpf_eq8(&filter2, 23, 0x11);
    dpf_eq8(&filter2, 30, ip_src[0]);
    dpf_eq8(&filter2, 31, ip_src[1]);
    dpf_eq8(&filter2, 32, ip_src[2]);
    dpf_eq8(&filter2, 33, ip_src[3]);

    fid = sys_dpf_insert(CAP_DPF, CAP_DPF, &filter1, ring_id, CAP_USER, envid);
    filter2.version = fid; /* XXX - BAD HACK */
    fid2 = sys_dpf_insert(CAP_DPF, CAP_DPF, &filter2, ring_id, CAP_USER, envid);
    assert(fid == fid2);
#endif
    
    dpf_begin(&filter1);  /* must be done before use */
    dpf_eq16(&filter1, 12, 0x8);
    dpf_eq8(&filter1, 14, 0x45);
    /* the fragment offset ip_off's MF bit should be 0 for no fragmentation */
    dpf_meq16(&filter1, 20, 0x2000, 0x00);
    dpf_eq8(&filter1, 23, 0x11);
    dpf_eq8(&filter1, 30, ip_src[0]);
    dpf_eq8(&filter1, 31, ip_src[1]);
    dpf_eq8(&filter1, 32, ip_src[2]);
    dpf_eq8(&filter1, 33, ip_src[3]);
    dpf_eq16(&filter1, 36, src_port);
  
    fid = sys_dpf_insert(CAP_DPF, CAP_DPF, &filter1, ring_id, CAP_USER, envid);
  }

  if (fid < 0)
    RETERR(V_ADDRNOTAVAIL);
  return fid;
}


int 
dpf_bind_tcpfilter(u_short localport, int remoteport, int ringid, u_int envid)
{
  struct dpf_ir ir;

  dpf_begin (&ir);					

  // dpf_eq16 (&ir, 12, 0x8);
  // dpf_eq8  (&ir, 23, IP_PROTO_TCP); /* must be TCP packet */

  if (remoteport > 0)
    /* 34 is offset of source TCP port! */
    dpf_eq16 (&ir, 34, htons((uint16)remoteport));
  /* 36 is offset of destination TCP port! */
  dpf_eq16 (&ir, 36, htons((uint16)localport));

  return sys_dpf_insert(CAP_DPF, CAP_DPF, &ir, ringid, CAP_USER, envid);
}


