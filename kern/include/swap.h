#ifndef _SWAP_H_
#define _SWAP_H_

// This function init the swapping functionality of the system (Open swap file and such)
void swap_init(void);

// Find how many page we have left in swapfile
unsigned int swap_get_avail_page_count(void);

// Load a page from swap file to memory
void swap_load_page(paddr_t paddr, unsigned int file_frame, struct addrspace *as, unsigned int pt_index, unsigned int ref_count);

// Store a page from memory to swap file
void swap_store_page(paddr_t paddr, unsigned int file_frame);

// Allocate swap page
unsigned int swap_alloc_page(void);

// Free swap page
void swap_free_page(unsigned int file_frame);

// Evict page(Remove a page out of memory and put it into swap) return 0 if success
unsigned int swap_evict(void);

// Evict a specific page return 0 of success
unsigned int swap_evict_specific(unsigned int pframe);

#endif
