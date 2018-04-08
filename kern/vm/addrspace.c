#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <synch.h>
#include <addrspace.h>
#include <coremap.h>
#include <swap.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <curthread.h>
#include <thread.h>
#include <vfs.h>

struct addrspace *
as_create(void)
{
    struct addrspace *as = kmalloc(sizeof(struct addrspace));
    if (as==NULL) {
        return NULL;
    }

    struct array *a = array_create();
    if (a == NULL) {
        return NULL;
    }
    as->as_segments = a;
    // We pre-allocate 2 segments for the array which should be enough
    int err = array_preallocate(as->as_segments, 2);
    if (err) {
        array_destroy(as->as_segments);
        kfree(as);
        return NULL;
    }

    // Create an empty array
    a = array_create();
    if (a == NULL) {
        return NULL;
    }
    as->page_table = a;

    err = array_preallocate(as->page_table, 512);
    if (err) {
        array_destroy(as->as_segments);
        array_destroy(as->page_table);
        kfree(as);
        return NULL;
    }

    as->as_heapbase = 0;
    as->as_heapsize = 0;

    return as;
}

void
as_destroy(struct addrspace *as)
{
    assert(as != NULL); // What are you doing?
    int spl = splhigh();
    // Iterate through all the segments and free them
    int i;
    for (i = 0; i < array_getnum(as->as_segments); i++) {
        struct as_segment *seg = array_getguy(as->as_segments, i);
        kfree(seg);
    }
    array_destroy(as->as_segments);

    lock_acquire(swap_lock);
    // Free page table entries
    for (i = 0; i < array_getnum(as->page_table); i++) {
        struct page_table_entry *e = array_getguy(as->page_table, i);
        if (e->swapped) {
            // If the page is currently in swap
            swap_free_page(e->swap_file_frame);
        } else {
            // Change the corresponding coremap entry
            coremap_free_page(e->pframe << PAGE_SHIFT, e);
        }
        kfree(e);
    }
    array_destroy(as->page_table);
    lock_release(swap_lock);

    kfree(as);
    splx(spl);
}

void
as_activate(struct addrspace *as)
{
    int i, spl;

    spl = splhigh();

    // Change uio_space to reflect to the new addrspace for the new process
    for (i = 0; i < array_getnum(as->as_segments); i++) {
        struct as_segment *seg = array_getguy(as->as_segments, i);
        seg->uio.uio_space = curthread->t_vmspace;
    }

    TLB_Flush();

    splx(spl);
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
         int readable, int writeable, int executable)
{
    size_t npages;

    /* Align the region. First, the base... */
    sz += vaddr & ~(vaddr_t)PAGE_FRAME;
    vaddr &= PAGE_FRAME;

    /* ...and now the length. */
    sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

    npages = sz / PAGE_SIZE;

    struct as_segment * seg = kmalloc(sizeof(struct as_segment));
    if (seg == NULL) {
        return ENOMEM;
    }

    // Setup the segment accordingly
    seg->vbase = vaddr;
    seg->npages = npages;
    seg->permission = (readable | writeable | executable);

    // Insert the segment into array
    int err = array_add(as->as_segments, seg);
    if (err) {
        kfree(seg);
        return err;
    }

    // Setup heap
    as->as_heapbase = vaddr + npages * PAGE_SIZE;
    as->as_heapsize = 0;
    return 0;
}

int
as_prepare_load(struct addrspace *as)
{
    (void)as;
    return 0;
}

int
as_complete_load(struct addrspace *as)
{
    (void)as;
    return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
    (void)as;
    *stackptr = USERSTACK;
    return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
    // Implements copy-on-write address space copying
    // 1. Create a new address space
    // 2. Deep copy data structures
    // 3. Make the page table from the new address space points to the same physical memory as the old one
    // 4. Change all the segments to be read only
    // Note: now any write to the address space with cause TLB Hit && Page Fault

    int err;
    int spl = splhigh(); // Critical for accessing key structures
    struct addrspace *new = as_create();
    if (new == NULL) {
        splx(spl);
        return ENOMEM;
    }

    int i;
    // Deep copy segments info
    int old_size = array_getnum(old->as_segments);
    err = array_preallocate(new->as_segments, old_size); // We pre-allocate so future array_add will not fail
    if (err) {
        splx(spl);
        return err;
    }
    for (i = 0; i < old_size; i++) {
        struct as_segment *new_seg = kmalloc(sizeof(struct as_segment));
        if (new_seg == NULL) {
            splx(spl);
            return ENOMEM;
        }
        struct as_segment *old_seg = array_getguy(old->as_segments, i);
        *new_seg = *old_seg; // Copy
        assert(array_add(new->as_segments, new_seg) == 0);
    }
    new->as_heapbase = old->as_heapbase;
    new->as_heapsize = old->as_heapsize;

    // Deep copy page table
    old_size = array_getnum(old->page_table);
    err = array_preallocate(new->page_table, old_size); // We pre-allocate so future array_add will not fail
    if (err) {
        splx(spl);
        return err;
    }
    for (i = 0; i < old_size; i++) {
        struct page_table_entry *new_pte = kmalloc(sizeof(struct page_table_entry));
        if (new_pte == NULL) {
            splx(spl);
            return ENOMEM;
        }
        struct page_table_entry *old_pte = array_getguy(old->page_table, i);
        *new_pte = *old_pte; // Copy

        // Now we consider if the page we are trying to share is in memory or not
        // If it is in memory then we do normal copy-on-write
        // If it is in swap we just allocate a new page here forget about copy-on-write

        int32_t ehi, elo;
        if (!old_pte->swapped) {
            // Copy-On-Write implementation
            // 1. We increase the reference count for all the pages
            // 2. Change cow bit to 1, so later tlb update will still maintain cow
            // 3. We set all the pages we copied to be readonly (now write -> page fault)

            // Make coremap entry also a pointer to the new_pte
            //kprintf("PPage: %d, Insert: %x, into %d\n", old_pte->pframe, new_pte, coremap[old_pte->pframe].ref_count);
            coremap[old_pte->pframe].ptes[coremap[old_pte->pframe].ref_count] = new_pte;

            coremap_inc_page_ref_count(old_pte->pframe << PAGE_SHIFT);

            old_pte->cow = 1;
            new_pte->cow = 1;

            ehi = old_pte->vframe << PAGE_SHIFT;

            int tlb_index = TLB_Probe(ehi, 0); // eho not used pass 0
            if (tlb_index >= 0) { // That means we found the corresponding entry in TLB
                TLB_Read(&ehi, &elo, tlb_index);
                // Make sure the entry we are chaning is valid
                // The entry can be not dirty already when you forked once already
                assert(elo & TLBLO_VALID);
                elo &= ~TLBLO_DIRTY; // Make all page only accessable for reading
                TLB_Write(ehi, elo, tlb_index);
            }
        } else {
            assert(0);
            if (coremap_get_avail_page_count() == 0) { // Now we need to evict
                err = swap_evict();
                if (err) {
                    return err;
                }
            }
            paddr_t paddr = coremap_alloc_upage(new_pte);
            new_pte->pframe = paddr >> PAGE_SHIFT;
            new_pte->cow = 0;
            new_pte->swapped = 0;
            new_pte->swap_file_frame = -1;

            // Swapping in without freeing the page in pagefile
            swap_load_page_without_free(paddr, old_pte->swap_file_frame, new_pte);
        }
        assert(array_add(new->page_table, new_pte) == 0);
    }

    *ret = new;
    splx(spl);
    return 0;
}
