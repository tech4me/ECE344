#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

struct addrspace *
as_create(void)
{
    int i;
    struct addrspace *as = kmalloc(sizeof(struct addrspace));
    if (as==NULL) {
        return NULL;
    }

    as->as_vbase1 = 0;
    as->as_pbase1 = 0;
    as->as_npages1 = 0;
    as->as_vbase2 = 0;
    as->as_pbase2 = 0;
    as->as_npages2 = 0;

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

    // Create an empty queue
    struct queue *q = q_create(1);
    if (q == NULL) {
        return NULL;
    }
    as->page_table = q;

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
    //as->as_vbase1 = alloc_kpages(as->as_npages1);
    //as->as_pbase1 = getppages(as->as_npages1);
    //if (as->as_pbase1 == 0) {
    //    return ENOMEM;
    //}

    //as->as_vbase2 = alloc_kpages(as->as_npages2);
    //as->as_pbase2 = getppages(as->as_npages2);
    //if (as->as_pbase2 == 0) {
    //    return ENOMEM;
    //}

    //as->as_stackbase = alloc_kpages(DUMBVM_STACKPAGES);
    //as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
    //if (as->as_stackbase == 0) {
    //    return ENOMEM;
    //}

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
    struct addrspace *new;

    new = as_create();
    if (new==NULL) {
        return ENOMEM;
    }

    new->as_vbase1 = old->as_vbase1;
    new->as_npages1 = old->as_npages1;
    new->as_vbase2 = old->as_vbase2;
    new->as_npages2 = old->as_npages2;

    if (as_prepare_load(new)) {
        as_destroy(new);
        return ENOMEM;
    }

    assert(new->as_pbase1 != 0);
    assert(new->as_pbase2 != 0);
    assert(new->as_stackbase != 0);

    memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
        (const void *)PADDR_TO_KVADDR(old->as_pbase1),
        old->as_npages1*PAGE_SIZE);

    memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
        (const void *)PADDR_TO_KVADDR(old->as_pbase2),
        old->as_npages2*PAGE_SIZE);

    /*
    memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
        (const void *)PADDR_TO_KVADDR(old->as_stackpbase),
        DUMBVM_STACKPAGES*PAGE_SIZE);
    */

    *ret = new;
    return 0;
}
