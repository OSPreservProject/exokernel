#include <xok/xok_net_interface.h>

#include <xok/defs.h>
#include <xok/types.h>
#include <xok/kerrno.h>
#include <xok/bitvector.h>
#include <xok_include/assert.h>
#include <xok/queue.h>
#include <xok/env.h>
#include <xok/sys_proto.h>
#include <xok/sysinfo.h>
#include <dpf/dpf.h>
#include <dpf/dpf-internal.h>
#include <xok/pkt.h>
#include <xok/pctr.h>
#include <xok/pktring.h>
#include <xok/init.h>
#include <xok/pmap.h>
#include <xok/printf.h>

#include <machine/pio.h>
#include <xok/picirq.h>
/*
 * mseu, adding some oskit includes.
 */
#include <oskit/types.h>
#include <oskit/dev/dev.h>
#include <oskit/dev/net.h>
#include <oskit/dev/ethernet.h>
#include <oskit/dev/linux.h>
#include <xok_include/net/ether.h>
#include <oskit/io/bufio.h>

/*********************** structures *************************/
/*
 * I need to use this for the card struct.
 */
struct etherdev {
	oskit_etherdev_t	*dev;
	oskit_netio_t	*send_nio;
	oskit_netio_t	*recv_nio;
	oskit_devinfo_t	info;
	unsigned char	haddr[ETHER_ADDR_LEN];
   struct network *xoknet;
	u_int myip;	/* My IP address in host order. */
};

/************************ Global Variables ******************/

extern int oskit_irq_gl;

/*
 * this holds a pointer to the network structure assigned to us.
 */
struct network *oskit_network_ptr_gl = NULL;

/*********************** Local functions *******************/

static inline struct xokpkt * oskit_alloc_xokpkt(struct network *xoknet, 
																 unsigned char *data, 
																 int size);


void oskit_recv_freeFunc(struct xokpkt *pkt)
{
  //  printf("recv_freeFunc called pkt = 0x%x\n", (unsigned) pkt);
  assert(pkt != NULL);
  //  free(pkt->data);
  free(pkt);
}

/*
 * this function assumes the data has already been allocated, and promises to
 * tell the xokpkt to free it correctly.
 */

static inline struct xokpkt * oskit_alloc_xokpkt(struct network *xoknet, unsigned char *data, int size)
{
  int error;
  struct xokpkt *pkt;
  
  assert(xoknet != NULL && data != NULL && size > 0);

  pkt = xokpkt_alloc(0, xoknet->netno, &xoknet->discards, &error);
  if(pkt == NULL)
	 {
		printf("error %d: could not allocate packet on recv\n", error);
		return NULL;
	 }

  pkt->data = data;
  pkt->freeFunc = oskit_recv_freeFunc;
  pkt->freeArg = pkt;
  pkt->len = size;
  
  return pkt;
}


/*
 * The device driver in the oskit calls this function and
 * ands it a buffer containing the packet.
 */
int
oskit_recv(void *data, oskit_bufio_t *b, oskit_size_t pkt_size)
{
  oskit_error_t err;
  unsigned char *frame;

  /*  
   * this is on the interrupt critical path,
   * so speed is important.  I'm not going to 
   * check the arguments.
   */
  assert(data != NULL && b != NULL && pkt_size > 0);
  oskit_network_ptr_gl->intrs++;
  
  err = oskit_bufio_map(b, (void **)&frame, 0, pkt_size);
  
  if( err == 0)
	{
	  struct xokpkt *pkt = oskit_alloc_xokpkt(oskit_network_ptr_gl, frame, pkt_size);
	  if(pkt == NULL)
		 {
			assert(0);
		 }
	  /*
		* perhaps If I put the data i received into the
		* packet before I pass it up, this would work better...
		*/
	  xokpkt_recv (pkt);
	  err = oskit_bufio_unmap(b, (void *)frame, 0, pkt_size);
	  
	}else{
	  printf("oskit_recv: packet recieved but could not map\n");
	  assert(0);
	  return -1;
	}

  return err;
}



struct bufio_extern {
	oskit_bufio_t	ioi;		/* COM I/O Interface */
	unsigned 	count;		/* reference count */
	void		*buf;		/* the buffer */
	oskit_size_t	size;		/* size of buffer */
	void		(*free)(void *cookie, void *buf);	/* free */
	void		*cookie;
};
typedef struct bufio_extern bufio_extern_t;

void free_my_buf(void *cookie, void *buf)
{
  /*
	* Frees the buffer I allocated to do the send and
	* decrements the integer which tells the exokernel
	* the transmission is complete and it can
	* reuse the packet.
	*/
  struct xokpkt *xokpkt = (struct xokpkt *) cookie;
  assert(buf != NULL && cookie != NULL);
  free(buf);
  xokpkt->freeFunc(xokpkt);
}

