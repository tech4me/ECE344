#include <types.h>
#include <lib.h>
#include <synch.h>
#include <coremap.h>
#include <swap.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>
#include <machine/tlb.h>

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
            coremap[i].status = 1; // Used
            coremap[i].kernel = 1; // Kernel
            coremap[i].block_page_count = 1; // The page itself
            coremap[i].ref_count = 1;
            // coremap[i].ptes are init to zero already
        } else {
            coremap[i].status = 0; // Unused
            coremap[i].kernel = 0; // Not Kernel
            coremap[i].block_page_count = 0; // Not being allocated
            coremap[i].ref_count = 0;
            // coremap[i].ptes are init to zero already
        }
    }
}

int
coremap_stats(int nargs, char **arg)
{
    // Unused parameters
    (void)nargs;
    (void)arg;
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

unsigned int
coremap_get_avail_page_count(void)
{
    assert(curspl>0); // Make sure interrupt is disabled
    unsigned int i;
    unsigned int count = 0;
    for (i = 0; i < page_count; i++) {
        if (!coremap[i].status) {
            count++;
        }
    }
    return count;
}

paddr_t
coremap_alloc_pages(int npages, unsigned int kernel_or_user, struct page_table_entry *pte)
{
    lock_acquire(swap_lock);
    assert(curspl>0); // Make sure interrupt is disabled
    (void)pte; // pte won't be used because we know this is for kernel

    // Here we need to look at which physical memory page is available
    // 1. Do we even have n pages available?
    // 2. Are they continous?
    // 3. Now we need to swap out pages to get all the pages continous
    // Note: because we only allocate multiple page for kernel, we want to try to make them together(not really that necessary)

    unsigned int i, j;
    int count = 0;
    for (i = 0; i < page_count; i++) {
        if (!coremap[i].status) { // Try to used unused pages first
            // Now check if we have npages of continous page
            count = 0;
            for (j = i; j < i + npages; j++) {
                if (coremap[j].status) {
                    count = 1;
                    break;
                }
            }
            if (count == 0) {
                goto found_continous_pages;
            }
        }
    }
    count = 0;
    for (i = 0; i < page_count; i++) {
        if (!coremap[i].kernel) { // Found an non-kernel page since we can't move kernel pages
            // Now check if we have npages of continous page
            count = 0;
            for (j = i; j < i + npages; j++) {
                if (coremap[j].kernel) {
                    count = 1;
                    break;
                }
            }
            if (count == 0) {
                goto found_continous_pages;
            }
        }
    }
    coremap_stats(0, NULL);
    panic("coremap_alloc_pages failed: too many kernel pages already\n"); // We are done

found_continous_pages:

    // Moving page start froming i
    for (j = i; j < i + npages; j++) {
        if (coremap[j].status) { // Page being used, need to move it to an available page
            assert(coremap[j].kernel == 0); // Must not be kernel
            swap_evict_specific(j);
            /*
            for (k = 0; k < page_count; k++) {
                struct page_table_entry *e = array_getguy(coremap[j].as->page_table, coremap[j].pt_index);
                if (!coremap[k].status && (coremap[i].ref_count <= 1)) { // Found a free page
                    // Copy page
                    memmove((void *)PADDR_TO_KVADDR(k << PAGE_SHIFT), (const void *)PADDR_TO_KVADDR(j << PAGE_SHIFT), PAGE_SIZE);
                    // Update pte to reflect
                    kprintf("j: %d, cow:%d\n", j, e->cow);
                    e->pframe = k;
                    // Update coremap
                    coremap[k] = coremap[j];
                    // We need to flush TLB later
                    break;
                }
            }
            */
        }
        if (j == i) {
            coremap[j].status = 1;
            coremap[j].kernel = kernel_or_user;
            coremap[j].block_page_count = npages;
            coremap[j].ref_count = 1;
        } else {
            coremap[j].status = 1;
            coremap[j].kernel = kernel_or_user;
            coremap[j].block_page_count = 0;
            coremap[j].ref_count = 1;
        }
    }
    lock_release(swap_lock);
    return (i * PAGE_SIZE);
}

paddr_t
coremap_alloc_page(unsigned int kernel_or_user, struct page_table_entry *pte)
{
    assert(curspl>0); // Make sure interrupt is disabled
    int i;
    for (i = 0; i < (int)page_count; i++) {
        if (!coremap[i].status) { // Found an empty page
            coremap[i].status = 1;
            coremap[i].kernel = kernel_or_user;
            coremap[i].block_page_count = 1;
            coremap[i].ref_count = 1;
            coremap[i].ptes[0] = pte; // Set the first one
            return (i << PAGE_SHIFT);
        }
    }
    assert(0); // We should never be here, because this function will only be called if we know we have memory
    return (paddr_t)NULL;
}

void
coremap_free_pages(paddr_t paddr, struct page_table_entry *pte)
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
    unsigned int i, j, k;
    unsigned int block_page_count = coremap[pframe].block_page_count;
    for (i = 0; i < block_page_count; i++) {
        coremap[pframe + i].ref_count -= 1;
        assert(coremap[pframe + i].ref_count >= 0);
        if (coremap[pframe + i].ref_count == 0) {
            coremap[pframe + i].status = 0;
            coremap[pframe + i].kernel = 0;
            coremap[pframe + i].block_page_count = 0;
            coremap[pframe + i].ref_count = 0;
            for (j = 0; j < MAX_SHARED_PAGE; j++) {
                coremap[pframe + i].ptes[j] = NULL;
            }
        } else {
            // Just remove the pte pointer
            int flag = 0;
            for (j = 0; j < MAX_SHARED_PAGE; j++) {
                if (coremap[pframe + i].ptes[j] == pte) {
                    for (k = j; k < MAX_SHARED_PAGE - 1; k++) {
                        coremap[pframe + i].ptes[k] = coremap[pframe + i].ptes[k + 1];
                    }
                    flag = 1;
                    break;
                }
            }
            assert(flag);
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

unsigned int
coremap_get_page_ref_count(paddr_t paddr)
{
    assert(curspl>0); // Make sure interrupt is disabled

    unsigned int pframe = paddr >> PAGE_SHIFT;
    return coremap[pframe].ref_count;
}

void
coremap_page_swap_in(paddr_t paddr, struct page_table_entry *pte)
{
    assert(curspl>0); // Make sure interrupt is disabled

    unsigned int pframe = paddr >> PAGE_SHIFT;
    coremap[pframe].status = 1;
    coremap[pframe].kernel = 0; // Kernel should never be swapped out in the first place
    coremap[pframe].block_page_count = 1;
    coremap[pframe].ref_count = 1; // No cow after swap
    coremap[pframe].ptes[0] = pte;
}

void
coremap_page_swap_out(paddr_t paddr)
{
    assert(curspl>0); // Make sure interrupt is disabled

    unsigned int pframe = paddr >> PAGE_SHIFT;
    coremap[pframe].status = 0;
    coremap[pframe].kernel = 0; // Kernel should never be swapped out in the first place
    coremap[pframe].block_page_count = 0;
    coremap[pframe].ref_count = 0;
    unsigned int j;
    for (j = 0; j < MAX_SHARED_PAGE; j++) {
        coremap[pframe].ptes[j] = NULL;
    }
}

unsigned int
coremap_page_to_evict(void)
{
    assert(curspl>0); // Make sure interrupt is disabled

    // Right now we just return a random page TODO: Change this to be better such as aging
    unsigned int i, count = 0;
    for (i = 0; i < page_count; i++) {
        if (!coremap[i].kernel) { // Have to be not a kernel page, bad things might happen
            count++;
        }
    }
    assert(count > 0); // We are out of memory
    unsigned int index = random() % count;
    count = 0;
    for (i = 0; i < page_count; i++) {
        if (!coremap[i].kernel) { // Have to be not a kernel page, bad things might happen
            if (index == count) {
                assert(coremap[i].status == 1);
                assert(coremap[i].kernel == 0);
                return i; // i is the page that we want to evict
            }
            count++;
        }
    }
    assert(0); // Should not reach here
    return -1;
}

unsigned int
coremap_page_to_evict_avoidance(unsigned int pframe)
{
    assert(curspl>0); // Make sure interrupt is disabled

    // Right now we just return a random page TODO: Change this to be better such as aging
    unsigned int i, count = 0;
    for (i = 0; i < page_count; i++) {
        if (!coremap[i].kernel && (i != pframe)) { // Have to be not a kernel page, bad things might happen
            count++;
        }
    }
    assert(count > 0); // We are out of memory
    unsigned int index = random() % count;
    count = 0;
    for (i = 0; i < page_count; i++) {
        if (!coremap[i].kernel && (i != pframe)) { // Have to be not a kernel page, bad things might happen
            if (index == count) {
                if (coremap[i].status != 1) {
                    coremap_stats(0, NULL);
                    assert(coremap[i].status == 1);
                }
                assert(coremap[i].kernel == 0);
                return i; // i is the page that we want to evict
            }
            count++;
        }
    }
    assert(0); // Should not reach here
    return -1;

}
