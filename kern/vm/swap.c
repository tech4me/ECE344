#include <types.h>
#include <lib.h>
#include <machine/vm.h>
#include <machine/spl.h>
#include <kern/unistd.h>
#include <vfs.h>
#include <vnode.h>
#include <uio.h>
#include <kern/stat.h>
#include <swap.h>
#include <coremap.h>
#include <addrspace.h>
#include <bitmap.h>

static struct vnode *swapfile;
static unsigned int swapsize;
static struct bitmap *swap_table;

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
}


void
swap_load_page(paddr_t paddr, unsigned int file_frame, struct addrspace *as, unsigned int pt_index, unsigned int ref_count)
{
    assert(curspl>0); // Make sure interrupt is disabled

    struct uio u;
    // Need to work within kernel space
    mk_kuio(&u, (void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE, file_frame << PAGE_SHIFT, UIO_READ);

    if (VOP_READ(swapfile, &u)) {
        panic("swap_load_page failed\n");
    }

    // Update coremap here to reflect the change
    coremap_page_swap_in(paddr, as, pt_index, ref_count);

    // Free the swap page
    swap_free_page(file_frame);
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
    coremap_page_swap_out(paddr);
}

unsigned int
swap_alloc_page(void)
{
    assert(curspl>0); // Make sure interrupt is disabled
    unsigned int temp;
    bitmap_alloc(swap_table, &temp);
    assert(temp < swapsize);
    return temp;
}

void
swap_free_page(unsigned int file_frame)
{
    assert(curspl>0); // Make sure interrupt is disabled
    assert(bitmap_isset(swap_table, file_frame)); // This should always be true
    bitmap_unmark(swap_table, file_frame);
}

unsigned int
swap_evict(void)
{
    assert(curspl>0); // Make sure interrupt is disabled

    // Figure out which page to be removed from memory
    unsigned int pframe = coremap_page_to_evict();

    // Allocate a page in swap file
    unsigned int file_frame = swap_alloc_page();

    // Update pte
    struct addrspace *as = coremap[pframe].as;
    unsigned int pt_index = coremap[pframe].pt_index;
    if (pt_index >= array_getnum(as->page_table)) {
        kprintf("Swap evict caused it!\n");
        kprintf("pframe: %d\n", pframe);
        kprintf("fileframe: %d\n", fileframe);
        kprintf("Coremap entry:\n");
        kprintf("status %d\n", coremap[pframe].status);
        kprintf("kernel %d\n", coremap[pframe].kernel);
        kprintf("as 0x%x\n", coremap[pframe].as);
        kprintf("pt_index %d\n", coremap[pframe].pt_index);
    }
    struct page_table_entry *e = array_getguy(as->page_table, pt_index);
    assert(e->swapped == 0); // Page shouldn't be swapped out already
    e->swapped = 1;
    e->swap_file_frame = file_frame;
    e->swap_coremap_ref_count = coremap[pframe].ref_count;

    // Swap out the page
    swap_store_page(pframe << PAGE_SHIFT, file_frame);

    return pframe;
}
