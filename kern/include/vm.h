#ifndef _VM_H_
#define _VM_H_

#include <machine/vm.h>
#include <addrspace.h>

struct coremap_entry {
    struct addrspace *as; // Which address space does this page belongs to
    unsigned int status_flag : 1; // Indicate if this page is being used or not
    unsigned int kernel_flag : 1; // Indicate un-swappable kernel page
    unsigned int block_page_count : 16; // Number of pages in the block following this page(include this page)
};

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/


/* Initialization function */
void vm_bootstrap(void);

/* Get some physical pages */
paddr_t getppages(unsigned long npages);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(int npages);
void free_kpages(vaddr_t addr);

#endif /* _VM_H_ */
