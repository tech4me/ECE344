#ifndef _ADDRSPACE_H_
#define _ADDRSPACE_H_

#include <array.h>
#include <uio.h>
#include <vm.h>
#include "opt-dumbvm.h"

struct vnode;

struct page_table_entry {
    u_int32_t vframe : 20; // 32 - 12 = 20
    u_int32_t pframe : 20; // 32 - 12 = 20
    u_int32_t permission : 3; // *nix style permission
    u_int32_t cow : 1; // 0 means that tlb entry should be dirty, 1 means that tlb should not be dirty(readonly)
    u_int32_t swapped : 1; // Indicates if this page is in memory or in swap file
    u_int32_t swap_file_frame : 16; // File frame for the page in swap
    u_int32_t swap_coremap_ref_count : 16; // When the page is swapped out the reference count is saved here
};

// This defines how each segment exist in the addrspace
struct as_segment {
    vaddr_t vbase;
    size_t npages;
    u_int32_t permission; // We use *nix style permission 0-7
    struct vnode *vnode; // Used for on-demand paging
    struct uio uio; // Used for on-demand paging
};

/*
 * Address space - data structure associated with the virtual memory
 * space of a process.
 */

struct addrspace {
#if OPT_DUMBVM
    vaddr_t as_vbase1;
    paddr_t as_pbase1;
    size_t as_npages1;
    vaddr_t as_vbase2;
    paddr_t as_pbase2;
    size_t as_npages2;
    paddr_t as_stackpbase;
#else
    struct array *as_segments;
    vaddr_t as_heapbase;
    size_t as_heapsize;
    vaddr_t as_stackbase;
    struct array *page_table;
#endif
};

/*
 * Functions in addrspace.c:
 *
 *    as_create - create a new empty address space. You need to make
 *                sure this gets called in all the right places. You
 *                may find you want to change the argument list. May
 *                return NULL on out-of-memory error.
 *
 *    as_copy   - create a new address space that is an exact copy of
 *                an old one. Probably calls as_create to get a new
 *                empty address space and fill it in, but that's up to
 *                you.
 *
 *    as_activate - make the specified address space the one currently
 *                "seen" by the processor. Argument might be NULL,
 *        meaning "no particular address space".
 *
 *    as_destroy - dispose of an address space. You may need to change
 *                the way this works if implementing user-level threads.
 *
 *    as_define_region - set up a region of memory within the address
 *                space.
 *
 *    as_prepare_load - this is called before actually loading from an
 *                executable into the address space.
 *
 *    as_complete_load - this is called when loading from an executable
 *                is complete.
 *
 *    as_define_stack - set up the stack region in the address space.
 *                (Normally called *after* as_complete_load().) Hands
 *                back the initial stack pointer for the new process.
 */

struct addrspace *as_create(void);
int               as_copy(struct addrspace *src, struct addrspace **ret);
void              as_activate(struct addrspace *);
void              as_destroy(struct addrspace *);

int               as_define_region(struct addrspace *as,
                   vaddr_t vaddr, size_t sz,
                   int readable,
                   int writeable,
                   int executable);
int       as_prepare_load(struct addrspace *as);
int       as_complete_load(struct addrspace *as);
int               as_define_stack(struct addrspace *as, vaddr_t *initstackptr);

/*
 * Functions in loadelf.c
 *    load_elf - load an ELF user program executable into the current
 *               address space. Returns the entry point (initial PC)
 *               in the space pointed to by ENTRYPOINT.
 */

int load_elf(struct vnode *v, vaddr_t *entrypoint);

// The on-demand version of load_elf
int load_elf_on_demand(struct vnode *v, vaddr_t *entrypoint);

// Load a page from a segment
int load_page_on_demand(struct vnode *v, struct uio u, off_t page_offset);

#endif /* _ADDRSPACE_H_ */
