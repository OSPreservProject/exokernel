#include <xok/sys_ucall.h>
#include <xok/sysinfo.h>
#include <xok/mmu.h>
#include <xok/pmap.h>
#include <exos/cap.h>
#include <stdio.h>
#include <stdlib.h>

int main(int ac, char **av)
{
	unsigned long	start_tick = __sysinfo.si_system_ticks,
			i,
			start = 0;
	int		err = 0,
			eid;
	if (ac < 2)
		eid = sys_geteid();
	else
		eid = atoi(av[1]);

	sys_capdump();

	printf("memory map for environment %u\n", eid);

	for (i = 0; i <= (1 << 19) + ((1 << 19) - 1); i++) {
		err = 0;
		sys_read_pte(i<<12, CAP_ROOT, eid, &err);
		if (err < 0) {
			if (i && start != -1) {
				printf("0x%08lX .. 0x%08lX : 0x%08lX bytes\n",
					start, ((i+1)<<12) - 1, ((i+1)<<12) - start);
			}
			start = -1;
		} else if (start == -1)
			start = i << 12;
	}
	if (start != -1)
		printf("0x%08lX .. 0xFFFFFFFF : 0x%08lX bytes\n",
			start, (0xFFFFFFFF - start) + 1);

	printf("\n%u ticks, (MHz %u, usec/tick %u), \n",
		__sysinfo.si_system_ticks - start_tick,
		__sysinfo.si_mhz, __sysinfo.si_rate);

	return 0;
}
