/* Routines to allocate/deallocate integers in range [0..DPF_MAXFILTS) */
#include <dpf/dpf-internal.h>

struct pids {
	struct pids *next;
} pids[DPF_MAXFILT], *free_pids;

/* Highest pid yet allocated. */
static int highwater;

int dpf_allocpid(void) {
	if(!free_pids) {
		return (highwater >= DPF_MAXFILT) ? -1 : highwater++;
	} else {
		struct pids *p;
		p = free_pids;
		free_pids = free_pids->next;
		return p - &pids[0];
	}
}

void dpf_freepid(int pid) {
	struct pids *p;

	p =  &pids[pid];
	p->next = free_pids;
	free_pids = p;
}
