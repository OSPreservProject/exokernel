/*
 * ExoKernel include files
 */

#include <xok/types.h>
#include <xok/printf.h> 
#include <xok/defs.h>
#include <xok/pmap.h>
#include <xok_include/assert.h>
#include <xok_include/errno.h>
#include <xok/sysinfo.h>
#include <xok/pkt.h>
#include <xok/bios32.h>
#include <machine/endian.h>
#include <xok/ae_recv.h>
#include <xok/sys_proto.h>
#include <xok_include/string.h>
#include <xok/trap.h>
#include <xok/cpu.h>
#include <xok/sys_syms.h>
#include <xok/mmu.h>
#include <xok/cpu.h>

/* added so it could find kva2pp  , ECP 1/25  */
#include <xok/pmapP.h>


/*
 * Oskit include files 
 */
#include <dev/dev.h>
#include <oskit/machine/types.h>
#include <oskit/types.h>
#include <oskit/error.h>
#include <dev/device.h>

/*
 * Some local functions.
 */
static void *allocate_aligned_mem(oskit_size_t size, unsigned align); 
static inline unsigned int mod_power_of_2(unsigned int num, int pow_of_2);
static unsigned int align_num(unsigned int pointer, unsigned int byte_boundary);
static inline int pa_out_of_range(u_long pa);

/*
 * External global variables
 */

extern int booting_up;

/* ********************************** Local functions ************************/

static inline
int pa_out_of_range(u_long pa)
{
  u_long ppn = (u_long) (pa) >> PGSHIFT;	
  if (ppn >= nppage)						
	{
	  return 1;
	}else{
	  return 0;
	}
}


static inline
unsigned int mod_power_of_2(unsigned int num, int pow_of_2)
{
  /* not sure if this will work with negatives. */
  return num & (pow_of_2 - 1);
}  



static unsigned int align_num(unsigned int pointer, 
							  unsigned int byte_boundary)
{
  /* 
   * alignes a pointer or number with the specified byte boundary.  
   * byte_boundry must be a power of two, and if the 
   * pointer is already on the correct byte boundry,
   * no change is made.
   */
  
  if(mod_power_of_2((unsigned int) pointer, byte_boundary) == 0)
	{
	  /* the pointer is already aligned, so return it. */
	  return pointer;
	}
  else
	{
	  (unsigned int) pointer += (byte_boundary - mod_power_of_2((unsigned int) pointer, 
																byte_boundary));
	  return pointer;
	}
}


static
void *allocate_aligned_mem(oskit_size_t size, /* the size of the memory to be allocated */
						   unsigned align)    /* the byte boundary to align it on.	*/
{
  /* 
   * This routine returns a pointer to newly allocated 
   * memory of size, size, and aligned on byte boundary align.
   */
	 
  if(align < 2)
	{
	  /*
	   * no alignment necessary.
	   */
	  return malloc(size);
	}
  else
	{
	  /*
	   * must return memory aligned on the 'align' byte
	   * boudry.  If align is 4, then the memory must
	   * be aligned on a four byte boundry.
	   */
	  void *mem = malloc(size + (align - 1));
	  if(mem != NULL)
		{
		  mem = (void *) align_num((unsigned int) mem, align);
		  return mem;
		}
	  else
		{
		  return NULL;
		}
	}
}

static
void *allocate_aligned_contiguous_mem(size, align)
{
  /* 
   * I think the exokernel tries to allocate aligned pages..
   * I guess I'll find out.
   */
  return allocate_aligned_mem(size, align);
}

#define SIXTY_FOUR_KB 64 * 1024

