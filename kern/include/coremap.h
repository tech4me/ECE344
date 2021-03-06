#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <addrspace.h>

#define MAX_SHARED_PAGE 24

struct coremap_entry {
    //struct addrspace *as; // Which address space does this page belongs to
    unsigned int status : 1; // Indicate if this page is being used or not
    unsigned int kernel : 1; // Indicate un-swappable kernel page
    unsigned int block_page_count : 14; // Number of pages in the block following this page(include this page)
    unsigned int ref_count : 6; // Reference count to a physical page, the page should only be freed when ref_count is 0(Allows for 65535 reference should be enough)
    //unsigned int pt_index : 16; // Index of the page in the page table
    struct page_table_entry *ptes[MAX_SHARED_PAGE]; // Use to backtrack to pte
};

extern struct coremap_entry *coremap;

// Init coremap structure
void coremap_init(void);

// All the functions below should only be called after vm_bootstrap

// Print physical page usage
int coremap_stats(int nargs, char **arg);

// Get current max number of pages that can be allocated
unsigned int coremap_get_avail_page_count(void);

// The following two function should not be used directly
// Use the kernel/user macros instead
///////////////////////////////////////////
// Allocate mutiple physical page
paddr_t coremap_alloc_pages(int npages, unsigned int kernel_or_user, struct page_table_entry *pte);

// Allocate a single physical page
paddr_t coremap_alloc_page(unsigned int kernel_or_user, struct page_table_entry *pte);
///////////////////////////////////////////

#define coremap_alloc_kpages(npages) coremap_alloc_pages(npages, 1, NULL)
//#define coremap_alloc_kpages(npages) coremap_alloc_page(1, NULL)
#define coremap_alloc_kpage() coremap_alloc_page(1, NULL)

// This should never be called
//#define coremap_alloc_upages(npages, pte) coremap_alloc_pages(npages, 0, pte)
#define coremap_alloc_upage(pte) coremap_alloc_page(0, pte)

// Free multiple physical page(dec ref count first, if 0 free)
void coremap_free_pages(paddr_t paddr, struct page_table_entry *pte);

// Free a single physical page(dec ref count first, if 0 free)
#define coremap_free_page(paddr, pte) coremap_free_pages(paddr, pte)

// Increase the page reference count
void coremap_inc_page_ref_count(paddr_t paddr);

// Returns the reference count of a physical page
unsigned int coremap_get_page_ref_count(paddr_t paddr);

// Update coremap for swap-in/swap-out
void coremap_page_swap_in(paddr_t paddr, struct page_table_entry *pte);
void coremap_page_swap_out(paddr_t paddr);

// Find a page to evict
unsigned int coremap_page_to_evict(void);

// Find a page to evict(avoidance)
unsigned int coremap_page_to_evict_avoidance(unsigned int pframe);

#endif
