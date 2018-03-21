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

#define STACKPAGES    32

static int vm_bootstrap_flag = 0;

static
int
fault_handler(vaddr_t faultaddress, int faulttype, unsigned int permission, struct addrspace *as)
{
    assert(as); // No NULL pointer pls
    assert(as->page_table); // No NULL pointer pls
    int spl = splhigh();

    u_int32_t ehi, elo;

    struct array *a = as->page_table;
    int found_flag = 0;
    paddr_t paddr = 0;
    // Iterate through our linked list and find the page table entry
    int i, err;
    struct page_table_entry *e;
    for (i = 0; i < array_getnum(a); i++) {
        e = array_getguy(a, i);
        if (e->valid) { // We only care about it if it is valid
            if ((vaddr_t)(e->vframe << PAGE_SHIFT) == faultaddress) { // We found the page
                found_flag = 1;
                break;
            }
        }
    }

    if (found_flag) { // We only have a TLB miss, page in memory
        paddr = e->pframe << PAGE_SHIFT;
    } else { // Right now it can only be page fault (page doesn't exsist yet)
        // Below is on-demand paging
        paddr_t new_page = coremap_alloc_page();
        // Now change page table to reflect this
        struct page_table_entry *entry = kmalloc(sizeof(struct page_table_entry));
        if (entry == NULL) {
            splx(spl);
            return ENOMEM;
        }
        entry->valid = 1;
        entry->vframe = faultaddress >> PAGE_SHIFT;
        entry->pframe = new_page >> PAGE_SHIFT;
        entry->permission = permission;
        err = array_add(a, entry);
        if (err) {
            splx(spl);
            return err;
        }
        // Now change paddr
        paddr = entry->pframe << PAGE_SHIFT;
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

    first_avail_page = start/PAGE_SIZE;

    // Here we init coremap
    unsigned int i;
    for (i = 0; i < page_count; i++) {
        if (i < first_avail_page) {
            coremap[i].as = NULL; // There is no corresponding address space for the kernel
            coremap[i].status = 1; // Used
            coremap[i].kernel = 1; // Kernel
            coremap[i].block_page_count = 1; // The page itself
        } else {
            coremap[i].as = NULL; // There is no corresponding address space for the unmapped space
            coremap[i].status = 0; // Unused
            coremap[i].kernel = 0; // Not Kernel
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
    for (i = first_avail_page; i < page_count; i++) {
        if (!coremap[i].status) {
            count++;
        }
    }
    assert(count >= npages);
    for (i = first_avail_page; i < page_count; i++) {
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
    splx(spl);
    if (count == 1) {
        panic("Unable to allocate continous memory");
    }

    // Now i is the offset we can use to start page allocation
    for (j = i; j < i + npages; j++) {
        if (j == i) {
            coremap[j].as = curthread->t_vmspace;
            coremap[j].status = 1;
            coremap[j].block_page_count = npages;
        } else {
            coremap[j].as = curthread->t_vmspace;
            coremap[j].status = 1;
            coremap[j].block_page_count = 0;
        }
    }
    return PADDR_TO_KVADDR(i * PAGE_SIZE);
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
        /* We always create pages read-write, so we can't get this */
        panic("vm: got VM_FAULT_READONLY\n");
        return EFAULT;
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
