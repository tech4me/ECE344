#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <machine/vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <kern/unistd.h>
#include <vfs.h>
#include <vnode.h>
#include <uio.h>
#include <kern/stat.h>
#include <synch.h>
#include <swap.h>
#include <coremap.h>
#include <addrspace.h>
#include <bitmap.h>

static struct vnode *swapfile;
static unsigned int swapsize;
static struct bitmap *swap_table;
static unsigned int swap_avail_page;
struct lock *swap_lock;

void
swap_init(void)
{
    // Interrupt doesn't need to be off here because we are boot straping
    char *swapfile_name = "lhd1raw:";
    int err = vfs_open(swapfile_name, O_RDWR, &swapfile);
    if (err) {
        panic("Unable to open swapfile\n");
    }
    struct stat stat;
    VOP_STAT(swapfile, &stat);
    swapsize = stat.st_size / PAGE_SIZE; // Get the number of pages that can be used for swapping
    swap_table = bitmap_create(swapsize); // Use this structure to allocate and deallocate swap page
    swap_avail_page = swapsize;
    swap_lock = lock_create("swap_lock");
}

unsigned int
swap_get_avail_page_count(void)
{
    assert(curspl>0); // Make sure interrupt is disabled
    return swap_avail_page;
}

void
swap_load_page(paddr_t paddr, unsigned int file_frame, struct page_table_entry *pte)
{
    lock_acquire(swap_lock);
    assert(curspl>0); // Make sure interrupt is disabled

    //kprintf("Swapping in! pframe: %d, vframe: %d\n", paddr >> 12, pte->vframe);
    struct uio u;
    // Need to work within kernel space
    mk_kuio(&u, (void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE, file_frame << PAGE_SHIFT, UIO_READ);

    if (VOP_READ(swapfile, &u)) {
        panic("swap_load_page failed\n");
    }

    // Update coremap here to reflect the change
    coremap_page_swap_in(paddr, pte);

    // Update pte to reflect the swapin
    pte->swapped = 0;
    pte->swap_file_frame = -1;

    // Free the swap page
    swap_free_page(file_frame);
    lock_release(swap_lock);
}

void
swap_load_page_without_free(paddr_t paddr, unsigned int file_frame, struct page_table_entry *pte)
{
    lock_acquire(swap_lock);
    assert(curspl>0); // Make sure interrupt is disabled

    //kprintf("Swapping in without free! pframe: %d, vframe: %d\n", paddr >> 12, pte->vframe);
    struct uio u;
    // Need to work within kernel space
    mk_kuio(&u, (void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE, file_frame << PAGE_SHIFT, UIO_READ);

    if (VOP_READ(swapfile, &u)) {
        panic("swap_load_page failed\n");
    }

    // Update coremap here to reflect the change
    coremap_page_swap_in(paddr, pte);
    lock_release(swap_lock);
}

void
swap_store_page(paddr_t paddr, unsigned int file_frame)
{
    assert(curspl>0); // Make sure interrupt is disabled

    struct uio u;
    // Need to work within kernel space
    mk_kuio(&u, (void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE, file_frame << PAGE_SHIFT, UIO_WRITE);

    if (VOP_WRITE(swapfile, &u)) {
        panic("swap_store_page failed\n");
    }

    // Update coremap here to reflect the change
    //coremap_page_swap_out(paddr);
    TLB_Flush();
}

unsigned int
swap_alloc_page(void)
{
    assert(curspl>0); // Make sure interrupt is disabled
    unsigned int temp;
    bitmap_alloc(swap_table, &temp);
    swap_avail_page--;
    assert(temp < swapsize);
    return temp;
}

void
swap_free_page(unsigned int file_frame)
{
    assert(curspl>0); // Make sure interrupt is disabled
    assert(bitmap_isset(swap_table, file_frame)); // This should always be true
    bitmap_unmark(swap_table, file_frame);
    swap_avail_page++;
}

unsigned int
swap_evict(void)
{
    //lock_acquire(swap_lock);
    assert(curspl>0); // Make sure interrupt is disabled

    if (swap_avail_page == 0) { // We are out of swap -> out of memory
        return ENOMEM;
    }

    //unsigned int k;
    //for (k = 0; k <128; k++)
    //    kprintf("coremap: %d,ref_count: %d\n", k, coremap[k].ref_count);

    // Figure out which page to be removed from memory
    unsigned int pframe = coremap_page_to_evict();
    //kprintf("Planning to swap %d\n", pframe);

    // Update all the ptes because when swaping out page, we can't have shared pages anymore
    unsigned int i;
    for (i = 0; i < coremap[pframe].ref_count; i++) {
        assert(i == 0); // Not testing i > 0 yet

        // Allocate a page in swap file
        unsigned int file_frame = swap_alloc_page();

        // Swap out the page
        swap_store_page(pframe << PAGE_SHIFT, file_frame);

        struct page_table_entry *e = coremap[pframe].ptes[i];
        //kprintf("kernel: %d pframe: %d, pte: 0x%x\n", coremap[pframe].kernel, pframe, e);
        //kprintf("Swapping out! pframe: %d, vframe: %d, i: %d\n", pframe, e->vframe, i);
        if (e->swapped != 0) {
            assert(e->swapped == 0); // Page shouldn't be swapped out already
        }
        e->cow = 0; // No copy-on-write anymore
        e->swapped = 1;
        e->swap_file_frame = file_frame;
    }
    coremap_page_swap_out(pframe << PAGE_SHIFT);

    //lock_release(swap_lock);
    return 0;
}

unsigned int
swap_evict_avoidance(unsigned int avoid_pframe)
{
    assert(0);
    assert(curspl>0); // Make sure interrupt is disabled

    if (swap_avail_page == 0) { // We are out of swap -> out of memory
        return ENOMEM;
    }

    //kprintf("In swap_evict_avoidance:\n");

    // Figure out which page to be removed from memory
    unsigned int pframe = coremap_page_to_evict_avoidance(avoid_pframe);

    // Update all the ptes because when swaping out page, we can't have shared pages anymore
    unsigned int i;
    for (i = 0; i < coremap[pframe].ref_count; i++) {

        // Allocate a page in swap file
        unsigned int file_frame = swap_alloc_page();

        // Swap out the page
        swap_store_page(pframe << PAGE_SHIFT, file_frame);

        struct page_table_entry *e = coremap[pframe].ptes[i];
        //kprintf("Swapping out: %d, i: %d\n", pframe, i);
        if (e->swapped != 0) {
            assert(e->swapped == 0); // Page shouldn't be swapped out already
        }
        if (e->cow) {
            kprintf("Swapping out cow\n");
        }
        e->cow = 0; // No copy-on-write anymore
        e->swapped = 1;
        e->swap_file_frame = file_frame;
    }
    coremap_page_swap_out(pframe << PAGE_SHIFT);

    return 0;
}

unsigned int
swap_evict_specific(unsigned int pframe)
{
    //lock_acquire(swap_lock);
    assert(curspl>0); // Make sure interrupt is disabled

    if (swap_avail_page == 0) { // We are out of swap -> out of memory
        return ENOMEM;
    }

    // Update all the ptes because when swaping out page, we can't have shared pages anymore
    unsigned int i;
    for (i = 0; i < coremap[pframe].ref_count; i++) {
        assert(i == 0); // Not testing i > 0 yet

        // Allocate a page in swap file
        unsigned int file_frame = swap_alloc_page();

        // Swap out the page
        swap_store_page(pframe << PAGE_SHIFT, file_frame);

        struct page_table_entry *e = coremap[pframe].ptes[i];
        if (e->swapped != 0) {
            assert(e->swapped == 0); // Page shouldn't be swapped out already
        }
        e->cow = 0; // No copy-on-write anymore
        e->swapped = 1;
        e->swap_file_frame = file_frame;
    }
    coremap_page_swap_out(pframe << PAGE_SHIFT);

    //lock_release(swap_lock);
    return 0;
}
