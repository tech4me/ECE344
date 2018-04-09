#ifndef _SWAP_H_
#define _SWAP_H_

#include <addrspace.h>

extern struct lock *swap_lock;

// This function init the swapping functionality of the system (Open swap file and such)
void swap_init(void);

// Find how many page we have left in swapfile
unsigned int swap_get_avail_page_count(void);

// Load a page from swap file to memory
void swap_load_page(paddr_t paddr, unsigned int file_frame, struct page_table_entry *pte);

void swap_load_page_without_free(paddr_t paddr, unsigned int file_frame, struct page_table_entry *pte);

// Store a page from memory to swap file
void swap_store_page(paddr_t paddr, unsigned int file_frame);

// Allocate swap page
unsigned int swap_alloc_page(void);

// Free swap page
void swap_free_page(unsigned int file_frame);

// Evict page(Remove a page out of memory and put it into swap) return 0 if success
unsigned int swap_evict(void);

// Evict page(Remove a page out of memory and put it into swap) return 0 if success, avoid a page
unsigned int swap_evict_avoidance(unsigned int pframe);

// Evict a specific page return 0 of success
unsigned int swap_evict_specific(unsigned int pframe);

#endif
