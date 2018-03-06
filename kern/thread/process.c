#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <array.h>
#include <machine/spl.h>
#include <machine/trapframe.h>
#include <addrspace.h>
#include <process.h>

#define PREALLOCATE_PROCESS 32

extern struct thread *curthread;

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
    process->exited_flag = 0;
    process->exit_code = -1;
    process->p_thread = thread;

    if (array_add(process_table, process)) {
        kfree(process);
        return NULL;
    }

    return process;
}

void
process_exit(int exit_code)
{
    thread_exit();
    curthread->p_process->exited_flag = 1; // Process exited
    curthread->p_process->exit_code = exit_code;
}

void
process_destroy(struct process *process)
{
    assert(process != curthread->p_process);
    kfree(process);
}

void
process_shutdown(void)
{
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
