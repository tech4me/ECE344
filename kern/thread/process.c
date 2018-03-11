#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <array.h>
#include <machine/spl.h>
#include <machine/trapframe.h>
#include <addrspace.h>
#include <vfs.h>
#include <process.h>

#define PREALLOCATE_PROCESS 32

extern struct thread *curthread;
extern struct array *zombies;

static pid_t pid_counter = 1;

/* An array of processes*/
static struct array *process_table;

static
pid_t
allocate_pid(void)
{
    if (pid_counter == 0) {
        return 0; // Unable to allocate a pid
    }

    return pid_counter++;
}

void
process_bootstrap(void)
{
    process_table = array_create();
    array_preallocate(process_table, PREALLOCATE_PROCESS);
    if (process_table == NULL) {
        panic("Cannot create process table array\n");
    }
}

struct process *
process_create(struct thread * thread)
{
    pid_t new_pid = allocate_pid();
    if (new_pid == 0) {
        return NULL;
    }

    struct process *process = kmalloc(sizeof(struct process));
    if (process == NULL) {
        return NULL;
    }

    // Init thread & process structure
    thread->p_process = process;
    process->pid = new_pid;
    if (new_pid == 1) {
        process->ppid = 0; // The first process's parent is pid 0
    } else {
        process->ppid = curthread->p_process->pid;
    }
    process->adopted_flag = 0;
    process->exited_flag = 0;
    process->exit_code = -1;
    process->p_thread = thread;
    struct semaphore *sem = sem_create("sem_exit", 0);
    if (sem == NULL) {
        kfree(process);
        return NULL;
    }
    process->sem_exit = sem;

    if (array_add(process_table, process)) {
        kfree(process);
        kfree(sem);
        return NULL;
    }

    return process;
}

void
process_exit(int exit_code)
{
    splhigh(); // Disable interrupt, the exit procedure should not be interleaved
    curthread->p_process->exited_flag = 1; // Process exited
    curthread->p_process->exit_code = exit_code;

    // Now all the child process will be orphant, we need to adopt them
    // Search through the process table, change all children's ppid
    int i;
    for (i = 0; i < array_getnum(process_table); i++) {
        struct process * process = array_getguy(process_table, i);
        if (process != NULL && process->ppid == curthread->p_process->pid) { // We found a child here, it should be a orphant now
            process->ppid = 1; // Now the init(boot/menu) process should adopt the child process
            process->adopted_flag = 1;
        }
    }

    V(curthread->p_process->sem_exit); // Now signal processes which are waiting

    // Now exit the thread
    thread_exit();
}

void
process_destroy(struct process *process)
{
    assert(curspl>0); // Interrupt should be off here
    assert(process != curthread->p_process);
    pid_t pid = process->pid;
    sem_destroy(process->sem_exit);
    kfree(process);
    array_setguy(process_table, pid - 1, NULL);
}

void
process_shutdown(void)
{
    int i;
    for (i = 0; i < array_getnum(process_table); i++) {
        kfree(array_getguy(process_table, i));
    }
    array_destroy(process_table);
    process_table = NULL;
}

int
process_fork(const char *name, struct trapframe *tf, pid_t *child_pid)
{
    int return_val;
    struct addrspace *child_addrspace;
    return_val = as_copy(curthread->t_vmspace, &child_addrspace); // Copy parent's address space
    if (return_val) {
        return return_val;
    }

    struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
    if (child_tf == NULL) {
        as_destroy(child_addrspace);
        return ENOMEM;
    }
    *child_tf = *tf; // Copy parent's trapframe

    struct thread *child_thread;
    return_val = thread_fork(name, (void *)child_tf, (unsigned long)child_addrspace, md_forkentry, &child_thread);
    if (return_val) {
        as_destroy(child_addrspace);
        kfree(child_tf);
        return return_val;
    }
    *child_pid = child_thread->p_process->pid;
    return 0;
}

int
process_wait(pid_t pid, int *status)
{
    int spl = splhigh();
    int return_val = 0;
    if (pid <= 0 || pid > array_getnum(process_table)) {
        *status = EINVAL;
        return_val = 0;
        goto done_wait;
    }
    struct process *process = array_getguy(process_table, pid - 1);
    if (process == NULL) { // Here either the process was never used or already reaped
        *status = EINVAL;
        return_val = 0;
        goto done_wait;
    }
    if (process->ppid != curthread->p_process->pid) { // waitpid can only be used on process's children
        *status = EINVAL;
        return_val = 0;
        goto done_wait;
    }
    if (process->exited_flag) {
        *status = 0;
        return_val = process->exit_code;
    } else {
        // Now we need to put the current process into sleep until the process that we are waiting for exits
        P(process->sem_exit);
        *status = 0;
        return_val = process->exit_code;
    }

    // Reap the child process
    int i;
    for (i=0; i<array_getnum(zombies); i++) {
        struct thread *z = array_getguy(zombies, i);
        if (z->p_process->pid == pid) {
            array_remove(zombies, i);
            break;
        }
    }
    thread_destroy(process->p_thread);
    process_destroy(process);

done_wait:
    splx(spl);
    return return_val;
}

int
process_execv(const char *program, unsigned long argc, char **argv)
{
    //if (*program == NULL)
    //    return -1;
    struct vnode *v;
    vaddr_t entrypoint, stackptr;
    int result;

    /* Open the file. */
    result = vfs_open((char *)program, O_RDONLY, &v);
    if (result) {
        return result;
    }

    // Save old address space before destroy it, in case load_elf fails
    assert(curthread->t_vmspace != NULL);
    struct addrspace *old_addrspace = curthread->t_vmspace;
    curthread->t_vmspace = NULL;

    /* Create a new address space. */
    curthread->t_vmspace = as_create();
    if (curthread->t_vmspace == NULL) {
        curthread->t_vmspace = old_addrspace;
        vfs_close(v);
        return ENOMEM;
    }

    /* Activate it. */
    as_activate(curthread->t_vmspace);

    /* Load the executable. */
    result = load_elf(v, &entrypoint);
    if (result) {
        as_destroy(curthread->t_vmspace);
        curthread->t_vmspace = old_addrspace;
        /* thread_exit destroys curthread->t_vmspace */
        vfs_close(v);
        return result;
    }

    /* Done with the file now. */
    vfs_close(v);

    /* Define the user stack in the address space */
    result = as_define_stack(curthread->t_vmspace, &stackptr);
    if (result) {
        as_destroy(curthread->t_vmspace);
        curthread->t_vmspace = old_addrspace;
        /* thread_exit destroys curthread->t_vmspace */
        return result;
    }

    // Now we can destroy the old one because the new one have been set up correctly
    as_destroy(old_addrspace);

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

    // Do this at the end to avoid double free
    unsigned long j;
    for (j = 0; j < argc; j++) {
        kfree(argv[j]);
    }
    kfree(argv);

    /* Warp to user mode. */
    md_usermode((int)argc, (userptr_t)stackptr, stackptr, entrypoint);

    /* md_usermode does not return */
    panic("md_usermode returned\n");
    return EINVAL;
}
