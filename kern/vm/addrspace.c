#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <coremap.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <curthread.h>
#include <thread.h>

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
        kfree(as);
        return NULL;
    }

    // Create an empty arrray
    a = array_create(); // TODO: Figure what is the correct size to pre-allocate
    if (a == NULL) {
        return NULL;
    }
    as->page_table = a;

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

    // Free page table entries
    for (i = 0; i < array_getnum(as->page_table); i++) {
        struct page_table_entry *e = array_getguy(as->page_table, i);
        // Change the corresponding coremap entry (Need to change this when add swapping)
        coremap_free_page(e->pframe << PAGE_SHIFT);
        kfree(e);
    }
    array_destroy(as->page_table);

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

    for (i=0; i<NUM_TLB; i++) {
        TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }

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
#ifdef NO_COW
        paddr_t paddr = coremap_alloc_page();
        new_pte->pframe = paddr >> PAGE_SHIFT;
        // Below need to use the kernel space
        // Important! Because each virtual address space is independent but kernel is all mapped to the same address at different address space
        memmove((void *)PADDR_TO_KVADDR(paddr), (const void *)PADDR_TO_KVADDR(old_pte->pframe << PAGE_SHIFT), PAGE_SIZE);
#else
        // Copy-On-Write implementation
        // 1. We increase the reference count for all the pages
        // 2. Change cow bit to 1, so later tlb update will still maintain cow
        // 3. We set all the pages we copied to be readonly (now write -> page fault)
        coremap_inc_page_ref_count(old_pte->pframe << PAGE_SHIFT);

        old_pte->cow = 1;
        new_pte->cow = 1;

        u_int32_t ehi, elo;
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
#endif
        assert(array_add(new->page_table, new_pte) == 0);
    }

    *ret = new;
    splx(spl);
    return 0;
}
