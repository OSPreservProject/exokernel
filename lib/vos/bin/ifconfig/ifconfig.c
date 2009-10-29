
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <xok/mmu.h>
#include <xok/sys_ucall.h>
#include <vos/cap.h>
#include <vos/vm.h>
#include <vos/vm-layout.h>
#include <vos/uinfo.h>
#include <vos/errno.h>
#include <vos/net/iptable.h>


/* ifconfig
 *
 * a simple ifconfig program
 */

#define dprintf if (0) kprintf


static int
parse_ipaddr(char *str, ipaddr_t *ip)
{
  int n;
  char *sn;
  char *end;

  sn = strtok(str,".\0");
  if (!sn)
    return -1;
  n = strtol(sn,&end,10);
  if (*end != '\0')
    return -1;
  (*ip)[0] = n;

  sn = strtok(NULL,".\0");
  if (!sn) 
    return -1;
  n = strtol(sn,&end,10);
  if (*end != '\0')
    return -1;
  (*ip)[1] = n;

  sn = strtok(NULL,".\0");
  if (!sn) 
    return -1;
  n = strtol(sn,&end,10);
  if (*end != '\0')
    return -1;
  (*ip)[2] = n;

  sn = strtok(NULL,"\0");
  if (!sn) 
    return -1;
  n = strtol(sn,&end,10);
  if (*end != '\0')
    return -1;
  (*ip)[3] = n;

  return 0;
}



static void
usage(char *prog)
{
  printf("%s: usage: %s interface up/down [ip aa.bb.cc.dd] [mask aa.bb.cc.dd] [mtu NN]\n",prog,prog);
}


static int 
parse_cmd(int argc, char *argv[])
{
  if_entry_t *ife;
  int i;

  if (argc == 1)
  {
    for(i = 0; i < ip_table->num_if_entries; i++)
    {
      ife = IPTABLE_GETIFP(i);
      if (ife->flags != IF_FREE)
	print_ife(ife);
    }

    return 0;
  }

  if (argc < 3 || (argc != 3 && argc != 5 && argc != 7 && argc != 9))
  {
    usage(argv[0]);
    return -1;
  }

  yieldlock_acquire(&(ip_table->if_lock));

  if ((i = iptable_find_if_name(argv[1])) < 0)
  {
    printf("%s: interface %s not present on this machine\n", argv[0], argv[1]);
    yieldlock_release(&(ip_table->if_lock));
    return -1;
  }

  ife = IPTABLE_GETIFP(i);
  // print_ife(ife);

  if (strncmp(argv[2], "up", 2)==0)
  {
    ip_table->if_stamp++;  /* start modification */
    ife->flags = ife->flags & ~IF_DOWN;
    ife->flags = ife->flags | IF_UP;
  }
  else if (strncmp(argv[2], "down", 4)==0)
  {
    ip_table->if_stamp++;  /* start modification */
    ife->flags = ife->flags & ~IF_UP;
    ife->flags = ife->flags | IF_DOWN;
    ip_table->if_stamp++;  /* end modification */

    yieldlock_release(&(ip_table->if_lock));
    return 0;
  }
  else
  {
    dprintf("%s\n",argv[2]);
    usage(argv[0]);
   
    yieldlock_release(&(ip_table->if_lock));
    return -1;
  }

  for(i=3;i<argc;i+=2)
  {
    if (strncmp(argv[i],"ip",2)==0)
    {
      ipaddr_t ip;
      if (parse_ipaddr(argv[i+1], &ip) < 0)
      {
        dprintf("%s\n",argv[i+1]);
	usage(argv[0]);
    
        ip_table->if_stamp++;  /* end modification */
	yieldlock_release(&(ip_table->if_lock));
	return -1;
      }
      bcopy(ip,ife->ipaddr,4);
      apply_netmask_net(ife->net, ife->ipaddr, ife->netmask);
    }
    
    else if (strncmp(argv[i],"mask",4)==0)
    {
      ipaddr_t mask;
      if (parse_ipaddr(argv[i+1], &mask) < 0)
      {
        dprintf("%s\n",argv[i+1]);
	usage(argv[0]);

        ip_table->if_stamp++;  /* end modification */
	yieldlock_release(&(ip_table->if_lock));
	return -1;
      }
      bcopy(mask,ife->netmask,4);
      apply_netmask_net(ife->net, ife->ipaddr, ife->netmask);
    }

    else if (strncmp(argv[i],"mtu",3)==0)
    {
      char *end;
      int mtu = strtol(argv[i+1],&end,10);
      if (*end != '\0')
      {
	usage(argv[0]);

        ip_table->if_stamp++;  /* end modification */
	yieldlock_release(&(ip_table->if_lock));
	return -1;
      }
      ife->mtu = mtu;
    }
  }
  
  print_ife(ife);
  
  ip_table->if_stamp++;  /* end modification */
  yieldlock_release(&(ip_table->if_lock));
  return 0;
}


int 
main(int argc, char *argv[])
{
  iptable_remap();

  parse_cmd(argc, argv);
  return 0;
}


