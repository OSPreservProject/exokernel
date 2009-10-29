/*
 * getether.c : get the ethernet address of an interface
 *
 * All of this code is quite system-specific.  As you may well
 * guess, it took a good bit of detective work to figure out!
 *
 * If you figure out how to do this on another system,
 * please let me know.  <gwr@mc.com>
 */

#include <exos/net/if.h>
int
getether(char *ifname, char *eaddr)
{
  int ifnum;
#define FIRST_TRY "ed0"
#define SECOND_TRY "de0"
  /* try two interfaces: ed0, or de0 */
  if ((ifnum = get_ifnum_by_name(FIRST_TRY))) {
	strcpy(ifname,FIRST_TRY);
	get_ifnum_ethernet(ifnum,eaddr);
	return 0;
  } else if ((ifnum = get_ifnum_by_name(SECOND_TRY))) {
	strcpy(ifname,SECOND_TRY);
	get_ifnum_ethernet(ifnum,eaddr);
	return 0;
  }
  return -1;
}

/*
 * Local Variables:
 * tab-width: 4
 * c-indent-level: 4
 * c-argdecl-indent: 4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: -4
 * c-label-offset: -4
 * c-brace-offset: 0
 * End:
 */
