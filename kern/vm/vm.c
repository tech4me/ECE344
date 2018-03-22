#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <coremap.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>

#define STACKPAGES 32

static int vm_bootstrap_flag = 0;

static
int
fault_handler(vaddr_t faultaddress, int faulttype, unsigned int permission, struct addrspace *as)
{
    assert(as); // No NULL pointer pls
    assert(as->page_table); // No NULL pointer pls
    int spl = splhigh();

    u_int32_t ehi, elo;

    paddr_t paddr = 0;
    unsigned int cow_flag;
    int found_flag = 0;
    struct array *a = as->page_table;

    // Iterate through our array and find the page table entry
    int i, err;
    struct page_table_entry *e;
    for (i = 0; i < array_getnum(a); i++) {
        e = array_getguy(a, i);
        if ((vaddr_t)(e->vframe << PAGE_SHIFT) == faultaddress) { // We found the page
            found_flag = 1;
            break;
        }
    }

    if (found_flag) { // We found the page in page table
        if (faulttype == VM_FAULT_READONLY) { // Here we detected a write on shared page
            // Copy-On-Write shared page
            // 1. Allocate a new page
            // 2. Copy the page content
            // 3. Decrease the page ref count by trying to free the page
            // 4. Update page table to use the new page
            // 5. Update the TLB entry to use the new page
            paddr = coremap_alloc_kpage();
            memmove((void *)PADDR_TO_KVADDR(paddr), (const void *)PADDR_TO_KVADDR(e->pframe << PAGE_SHIFT), PAGE_SIZE);
            coremap_free_page(e->pframe << PAGE_SHIFT);
            e->pframe = paddr >> PAGE_SHIFT;
            e->cow = 0; // No copy-on-write anymore
            ehi = faultaddress;
            int tlb_index = TLB_Probe(ehi, 0); // eho not used pass 0
            assert(tlb_index >= 0);
            TLB_Read(&ehi, &elo, tlb_index);
            // Make sure the entry we are chaning is valid and not dirty
            assert(elo & TLBLO_VALID);
            assert((elo & TLBLO_DIRTY) == 0);
            // Now change entry to use the new physical page
            elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
            TLB_Write(ehi, elo, tlb_index);
            splx(spl);
            return 0;
        } else {
            // Simple TLB miss and no page fault
            paddr = e->pframe << PAGE_SHIFT;
            cow_flag = e->cow;
        }
    } else { // Right now it can only be page fault (page doesn't exsist yet)
        // Below is on-demand paging
        paddr_t new_page = coremap_alloc_kpage();
        // Now change page table to reflect this
        struct page_table_entry *entry = kmalloc(sizeof(struct page_table_entry));
        if (entry == NULL) {
            splx(spl);
            return ENOMEM;
        }
        entry->vframe = faultaddress >> PAGE_SHIFT;
        entry->pframe = new_page >> PAGE_SHIFT;
        entry->permission = permission;
        entry->cow = 0; // No copy-on-write
        err = array_add(a, entry);
        if (err) {
            splx(spl);
            return err;
        }
        // Now change paddr
        paddr = entry->pframe << PAGE_SHIFT;
        cow_flag = entry->cow;
    }

    /* make sure it's page-aligned */
    assert((paddr & PAGE_FRAME)==paddr);

    for (i=0; i<NUM_TLB; i++) {
        TLB_Read(&ehi, &elo, i);
        if (elo & TLBLO_VALID) {
            continue;
        }
        ehi = faultaddress;
        if (cow_flag) {
            elo = paddr | TLBLO_VALID;
        } else {
            elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
        }
        TLB_Write(ehi, elo, i);
        splx(spl);
        return 0;
    }

    kprintf("Ran out of TLB entries - cannot handle page fault\n");
    splx(spl);
    return EFAULT;
}

void
vm_bootstrap(void)
{
    kprintf("VM bootstrap:\n");

    coremap_init();

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
    paddr_t paddr;
    if (!vm_bootstrap_flag) {
        paddr = getppages(npages);
        if (paddr==0) {
            return 0;
        }
        return PADDR_TO_KVADDR(paddr);
    }
    int spl = splhigh();
    vaddr_t vaddr = PADDR_TO_KVADDR(coremap_alloc_kpages(npages));
    splx(spl);
    return vaddr;
}

void
free_kpages(vaddr_t addr)
{
    int spl = splhigh();
    coremap_free_pages(VADDR_TO_KPADDR(addr));
    splx(spl);
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    vaddr_t vbase, vtop, stackbase, stacktop;
    int i, err = 0;
    int found_flag = 0;
    struct addrspace *as;

    int spl = splhigh();

    faultaddress &= PAGE_FRAME;

    switch (faulttype) {
        case VM_FAULT_READONLY:
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
            err = fault_handler(faultaddress, faulttype, seg->permission, as);
            found_flag = 1;
            break;
        }
    }

    stackbase = USERSTACK - STACKPAGES * PAGE_SIZE;
    stacktop = USERSTACK;

    if (faultaddress >= stackbase && faultaddress < stacktop) {
        err = fault_handler(faultaddress, faulttype, 6, as); // Read and Write
        found_flag = 1;
    }

    // TODO: heap here

    if (!found_flag) {
        kprintf("Fault address: 0x%x not Found\n", faultaddress);
        splx(spl);
        return EFAULT;
    }
    return err;
}
