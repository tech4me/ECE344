#include <types.h>
#include <lib.h>
#include <coremap.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>

struct coremap_entry *coremap = NULL;
unsigned int page_count = 0;
unsigned int first_avail_page = 0;

int
coremap_stats(void)
{
    int spl = splhigh(); // Disable interrupt when printing
    unsigned int i;
    for (i = 0; i < page_count; i++) {
        char a;
        if (coremap[i].kernel) {
            a = 'K';
        } else if (coremap[i].status) {
            a = 'X';
        } else {
            a = '_';
        }
        kprintf("%3d: %c", i, a);
        if ((i % 6) == 5) {
            kprintf("\n");
        } else {
            kprintf("   ");
        }
    }
    kprintf("\n");
    splx(spl);
    return 0;
}

paddr_t
coremap_alloc_page(void)
{
    assert(curspl>0); // Make sure interrupt is disabled
    int i;
    for (i = (int)first_avail_page; i < (int)page_count; i++) {
        if (!coremap[i].status) { // Found an empty page
            coremap[i].as = curthread->t_vmspace;
            coremap[i].status = 1;
            coremap[i].block_page_count = 1;
            return (i << PAGE_SHIFT);
        }
    }
    panic("coremap_alloc_page failed: out of physical memory!\n"); // TODO: remove this after swap
    return NULL;
}

/*
void
coremap_free_page(paddr_t paddr)
{
    assert(curspl>0); // Make sure interrupt is disabled
    unsigned int pframe = paddr >> PAGE_SHIFT;
    coremap[pframe].status = 0;
    coremap[pframe].as = NULL;
    coremap[pframe].block_page_count = 0;
}
*/

void
coremap_free_pages(paddr_t paddr)
{
    assert(curspl>0); // Make sure interrupt is disabled

    // Determine how many pages we need to free
    // 1. Find the page from the paddr that we got
    // 2. See if we are freeing multiple page
    unsigned int pframe = paddr >> PAGE_SHIFT;
    if (pframe >= first_avail_page && pframe < page_count) {
        unsigned int i;
        for (i = 0; i < coremap[pframe].block_page_count; i++) {
            coremap[pframe].status = 0;
            coremap[pframe].as = NULL;
            coremap[pframe].block_page_count = 0;
        }
    } else {
        kprintf("coremap_free_pages failed, address not found!\n");
    }
}
