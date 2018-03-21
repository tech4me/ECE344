#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <coremap.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>

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
    as->as_stackbase = 0;

    return as;
}

void
as_destroy(struct addrspace *as)
{
    assert(as != NULL); // What are you doing?
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
}

void
as_activate(struct addrspace *as)
{
    int i, spl;

    (void)as;

    spl = splhigh();

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

    // Setup heap and stack
    as->as_heapbase = vaddr + npages * PAGE_SIZE;
    as->as_heapsize = 0;
    as->as_stackbase = USERSTACK;
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
    assert(as->as_stackbase != 0);

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
    for (i = 0; i < array_getnum(old->as_segments); i++) {
        struct as_segment *new_seg = kmalloc(sizeof(struct as_segment));
        if (new_seg == NULL) {
            splx(spl);
            return ENOMEM;
        }
        struct as_segment *old_seg = array_getguy(old->as_segments, i);
        *new_seg = *old_seg; // Copy
        err = array_add(new->as_segments, new_seg);
        if (err) {
            splx(spl);
            return err;
        }
    }
    new->as_heapbase = old->as_heapbase;
    new->as_heapsize = old->as_heapsize;
    new->as_stackbase = old->as_stackbase;

    // Deep copy page table
    for (i = 0; i < array_getnum(old->page_table); i++) {
        struct page_table_entry *new_pte = kmalloc(sizeof(struct page_table_entry));
        if (new_pte == NULL) {
            splx(spl);
            return ENOMEM;
        }
        struct page_table_entry *old_pte = array_getguy(old->page_table, i);
        *new_pte = *old_pte; // Copy
        // TODO: change this
        paddr_t paddr = coremap_alloc_page();
        new_pte->pframe = paddr >> PAGE_SHIFT;
        memmove((void *)PADDR_TO_KVADDR(paddr), (const void *)PADDR_TO_KVADDR(old_pte->pframe << PAGE_SHIFT), PAGE_SIZE);
        err = array_add(new->page_table, new_pte);
        if (err) {
            splx(spl);
            return err;
        }
    }

    *ret = new;
    splx(spl);
    return 0;
}
