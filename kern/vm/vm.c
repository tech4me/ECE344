#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

static int vm_bootstrap_flag = 0;
static struct coremap_entry *coremap = NULL;
static unsigned int page_count = 0;

void
vm_bootstrap(void)
{
    kprintf("VM bootstrap:\n");
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
            coremap[i].status_flag = 1; // Used
            coremap[i].kernel_flag = 1; // Kernel
            coremap[i].block_page_count = 1; // The page itself
        } else {
            coremap[i].as = NULL; // There is no corresponding address space for the unmapped space
            coremap[i].status_flag = 0; // Unused
            coremap[i].kernel_flag = 0; // Not Kernel
            coremap[i].block_page_count = 0; // Not being allocated
        }
    }

    vm_bootstrap_flag = 1; // Finished bootstrap
}

paddr_t
getppages(unsigned long npages)
{
    int spl;
    paddr_t addr;

    spl = splhigh();
    addr = ram_stealmem(npages);
    splx(spl);
    return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(int npages)
{
    // Here we need to look to see if vm is up or not
    // If vm is up we can't steal anymore
    if (!vm_bootstrap_flag) {
        paddr_t pa;
        pa = getppages(npages);
        if (pa==0) {
            return 0;
        }
        return PADDR_TO_KVADDR(pa);
    }
    int spl = splhigh();
    // Here we need to look at which physical memory page is available
    // 1. Do we even have n pages available?
    // 2. Are they continues?
    unsigned int i, j;
    int count = 0;
    for (i = 0; i < page_count; i++) {
        if (!coremap[i].status_flag) {
            count++;
        }
    }
    assert(count >= npages);
    for (i = 0; i < page_count; i++) {
        if (!coremap[i].status_flag) { // Found an empty page
            // Now check if we have npages of continues page
            count = 0;
            for (j = i; j < i + npages; j++) {
                if (!coremap[i].status_flag) {
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
    splx(spl);
    if (count == 1) {
        panic("Unable to allocate continous memory");
    }

    // Now i is the offset we can use to start page allocation
    for (j = i; j < i + npages; j++) {
        if (j == i) {
            coremap[j].as = curthread->t_vmspace;
            coremap[j].status_flag = 1;
            coremap[j].block_page_count = npages;
        } else {
            coremap[j].as = curthread->t_vmspace;
            coremap[j].status_flag = 1;
            coremap[j].block_page_count = 0;
        }
    }
    return PADDR_TO_KVADDR(i * PAGE_SIZE);
}

void
free_kpages(vaddr_t addr)
{
    kprintf("Freeing 0x%x\n", addr);
    // Determine how many pages we need to free
    // 1. Find the page from the addr that we got
    // 2. See if we are freeing multiple page
    int spl = splhigh();
    unsigned int i;
    for (i = 0; i < page_count; i++) {
        if (PADDR_TO_KVADDR(i * PAGE_SIZE) == addr) {
            unsigned int j;
            for (j = 0; j < coremap[i].block_page_count; j++) {
                assert(coremap[i + j].kernel_flag == 0); // Make sure we know what we are doing
                coremap[i + j].as = NULL;
                coremap[i + j].status_flag = 0; // Available
                coremap[i + j].block_page_count = 0; // Reset to 0
            }
            goto finish_free_kpages;
        }
    }
    kprintf("Free failed, address not found!\n");
finish_free_kpages:
    splx(spl);
}

#if 1
int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    vaddr_t vbase, vtop, stackbase, stacktop;
    paddr_t paddr;
    int i;
    int found_flag = 0;
    u_int32_t ehi, elo;
    struct addrspace *as;

    int spl = splhigh();

    faultaddress &= PAGE_FRAME;

    switch (faulttype) {
        case VM_FAULT_READONLY:
        /* We always create pages read-write, so we can't get this */
        panic("vm: got VM_FAULT_READONLY\n");
        case VM_FAULT_READ:
        case VM_FAULT_WRITE:
        break;
        default:
        splx(spl);
        return EINVAL;
    }

    as = curthread->t_vmspace;
    if (as == NULL) {
        /*
         * No address space set up. This is probably a kernel
         * fault early in boot. Return EFAULT so as to panic
         * instead of getting into an infinite faulting loop.
         */
        panic("No addrspace\n");
        return EFAULT;
    }

    for (i = 0; i < array_getnum(as->as_segments); i++) {
        struct as_segment *seg = array_getguy(as->as_segments, i);
        vbase = seg->vbase;
        vtop = seg->vbase + seg->npages * PAGE_SIZE;
        if (faultaddress >= vbase && faultaddress < vtop) { // We found the region we want
            paddr = (faultaddress - vbase) + 245760; // TODO: Fix this
            found_flag = 1;
            break;
        }
    }

    stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
    stacktop = USERSTACK;

    if (faultaddress >= stackbase && faultaddress < stacktop) {
        paddr = (faultaddress - stackbase) + 327680; // TODO: Fix this
        found_flag = 1;
    }

    if (!found_flag) {
        kprintf("Fault address not Found\n");
        splx(spl);
        return EFAULT;
    }

    /* make sure it's page-aligned */
    assert((paddr & PAGE_FRAME)==paddr);

    for (i=0; i<NUM_TLB; i++) {
        TLB_Read(&ehi, &elo, i);
        if (elo & TLBLO_VALID) {
            continue;
        }
        ehi = faultaddress;
        elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
        DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
        TLB_Write(ehi, elo, i);
        splx(spl);
        return 0;
    }

    kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
    splx(spl);
    return EFAULT;
}
#else
int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
    paddr_t paddr;
    int i;
    u_int32_t ehi, elo;
    struct addrspace *as;
    int spl;

    spl = splhigh();

    faultaddress &= PAGE_FRAME;

    switch (faulttype) {
        case VM_FAULT_READONLY:
        /* We always create pages read-write, so we can't get this */
        panic("vm: got VM_FAULT_READONLY\n");
        case VM_FAULT_READ:
        case VM_FAULT_WRITE:
        break;
        default:
        splx(spl);
        return EINVAL;
    }

    as = curthread->t_vmspace;
    if (as == NULL) {
        /*
         * No address space set up. This is probably a kernel
         * fault early in boot. Return EFAULT so as to panic
         * instead of getting into an infinite faulting loop.
         */
        return EFAULT;
    }

    /* Assert that the address space has been set up properly. */
    assert(as->as_vbase1 != 0);
    assert(as->as_pbase1 != 0);
    assert(as->as_npages1 != 0);
    assert(as->as_vbase2 != 0);
    assert(as->as_pbase2 != 0);
    assert(as->as_npages2 != 0);
    assert(as->as_stackpbase != 0);
    assert((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
    assert((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
    assert((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
    assert((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
    assert((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);


    vbase1 = as->as_vbase1;
    vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
    vbase2 = as->as_vbase2;
    vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
    stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
    stacktop = USERSTACK;

    if (faultaddress >= vbase1 && faultaddress < vtop1) {
        paddr = (faultaddress - vbase1) + as->as_pbase1;
    }
    else if (faultaddress >= vbase2 && faultaddress < vtop2) {
        paddr = (faultaddress - vbase2) + as->as_pbase2;
    }
    else if (faultaddress >= stackbase && faultaddress < stacktop) {
        paddr = (faultaddress - stackbase) + as->as_stackpbase;
    }
    else {
        splx(spl);
        return EFAULT;
    }

    /* make sure it's page-aligned */
    assert((paddr & PAGE_FRAME)==paddr);

    for (i=0; i<NUM_TLB; i++) {
        TLB_Read(&ehi, &elo, i);
        if (elo & TLBLO_VALID) {
            continue;
        }
        ehi = faultaddress;
        elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
        DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
        TLB_Write(ehi, elo, i);
        splx(spl);
        return 0;
    }

    kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
    splx(spl);
    return EFAULT;
}
#endif
