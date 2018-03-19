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
    //ram_getsize(&start, &end);

    // Only need the end because we know memory start at 0
    page_count = (int)(end / PAGE_SIZE); // We also do paging for exsisting kernel memory
    kprintf("***Total page count: %d\n", page_count);

    // Now we know where to put coremap
    coremap =  (struct coremap_entry *)PADDR_TO_KVADDR(start); // Append coremap right after the exsisting kernel memory

    start += page_count * sizeof(struct coremap_entry); // New start after saving enough save for coremap

    // Page align(to make our life easier)
    start = (start + (PAGE_SIZE - 1)) & -PAGE_SIZE;
    kprintf("***Page left after bootstrap: %d\n", (end - start)/PAGE_SIZE);

    kprintf("%x\n", PADDR_TO_KVADDR(start));
    // Here we init coremap
    unsigned int i;
    for (i = 0; i < page_count; i++) {
        if (i < start/PAGE_SIZE) {
            coremap[i].as = NULL; // There is no corresponding address space for the kernel yet
            coremap[i].status_flag = 1; // Used
            coremap[i].kernel_flag = 1; // Kernel
        } else {
            coremap[i].as = NULL; // There is no corresponding address space for the unmapped space
            coremap[i].status_flag = 0; // Used
            coremap[i].kernel_flag = 0; // Kernel
        }
    }

    // TODO: CHANGE THIS
    vm_bootstrap_flag = 0;
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
    } else {
        int spl = splhigh();
        // Here we need to look at which physical memory page is available
        // 1. Do we even have n pages available?
        // 2. Are they continues?
        int i, j, count = 0;
        for (i = 0; i < page_count; i) {
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
        } else {
            return PADDR_TO_KVADDR(i * PAGE_SIZE);
        }
    }
}

void
free_kpages(vaddr_t addr)
{
    // Determine how many pages we need to free
    // 1. Starting from the page that we got
    kprintf("Freeing 0x%x\n", addr);

    (void)addr;
}

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
