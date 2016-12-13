/* Author(s): <Your name here>
 * Implementation of the memory manager for the kernel.
*/

/* memory.c
 *
 * Note:
 * There is no separate swap area. When a data page is swapped out,
 * it is stored in the location it was loaded from in the process'
 * image. This means it's impossible to start two processes from the
 * same image without screwing up the running. It also means the
 * disk image is read once. And that we cannot use the program disk.
 *
 */

#include "common.h"
#include "kernel.h"
#include "scheduler.h"
#include "memory.h"
#include "thread.h"
#include "util.h"
#include "interrupt.h"
#include "tlb.h"
#include "usb/scsi.h"

/* Static global variables */
// Keep track of all pages: their vaddr, status, and other properties
static page_map_entry_t page_map[PAGEABLE_PAGES];

// address of the kernel page directory (shared by all kernel threads)
static uint32_t *kernel_pdir;

// allocate the kernel page tables
static uint32_t *kernel_ptabs[N_KERNEL_PTS];


//other global variables...
static page_map_entry_t *page_map_pointer;

/* Main API */

/* Use virtual address to get index in page directory. */
uint32_t get_dir_idx(uint32_t vaddr){
  return (vaddr & PAGE_DIRECTORY_MASK) >> PAGE_DIRECTORY_BITS;
}

/* Use virtual address to get index in a page table. */
uint32_t get_tab_idx(uint32_t vaddr){
  return (vaddr & PAGE_TABLE_MASK) >> PAGE_TABLE_BITS;
}

/* TODO: Returns physical address of page number i */
uint32_t* page_addr(int i){

}

/* Set flags in a page table entry to 'mode' */
void set_ptab_entry_flags(uint32_t * pdir, uint32_t vaddr, uint32_t mode){
  uint32_t dir_idx = get_dir_idx((uint32_t) vaddr);
  uint32_t tab_idx = get_tab_idx((uint32_t) vaddr);
  uint32_t dir_entry;
  uint32_t *tab;
  uint32_t entry;

  dir_entry = pdir[dir_idx];
  ASSERT(dir_entry & PE_P); /* dir entry present */
  tab = (uint32_t *) (dir_entry & PE_BASE_ADDR_MASK);
  /* clear table[index] bits 11..0 */
  entry = tab[tab_idx] & PE_BASE_ADDR_MASK;

  /* set table[index] bits 11..0 */
  entry |= mode & ~PE_BASE_ADDR_MASK;
  tab[tab_idx] = entry;

  /* Flush TLB because we just changed a page table entry in memory */
  flush_tlb_entry(vaddr);
}

/* Initialize a page table entry
 *
 * 'vaddr' is the virtual address which is mapped to the physical
 * address 'paddr'. 'mode' sets bit [12..0] in the page table entry.
 *
 * If user is nonzero, the page is mapped as accessible from a user
 * application.
 */
void init_ptab_entry(uint32_t * table, uint32_t vaddr, uint32_t paddr, uint32_t mode){
  int index = get_tab_idx(vaddr);
  table[index] = (paddr & PE_BASE_ADDR_MASK) | (mode & ~PE_BASE_ADDR_MASK);
  flush_tlb_entry(vaddr);
}

/* Insert a page table entry into the page directory.
 *
 * 'mode' sets bit [12..0] in the page table entry.
 */
void insert_ptab_dir(uint32_t * dir, uint32_t *tab, uint32_t vaddr, uint32_t mode){

  uint32_t access = mode & MODE_MASK;
  int idx = get_dir_idx(vaddr);

  dir[idx] = ((uint32_t)tab & PE_BASE_ADDR_MASK) | access;
}

/* TODO: Allocate a page. Return page index in the page_map directory.
 *
 * Marks page as pinned if pinned == TRUE.
 * Swap out a page if no space is available.
 */
int page_alloc(int pinned){
  // define some local needed variables

  // find an availabe physical page

  // initialize a physical page (wirte infomation to page_map)

  // zero-out the process page

//   return free_index;
}

/* TODO: Set up kernel memory for kernel threads to run.
 *
 * This method is only called once by _start() in kernel.c, and is only
 * supposed to set up the page directory and page tables for the kernel.
 */
