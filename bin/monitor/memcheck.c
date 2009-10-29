#include "types.h"
#include "debug.h"
#include "memoryemu.h"
#include "emu.h"    /* for config struct */
#include "disks.h"  /* for SECTOR_SIZE */
/*
 * The point of these procedures is going to be to create a map of the
 * available memory to facilitate:
 *
 * 1.  check for memory conflicts (hardware page/ems page frame/etc.)
 * 2.  facilitate searching for frames (EMS frame, UMB blocks, etc.)
 */

#define PAGE_SIZE       SECTOR_SIZE    /* granularity we care about */
#define MEM_SIZE    (256*1024)        /* config.mem_size    only low mem for now */
#define MAX_PAGE (MEM_SIZE/PAGE_SIZE)

static unsigned char mem_map[MAX_PAGE]; /* Map of memory contents */
static unsigned int  mem_aux[MAX_PAGE];
static char *mem_names[256];	/* List of id. strings         */

static inline void round_addr(int *addr)
{
    *addr = (*addr + PAGE_SIZE - 1) / PAGE_SIZE;
    *addr *= PAGE_SIZE;
}

int memcheck_addtype(unsigned char map_char, char *name)
{
    if (mem_names[map_char] != NULL) {
	if (strcmp(mem_names[map_char], name)) {
	    debug("memcheck, conflicting map type '%c' defined for '%s' & '%s'\n",
		  map_char, mem_names[map_char], name);
	    leaveemu(ERR_MEM);
	} else
	    debug("memcheck, map type '%c' re-defined for '%s'\n",
		     map_char, name);
    }

    mem_names[map_char] = name;

    return 0;
}

void memcheck_reserve_aux(unsigned char map_char, int addr_start, int size, u_int aux)
{
    int cntr, addr_end;

    round_addr(&addr_start);
    addr_end = addr_start + size;
    round_addr(&addr_end);

    for (cntr = addr_start / PAGE_SIZE; cntr<MAX_PAGE && cntr < addr_end / PAGE_SIZE; cntr++) {
	if (mem_map[cntr]) {
	    if (mem_map[cntr] == map_char) {
		/* error("Possible error.  The memory type '%s' has been mapped twice to the same location (0x%4.4XF:0x0000)\n", */
/* 		      mem_names[map_char], (cntr * PAGE_SIZE) / 16); */
	    } else {
		dprintf("memcheck fatal error.  Memory conflict!\n");
		dprintf("    Memory at %4.4X:0000 is mapped to both:\n",
			(cntr * PAGE_SIZE) / 16);
		dprintf("    '%s' & '%s'\n", mem_names[map_char],
			mem_names[mem_map[cntr]]);
		memcheck_dump();
		leaveemu(ERR_MEM);
	    }
	} else {
	    mem_map[cntr] = map_char;
	    mem_aux[cntr] = aux;
	}
    }
}

void memcheck_reserve(unsigned char map_char, int addr_start, int size)
{
    memcheck_reserve_aux(map_char, addr_start, size, (u_int)-1);
}


void memcheck_type_init()
{
    memcheck_addtype('d', "Base DOS memory (first 640K)");
    memcheck_addtype('r', "Dosemu reserved area");
    memcheck_addtype('h', "Direct-mapped hardware page frame");
    memcheck_addtype('D', "blocks mapped to disk");
}

void memcheck_init()
{
    int i;
    for(i=0; i<MAX_PAGE; i++)
	mem_aux[i] = (u_int)-1;
    memcheck_type_init();
    /* memcheck_reserve('d', 0x00000, config.mem_size * 1024); */	/* dos memory  */
    memcheck_reserve('r', 0xF0000, 0x10000);	/* dosemu bios */
}

int memcheck_isfree(int addr_start, int size)
{
    int cntr, addr_end;

    round_addr(&addr_start);
    addr_end = addr_start + size;
    round_addr(&addr_end);

    for (cntr = addr_start / PAGE_SIZE; cntr < addr_end / PAGE_SIZE; cntr++) {
	if (mem_map[cntr])
	    return 0;
    }
    return 1;
}

int memcheck_findhole(int *start_addr, int min_size, int max_size)
{
    int cntr;

    round_addr(start_addr);

    for (cntr = *start_addr / PAGE_SIZE; cntr < MAX_PAGE; cntr++) {
	int cntr2, end_page;

	/* any chance of finding anything? */
	if ((MAX_PAGE - cntr) * PAGE_SIZE < min_size)
	    return 0;

	/* if something's already there, no go */
	if (mem_map[cntr])
	    continue;

	end_page = cntr + (max_size / PAGE_SIZE);
	if (end_page > MAX_PAGE)
	    end_page = MAX_PAGE;

	for (cntr2 = cntr + 1; cntr2 < end_page; cntr2++) {
	    if (mem_map[cntr2]) {
		if ((cntr2 - cntr) * PAGE_SIZE >= min_size) {
		    *start_addr = cntr * PAGE_SIZE;
		    return ((cntr2 - cntr) * PAGE_SIZE);
		} else {
		    /* hole isn't big enough, skip to the next one */
		    cntr = cntr2;
		    break;
		}
	    }
	}
    }
    return 0;
}

void memcheck_dump(void)
{
    int cntr;
    debug("Memory map dump:\n");
    for (cntr = 0; cntr < MAX_PAGE; cntr++) {
	if (cntr % 64 == 0)
	    debug("0x%5.5X:  ", cntr * PAGE_SIZE);
	if (mem_aux[cntr] != (u_int)-1) {
	    if (mem_aux[cntr] > 0xf) {
		debug("+");
	    } else {
		debug("%x", mem_aux[cntr]);
	    }
	} else {
	    debug("%c", (mem_map[cntr]) ? mem_map[cntr] : '.');
	}
	if (cntr % 64 == 63)
	    debug("\n");
    }
    debug("\nKey:\n");
    for (cntr = 0; cntr < 256; cntr++) {
	if (mem_names[cntr])
	    debug("%c:  %s\n", cntr, mem_names[cntr]);
    }
    debug(".:  (unused)\n");
    debug("End dump\n");
}
