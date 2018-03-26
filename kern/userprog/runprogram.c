/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>

int
runprogram(char *progname, unsigned long argc, char **argv)
{
    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    int result;

    /* Open the file. */
    result = vfs_open(progname, O_RDONLY, &v);
    if (result) {
        return result;
    }

    /* We should be a new thread. */
    assert(curthread->t_vmspace == NULL);

    /* Create a new address space. */
    curthread->t_vmspace = as_create();
    if (curthread->t_vmspace==NULL) {
        vfs_close(v);
        return ENOMEM;
    }

    /* Activate it. */
    as_activate(curthread->t_vmspace);

    /* Load the executable. */
    result = load_elf_on_demand(v, &entrypoint);
    if (result) {
        /* thread_exit destroys curthread->t_vmspace */
        vfs_close(v);
        return result;
    }

    // We don't close the file now any more because more read might happen during process execution
    /* Done with the file now. */
    //vfs_close(v);

    /* Define the user stack in the address space */
    result = as_define_stack(curthread->t_vmspace, &stackptr);
    if (result) {
        /* thread_exit destroys curthread->t_vmspace */
        return result;
    }

    // Copy arguments to user stack
    // First argument to md_usermode will be user main's first parameter(argc)
    // Second argument to md_usermode will be user main's second parameter(argv)

    // Allocate temporary space to save pointer to each string
    char ** temp_arg_ptr = kmalloc(sizeof(char *) * argc);

    // Store each string
    unsigned int i;
    for (i = 0; i < argc; i++) {
        int length = strlen(argv[i]) + 1; // The length of the string + 1 for \0
        stackptr -= length;
        result = copyout((const void *)argv[i], (userptr_t)stackptr, (size_t)length);
        if (result) {
            kfree(temp_arg_ptr);
            return result;
        }
        temp_arg_ptr[i] = (char *)stackptr;
    }

    // Take cares of allignment problem here since the strings are appended together
    stackptr -= (stackptr % 4);

    // Store pointers to each string (Note: the order is very important)
    // Program name must be at the lowest address, and the rest gets higher and higher
    stackptr -= (sizeof(char *) * argc); // Allocate stack for all char *
    result = copyout((const void *)temp_arg_ptr, (userptr_t)stackptr, (size_t)(sizeof(char *) * argc));
    kfree(temp_arg_ptr);
    if (result) {
        return result;
    }

    /* Warp to user mode. */
    md_usermode((int)argc, (userptr_t)stackptr, stackptr, entrypoint);

    /* md_usermode does not return */
    panic("md_usermode returned\n");
    return EINVAL;
}

