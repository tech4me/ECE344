#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <coremap.h>
#include <swap.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>

static int vm_bootstrap_flag = 0;

static
int
fault_handler(vaddr_t faultaddress, int faulttype, int segment_index, unsigned int permission, struct addrspace *as)
{
    assert(curspl>0); // Make sure interrupt is disabled
    if (coremap_get_avail_page_count() == 0) {
        kprintf("NOMEM!");
        return ENOMEM;
    }

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
            // 0. (Improvement) Check if the physical page have reference count = 1, if so we don't need to allocate new page
            // 1. Allocate a new page
            // 2. Copy the page content
            // 3. Decrease the page ref count by trying to free the page
            // 4. Update page table to use the new page
            // 5. Update the TLB entry to use the new page
            if (coremap_get_page_ref_count(e->pframe << PAGE_SHIFT) == 1) {
                paddr = e->pframe << PAGE_SHIFT;
            } else {
                paddr = coremap_alloc_upage(i);
                memmove((void *)PADDR_TO_KVADDR(paddr), (const void *)PADDR_TO_KVADDR(e->pframe << PAGE_SHIFT), PAGE_SIZE);
                coremap_free_page(e->pframe << PAGE_SHIFT);
                e->pframe = paddr >> PAGE_SHIFT;
            }
            e->cow = 0; // No copy-on-write anymore
            cow_flag = e->cow;
            ehi = faultaddress & TLBHI_VPAGE; // Set the pid field to 0
            int tlb_index = TLB_Probe(ehi, 0); // eho not used pass 0
            if (tlb_index >= 0) {
                TLB_Write(TLBHI_INVALID(tlb_index), TLBLO_INVALID(), tlb_index);
            }
#if 0
            assert(tlb_index >= 0);
            // No entry found in TLB that causes this fault, don't know why, but whatever
            // Note: my suspicion is that some how an context switch(TLB flush) happened between the fault and here, but how?
            // Edit: I know why now, in trap.c interrupt was re-enabled again... Gonna fix that...
            TLB_Read(&ehi, &elo, tlb_index);
            // Make sure the entry we are chaning is valid and not dirty
            assert(elo & TLBLO_VALID);
            assert((elo & TLBLO_DIRTY) == 0);
            // Now change entry to use the new physical page
            elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
            TLB_Write(ehi, elo, tlb_index);
            return 0;
#endif
        } else {
            // Simple TLB miss and no page fault
            paddr = e->pframe << PAGE_SHIFT;
            cow_flag = e->cow;
        }
    } else {
        // Below is on-demand paging
        paddr_t new_page = coremap_alloc_upage(array_getnum(a));
        // Now change page table to reflect this
        struct page_table_entry *entry = kmalloc(sizeof(struct page_table_entry));
        if (entry == NULL) {
            return ENOMEM;
        }
        entry->vframe = faultaddress >> PAGE_SHIFT;
        entry->pframe = new_page >> PAGE_SHIFT;
        entry->permission = permission;
        entry->cow = 0; // No copy-on-write
        err = array_add(a, entry);
        if (err) {
            return err;
        }
        // Now change paddr
        paddr = entry->pframe << PAGE_SHIFT;
        cow_flag = entry->cow;

        if (segment_index >= 0) { // Page haven't been read from disk yet
            // Add entry into TLB so we can load page
            err = 1;
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
                err = 0;
                break;
            }
            if (err) {
                ehi = faultaddress;
                if (cow_flag) {
                    elo = paddr | TLBLO_VALID;
                } else {
                    elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
                }
                TLB_Random(ehi, elo);
            }

            struct as_segment *seg = array_getguy(as->as_segments, segment_index);
            err = load_page_on_demand(seg->vnode, seg->uio, faultaddress - seg->vbase);
            if (err) {
                return err;
            }
            return 0;
        }
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
        return 0;
    }
    ehi = faultaddress;
    if (cow_flag) {
        elo = paddr | TLBLO_VALID;
    } else {
        elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
    }
    TLB_Random(ehi, elo);
    return 0;
}

void
vm_bootstrap(void)
{
    kprintf("VM bootstrap:\n");

    coremap_init();
    swap_init();

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
            err = fault_handler(faultaddress, faulttype, i, seg->permission, as);
            found_flag = 1;
            break;
        }
    }

    stackbase = USERSTACK - STACKPAGES * PAGE_SIZE;
    stacktop = USERSTACK;

    // Stack
    if (faultaddress >= stackbase && faultaddress < stacktop) {
        err = fault_handler(faultaddress, faulttype, -1, 6, as); // Read and Write
        found_flag = 1;
    }

    // Heap
    if (faultaddress >= as->as_heapbase && faultaddress < as->as_heapbase + as->as_heapsize) {
        err = fault_handler(faultaddress, faulttype, -1, 6, as); // Read and Write
        found_flag = 1;
    }

    if (!found_flag) {
        //kprintf("Fault address: 0x%x not Found\n", faultaddress);
        splx(spl);
        return EFAULT;
    }
    return err;
}
