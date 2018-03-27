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

static struct vnode *swapfile;
static int swapsize;

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
}


void
swap_load_page(paddr_t paddr, int file_frame, struct addrspace *as, unsigned int pt_index)
{
    assert(curspl>0); // Make sure interrupt is disabled

    struct uio u;
    // Need to work within kernel space
    mk_kuio(&u, (void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE, file_frame << PAGE_SHIFT, UIO_READ);

    if (VOP_READ(swapfile, &u)) {
        panic("swap_load_page failed\n");
    }

    // Update coremap here to reflect the change
    coremap_page_swap_in(paddr, as, pt_index);
}

void
swap_store_page(paddr_t paddr, int file_frame)
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