void *osenv_mem_alloc(oskit_size_t size, 
		      osenv_memflags_t flags,
		      unsigned align)
{
  /*
   * Allocate a block of memory that will be aligned on
   * align byte boundaries (2, 4, 8, 16 bytes... etc. or 1 or zero
   * meaning that the memory can be aligned in any fashion.
   *
   * Also, there are a number of flags that can be set.  I am assuming
   * that a regulare call to malloc in the exokernel covers the following
   * properties.  
   * 1. it keeps track of the size automatically, 
   * 2. it is non-blocking. 
   */

  void *memory = NULL;			/* pointer to newly allocated memory */
  /* printf("inside osenv_malloc(size = %d, fl=%d, algn=%d)\n",
	 size, flags, align);*/
  if(flags & OSENV_ISADMA_MEM)
	{
	  unsigned int min_boundary_for_size;
   
	  /*
	   * check size to make sure its
	   * less than 64KB.
	   */
	  if(size >= SIXTY_FOUR_KB) 
		{
		  printf("Error, osenv_mem_alloc: tried to allocate %d bytes when ", size);
		  printf("%d bytes is the max for ISADMA allocation\n", SIXTY_FOUR_KB);
		  return NULL;
		}

	  /*
	   * to make sure the allocated memory does not cross
	   * a 64KByte boundary I'm going to make sure its aligned
	   * on a byte that is greater than its size.  there is 
	   * a better way to do this, but I'll work on it later.
	   */

	  /*align size to its closest power of 2 */
	  min_boundary_for_size = align_num(size, 2);

	  /* see which alignment request is greater, 
	   * use that one. 
	   */
	  if(min_boundary_for_size < align)
		{
		  min_boundary_for_size = align;
		}
	  memory = allocate_aligned_contiguous_mem(size, min_boundary_for_size);
	}
  else if(flags & OSENV_PHYS_CONTIG)
	{
	  memory = allocate_aligned_contiguous_mem(size, align);
	}
  else
	{
	  /*
	   * normal case, no flags other than PHYS_WIRED or
	   * an alignment.
	   */
	  memory = allocate_aligned_mem(size, align);
	}

  /*
   * once memory has been allocated it may need to be pinned.
   * first, we must check and see if the allocation was successful.
   */

  if(memory != NULL)
	{
	  if(flags & OSENV_PHYS_WIRED)
		{
		  void *page_index; /* points to the pages to be pinned */
		  
		  /*
		   * pin all of the pages that were allocated.
		   */
		  for(page_index = memory;
			  (unsigned int) page_index < size;
			  page_index += NBPG)
			{
			  struct Ppage *pp = kva2pp((long)page_index);
			  ppage_pin(pp);
			}
		}
	}
  else
	{
	  /*
	   * Error, memory allocation failed.
	   */
	  printf("malloc failed\n");
	  return NULL;
	}

  return memory;
}

void osenv_mem_free(void *block, osenv_memflags_t flags, oskit_size_t size)
{
  /* 
   * Free a chunk of memory.
   * I assume the exokernel's malloc and free keep track of the sizes and
   * that pinned pages will be freed by a call to free without unpinning them.
   */

  free(block);
}

oskit_addr_t			/*returns the valid physical address*/
find_va_mapping(
					 oskit_addr_t va /*give a kernel virtual address*/
					 )
{
  /*
	* walks the kernel's page table looking for
	* the correct physical address
	*/
  oskit_addr_t pa;
  Pde pde = (unsigned) vpd[PDENO(va)];;
  
  /*
	* check pde and see if it is a 4Meg or 4KB page and
	* then do a table walk based on that
	*/

  if((pde & 8) == 8) 
	 {
		/* walk 4 Meg page table, only one step*/
		unsigned offset = va & PDMASK;
		pa = (pde & ~PDMASK) + offset;
	 }else{
		/* walk 4k page directory and page table */
		Pte *pagetable_va = (Pte *) (pa2kva(pde & ~PGMASK));
		unsigned pte_index = ((va >> PGSHIFT) & (NLPG - 1)) << 2;

		Pte pte = *(Pte *) ((unsigned) pagetable_va + pte_index);

		unsigned offset = va & PGMASK;
		
		pa = (pte & ~PGMASK) | offset;
	 }

  return pa;
}

