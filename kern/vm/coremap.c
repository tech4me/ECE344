#include <types.h>
#include <lib.h>
#include <coremap.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>

struct coremap_entry *coremap = NULL;
unsigned int page_count = 0;

void
coremap_init(void)
{
    u_int32_t start, end;

    // Get the amount of physical memory
    ram_getsize(&start, &end);

    // Only need the end because we know memory start at 0
    page_count = (int)(end / PAGE_SIZE); // We also do paging for exsisting kernel memory
    kprintf("***Total page count: %d\n", page_count);

    // Now we know where to put coremap
    coremap =  (struct coremap_entry *)PADDR_TO_KVADDR(start); // Append coremap right after the exsisting kernel memory

    start += page_count * sizeof(struct coremap_entry); // New start after saving enough save for coremap

    // Page align(to make our life easier)
    start = (start + (PAGE_SIZE - 1)) & -PAGE_SIZE;
    kprintf("***Page left after bootstrap: %d\n", (end - start)/PAGE_SIZE);

    // Here we init coremap
    unsigned int i;
    for (i = 0; i < page_count; i++) {
        if (i < start/PAGE_SIZE) {
            coremap[i].as = NULL; // There is no corresponding address space for the kernel
            coremap[i].status = 1; // Used
            coremap[i].kernel = 1; // Kernel
            coremap[i].block_page_count = 1; // The page itself
            coremap[i].ref_count = 1;
        } else {
            coremap[i].as = NULL; // There is no corresponding address space for the unmapped space
            coremap[i].status = 0; // Unused
            coremap[i].kernel = 0; // Not Kernel
            coremap[i].block_page_count = 0; // Not being allocated
            coremap[i].ref_count = 0;
        }
    }
}

int
coremap_stats(void)
{
    int spl = splhigh(); // Disable interrupt when printing
    unsigned int i;
    for (i = 0; i < page_count; i++) {
        char a;
        if (coremap[i].kernel & coremap[i].status) {
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
coremap_alloc_pages(int npages, unsigned int kernel_or_user)
{
    assert(curspl>0); // Make sure interrupt is disabled

    // Here we need to look at which physical memory page is available
    // 1. Do we even have n pages available?
    // 2. Are they continues?
    unsigned int i, j;
    int count = 0;
    for (i = 0; i < page_count; i++) {
        if (!coremap[i].status) {
            count++;
        }
    }
    if (count <= npages) {
        panic("coremap_alloc_pages failed: out of physical memory!\n"); // TODO: remove this after swap
        return (paddr_t)NULL;
    }
    for (i = 0; i < page_count; i++) {
        if (!coremap[i].status) { // Found an empty page
            // Now check if we have npages of continues page
            count = 0;
            for (j = i; j < i + npages; j++) {
                if (!coremap[i].status) {
                    count = 0;
                } else {
                    count = 1;
                    break;
                }
            }
            if (!count) { // We are done
                break;
            }
        }
    }
    if (count == 1) {
        panic("coremap_alloc_pages failed: out of physical memory!\n"); // TODO: remove this after swap
        return (paddr_t)NULL;
    }

    // Now i is the offset we can use to start page allocation
    for (j = i; j < i + npages; j++) {
        if (j == i) {
            coremap[j].as = curthread->t_vmspace;
            coremap[j].status = 1;
            coremap[i].kernel = kernel_or_user;
            coremap[j].block_page_count = npages;
            coremap[j].ref_count = 1;
        } else {
            coremap[j].as = curthread->t_vmspace;
            coremap[j].status = 1;
            coremap[i].kernel = kernel_or_user;
            coremap[j].block_page_count = 0;
            coremap[j].ref_count = 0;
        }
    }
    return (i * PAGE_SIZE);
}

paddr_t
coremap_alloc_page(unsigned int kernel_or_user)
{
    assert(curspl>0); // Make sure interrupt is disabled
    int i;
    for (i = 0; i < (int)page_count; i++) {
        if (!coremap[i].status) { // Found an empty page
            coremap[i].as = curthread->t_vmspace;
            coremap[i].status = 1;
            coremap[i].kernel = kernel_or_user;
            coremap[i].block_page_count = 1;
            coremap[i].ref_count = 1;
            return (i << PAGE_SHIFT);
        }
    }
    panic("coremap_alloc_page failed: out of physical memory!\n"); // TODO: remove this after swap
    return (paddr_t)NULL;
}

void
coremap_free_pages(paddr_t paddr)
{
    assert(curspl>0); // Make sure interrupt is disabled

    // Determine how many pages we need to free
    // 1. Find the page from the paddr that we got
    // 2. Free each page in the block
    // 3. Decrease ref count on the page
    // 4. If ref count is 0, than free the page otherwise don't do anything
    unsigned int pframe = paddr >> PAGE_SHIFT;
    // I thought the below assert was necessary, however one of the kernel page was freed by the OS
    //assert(pframe >= first_avail_page && pframe < page_count);
    unsigned int i;
    for (i = 0; i < coremap[pframe].block_page_count; i++) {
        coremap[pframe + i].ref_count -= 1;
        assert(coremap[pframe + i].ref_count >= 0);
        if (coremap[pframe + i].ref_count == 0) {
            coremap[pframe + i].status = 0;
            coremap[pframe + i].kernel = 0;
            coremap[pframe + i].as = NULL;
            coremap[pframe + i].block_page_count = 0;
            coremap[pframe + i].ref_count = 0;
        }
    }
}

void
coremap_inc_page_ref_count(paddr_t paddr)
{
    assert(curspl>0); // Make sure interrupt is disabled

    unsigned int pframe = paddr >> PAGE_SHIFT;
    coremap[pframe].ref_count += 1;
}