void init_memory(void){
	// initialize all pageable pages to a default state
	page_map_entry_t *pm;
	int i;
	for (i = 0; i < PAGEABLE_PAGES; i++)
	{
		pm = &page_map[i];
		pm->used = FALSE;
		pm->previous = pm;
		pm->next = pm;
		pm->paddr = (uint32_t *)(MEM_START + PAGE_SIZE * i);
	}

	// pin one page for the kernel page directory
	pm = &page_map[0];
	pm->used = TRUE;
	pm->pinned = TRUE;
	pm->previous = pm;
	pm->next = pm;
	pm->vaddr = 0;
	page_map_pointer = pm;
	// zero-out the kernel page directory
	for (i = 0; i < N_KERNEL_PTS; i++)
		insert_ptab_dir(pm->paddr, page_map[N_KERNEL_PTS + i].paddr, 0, PE_P);
		// *(pm->paddr + 4 * i) = (uint32_t)(page_map[N_KERNEL_PTS + i].paddr) & PAGE_DIRECTORY_MASK & PE_P;
	for (; i < PAGE_N_ENTRIES; i++)
		pm->paddr[i] = 0;
	// pin N_KERNEL_PTS pages for kernel page tables
	for (i = 0; i < N_KERNEL_PTS; i++)
	{
		pm = &page_map[1 + i];
		pm->used = TRUE;
		pm->pinned = TRUE;
		pm->previous = page_map_pointer->previous;
		page_map_pointer->previous = pm;
		pm->next = page_map_pointer;
		pm->vaddr = 0;
		// initialize the page table
		int j;
		for (j = 0; (PAGE_N_ENTRIES * i + j) < (MEM_START / PAGE_SIZE); j++)
			init_ptab_entry(pm->paddr, (PAGE_SIZE * PAGE_N_ENTRIES * i + PAGE_SIZE * j), (PAGE_SIZE * PAGE_N_ENTRIES * i + PAGE_SIZE * j), PE_P);
			// *(pm->paddr + 4 * j) = (uint32_t)(PAGE_SIZE * j) & PAGE_TABLE_MASK & PE_P;
		page_map_pointer = pm;
	}
}

/* TODO: Set up a page directory and page table for a new
 * user process or thread. */
void setup_page_table(pcb_t * p){
	// special case for thread virtual memory setup
	if (p->is_thread)
	{
		p->page_directory = page_map[0].paddr;
	}
	else
	{
		ASSERT2(0, "you get to here..........");
		page_map_entry_t *pm_dir, *pm_tab;
		int i;
		for (i = 2; i < PAGEABLE_PAGES; i++)
			if (!page_map[i].used)
			{
				pm_dir = &page_map[i];
				break;
			}
		for (; i < PAGEABLE_PAGES; i++)
			if (!page_map[i].used)
			{
				pm_tab = &page_map[i];
				break;
			}
		ASSERT2(i < PAGEABLE_PAGES, "Out of pageable pages");

		pm_dir->used = TRUE;
		pm_dir->previous = page_map_pointer->previous;
		page_map_pointer->previous = pm_dir;
		pm_dir->next = page_map_pointer;
		pm_dir->pinned = TRUE;
		for (i = 0; i < PAGE_N_ENTRIES; i++)
			pm_dir->paddr[i] = 0;
		insert_ptab_dir(pm_dir->paddr, pm_tab->paddr, p->base_kernel_stack, PE_P);
		page_map_pointer = pm_dir;

		pm_tab->used = TRUE;
		pm_tab->previous = page_map_pointer->previous;
		page_map_pointer->previous = pm_tab;
		pm_tab->next = page_map_pointer;
		pm_tab->pinned = TRUE;
	}
}

/* TODO: Swap into a free page upon a page fault.
 * This method is called from interrupt.c: exception_14().
 * Should handle demand paging.
 */
void page_fault_handler(void){
}

/* Get the sector number on disk of a process image
 * Used for page swapping. */
int get_disk_sector(page_map_entry_t * page){
  // return page->swap_loc + ((page->vaddr - PROCESS_START) / PAGE_SIZE) * SECTORS_PER_PAGE;
}

/* TODO: Swap from disk into the i-th page using fault address and swap_loc of current running */
void page_swap_in(int i){
}

uint32_t get_ptab_entry(uint32_t * pdir, uint32_t vaddr) {
  uint32_t dir_idx = get_dir_idx((uint32_t) vaddr);
  uint32_t tab_idx = get_tab_idx((uint32_t) vaddr);
  uint32_t dir_entry;
  uint32_t *tab;
  uint32_t entry;

  dir_entry = pdir[dir_idx];

  tab = (uint32_t *) (dir_entry & PE_BASE_ADDR_MASK);

  return tab[tab_idx];
}

/* TODO: Swap i-th page out to disk.
 *
 * Write the page back to the process image.
 * There is no separate swap space on the USB.
 *
 */
void page_swap_out(int i){

}

/* TODO: Decide which page to replace, return the page number  */
int page_replacement_policy(void){
}

