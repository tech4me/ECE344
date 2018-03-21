#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <addrspace.h>

struct coremap_entry {
    struct addrspace *as; // Which address space does this page belongs to
    unsigned int status : 1; // Indicate if this page is being used or not
    unsigned int kernel : 1; // Indicate un-swappable kernel page
    unsigned int block_page_count : 16; // Number of pages in the block following this page(include this page)
};

extern struct coremap_entry *coremap;
extern unsigned int page_count;
extern unsigned int first_avail_page;

// All the functions below should only be called after vm_bootstrap

// Print physical page usage
int coremap_stats(void);

// Allocate a single physical page
paddr_t coremap_alloc_page(void);

// Free multiple physical page
void coremap_free_pages(paddr_t paddr);

#define coremap_free_page(paddr) coremap_free_pages(paddr)

#endif
