#ifndef _SWAP_H_
#define _SWAP_H_

// This function init the swapping functionality of the system (Open swap file and such)
void swap_init(void);

// Load a page from swap file to memory
void swap_load_page(paddr_t paddr, int file_frame, struct addrspace *as, unsigned int pt_index);

// Store a page from memory to swap file
void swap_store_page(paddr_t paddr, int file_frame);

#endif