oskit_addr_t osenv_mem_get_phys(oskit_addr_t va)
{
  /* 
   * convert a virtual address. to a physical address 
   */
  if((u_long) va < KERNBASE)
	 {
		/* va out of range, must walk page table*/
		return find_va_mapping(va);
	 }else{
		return kva2pa(va);
	 }
}

oskit_addr_t osenv_mem_get_virt(oskit_addr_t pa)
{
  /* 
   * change the physical address to a kernel virtual address.
	* watch for new mappings.
   */
  if(!pa_out_of_range(pa))
	 {
		return pa2kva(pa);
	 }else{
		/* here we would have to check the page table */
		assert(0);
		return 0;
	 }
}

oskit_addr_t osenv_mem_phys_max(void)
{
  /* 
   * returns the amount of physical memory, 
   */
  return (nppage << PGSHIFT);
}

/* 
 * returns the address of the next free page table
 * entry.
 */
u32 *get_next_pte()
{
  
  return 0;
}


oskit_addr_t virt_mem_top()
{
  /*
	* returns the highest address in use for virtual memory
	*/
  static int first_time = 1;
  static uint top = 0;
  uint current_top;
  if(first_time)
	 {
		top = osenv_mem_phys_max();
		first_time = 0;
	 }else{
		top += NBPG;
	 }

  current_top = top + KERNBASE;
  return current_top;
}

int osenv_mem_map_phys(oskit_addr_t pa, oskit_size_t size, void **addr, int flags)
{
  /* 
   * This function should look at the address and determine whether or not it is out
   * of kernel space.  If it is out of kernel space... like if it is a high 
   * address like 0xff000000 physical... then it should be remapped.
   */
  
  if(pa_out_of_range(pa))
	{
	  if((8 & 8) == 8)  //mseu need to fix this
		 {			
			/* 
			 * this is a pentium, so the exokernel is
			 * using the large memory map. 4meg pages
			 */
			unsigned *pgtble = (unsigned *) vpd_H_ADDR;
			
			printf("pgtble = 0x%x, first entry = 0x%x\n", (unsigned) pgtble, (unsigned) *pgtble);
			delay(1000000);
			
			if(1) //map_pa(pa, addr))
			  {
				 /* see if it is there, check top 10 bits*/
				 /* I'm going to map addresses starting at top */
				 /*
				  * p	     (bit 0) = 1, page is present 
				  * r/w      (bit 1) = 1, readable, writeable
				  * u/s      (bit 2) = 0, page is supervisor level
				  * PWT      (bit 3) = 1, write through or write back...
				  *
				  * PCD      (bit 4) = 1, Page level cache disable... caching disabled
				  * accessed (bit 5) = 0, not accessed
				  * dirty    (bit 6) = 0, not dirty
				  * size     (bit 7) = 1, 4MB page size
				  *
				  * global   (bit 8) = 1, page is global because we are in kernel
				  */

				 int flags = 0x0000019b;
				 printf("setting pgtble[%d] = 0x%x now = 0x%x \n", 
						  ((virt_mem_top() & 0xff600000) >> 22), 
						  pgtble[(virt_mem_top() & 0xff600000) >> 22],
						  (pa & 0xff600000) | flags);
				 delay(5000000);
				 
				 if(pgtble[(virt_mem_top() & 0xff600000) >> 22] == 0)
					{
					  uint page_num = (virt_mem_top() & 0xff600000) >> 22;
					  printf("doing mapping\n");
					  pgtble[page_num] = (pa & 0xff600000) | flags;
					  delay(100000);
					  *addr = (void *) ((page_num << 22) | (pa & 0x003fffff));
					}
			  }
		 }else{
			/* not a pentium, PPro, or PII. */
			assert(0);
		 }
	}else{
	  *addr =  (void *)pa2kva(pa);
	} 
  return 0;
}