int oskit_pkt_send(void *cardstruct, struct ae_recv *recv, int xmitintr)
{
#define BUFF_SIZE 1600 /* max size for ethernet packet */
  char *pkt;
  void *mapped_pkt;
  struct xokpkt *incoming_xokpkt = (struct xokpkt *) recv; /*this is a horrible hack... look in readme*/
  int pkt_size;
  oskit_error_t err;
  oskit_bufio_t *buf;
 
  struct etherdev *card_etherdev = (struct etherdev *) cardstruct;
  struct bufio_extern *ext;

  /*
	* check all the parameters
	*/
  if(card_etherdev == NULL || recv == NULL)
	 {
		printf("could not send packet\n");
		return -1;
	 }

  if (card_etherdev->xoknet->inited == 0) {
       printf ("sys_de_xmit: card/link %d not yet inited\n", card_etherdev->xoknet->cardtype);
       return (-1);
    }

  /***************** Send the packet handed to us *******************/
  
  /*
	* allocate send buffer.
	*/
  pkt = (char *) malloc(BUFF_SIZE);
  if(pkt == NULL)
	 {
		/* error, could not malloc a buffer */
		return -1;
	 }

  /*... create a special buffer.*/
  buf = oskit_create_extern_bufio(pkt, BUFF_SIZE,
											 free_my_buf, (void *) incoming_xokpkt
											 );
  assert(buf != NULL);

  /* map packet anyway */
  assert(oskit_bufio_map(buf, (void **) &mapped_pkt, 0, BUFF_SIZE) == 0);
  assert(mapped_pkt == pkt);
  ext = (struct bufio_extern *) buf;

  /*
	* Do the scatter gather ...
	*/

  pkt_size = ae_recv_datacpy(recv, pkt, 0, BUFF_SIZE);
  assert(pkt_size > 0 && pkt_size < BUFF_SIZE);
  /* send the packet */

  if(err = oskit_netio_push(card_etherdev->send_nio, buf, pkt_size) != 0)
	 {
		printf("could not send packet err = %d\n", err);
		delay(1000000);
		assert(0);
	 }

  /* free the buffer */
  if(buf != NULL)
	 {
		oskit_bufio_release(buf);
	 }

  return 0;
}

int oskit_init()
{
  int ndev;
  oskit_error_t err;
  oskit_etherdev_t **etherdev; /* pointer to an array of etherdevs */

  printf("oskit net device drivers init\n");
  oskit_dev_init();
  oskit_linux_init_net();
  //  oskit_dump_drivers();
  printf("oskit probing devices\n");
  oskit_dev_probe();
  // oskit_dump_devices();
  printf("oskit testing for ethernet devs\n");
  ndev = osenv_device_lookup(&oskit_etherdev_iid, (void ***) & etherdev);
  printf("found %d devices etherdev=0x%x\n", ndev, (unsigned)etherdev);
 
  delay(1000000);
  if(ndev > 0)
	 {
		int i;
		/*  
		 * to do the real setup... need to initialize
		 * the network structure in sysinfo.
		 *
		 * use the xoknet_getstructure
		 */
		for(i = 0; i < ndev && i < XOKNET_MAXNETS; i++)
		  {
			 /*
			  * first, get all of the info you need for each card...
			  * like the send and revieve functions and the ethernet addresses.
			  */
			 struct etherdev *new_etherdev = (struct etherdev *) malloc(sizeof(struct etherdev));
			 struct network *new_network;
					  
			 /* zero out structure */
			 bzero(new_etherdev, sizeof(struct etherdev));
			 if((err = oskit_etherdev_getinfo(etherdev[i], &new_etherdev->info)) != 0)
				{
				  panic(" oskit_etherdev_getinfo failed!!\n");
				}
			 
			 oskit_etherdev_getaddr(etherdev[i], new_etherdev->haddr);
			 printf("  %s %s Ethernet adder = %x:%x:%x:%x:%x:%x \n", new_etherdev->info.name,
					  new_etherdev->info.description
					  ? new_etherdev->info.description : "", 
					  new_etherdev->haddr[0],
					  new_etherdev->haddr[1],
					  new_etherdev->haddr[2],
					  new_etherdev->haddr[3],
					  new_etherdev->haddr[4],
					  new_etherdev->haddr[5]
					  );
  
			 new_etherdev->dev = etherdev[i];
			 
			 new_etherdev->recv_nio = oskit_netio_create(oskit_recv, &new_etherdev);
			 if (new_etherdev->recv_nio == NULL)
				{
				  panic("unable to create recv_nio\n");
				}
			 
			 /* make the card ready for operation */
			 if((err = oskit_etherdev_open(etherdev[i], 0, new_etherdev->recv_nio,
													 &new_etherdev->send_nio)) != 0)
				{
				  panic(" oskit_etherdev_open failed... exit\n");
				}
			 
			 /* add the newly activated card to the system network structure */
			 new_network = xoknet_getstructure (XOKNET_OSKIT, 
															oskit_pkt_send,
															new_etherdev);
			 /* must set global here... */
			 oskit_network_ptr_gl = new_network;
			 /*hopefully this will work*/
			 new_etherdev->xoknet = new_network;
			 
			 /*
			  * set the irq, if the osenv_irq_alloc function has not
			  * taken care of it
			  */
			 
			 if(oskit_irq_gl != -1)
				{
				  new_network->irq = oskit_irq_gl;
				  printf("set xoknet->irq to %d with oskit_irq_gl\n", oskit_irq_gl);
				}
			 /* set up some more fields .. see if irq is setting up handler correctly*/
			 //done in osenv_intr_imp.c			 new_network->irq = 9;  /*tmp.. fix this */
 
			 memcpy(new_network->ether_addr, new_etherdev->haddr, ETHER_ADDR_LEN);
			 strncpy(new_network->cardname, "oskit_de0", XOKNET_NAMEMAX);
			 new_network->inited = 1;
			 printf("--- added network card %s as %s\n", new_network->cardname,
					  new_network->cardname);
			 
			 delay(1000000);
		  }
	 }
  return 0;
}

