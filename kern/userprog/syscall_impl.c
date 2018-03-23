#include <types.h>
#include <lib.h>
#include <kern/limits.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <clock.h>
#include <syscall.h>
#include <coremap.h>
#include <addrspace.h>
#include <thread.h>
#include <curthread.h>
#include <process.h>

int
sys__exit(int exitcode)
{
    process_exit(exitcode);
    panic("Process exit failed!");
    return 0;
}

int
sys_execv(const char *program, char **args, int *retval)
{
    if (args == NULL) {
        *retval = -1;
        return EFAULT;
    }

    // Need to copy data from userspace to kernel first
    int actual_length;
    char k_program[PATH_MAX];
    int j;

    int result = copyinstr((const_userptr_t)program, k_program, PATH_MAX, &actual_length);
    if (result) {
        *retval = -1;
        return result;
    }

    // Testify the pointer passed in is valid
    char *temp;
    result = copyin((const_userptr_t)args, (void *)&temp, sizeof(char **));
    if (result) {
        *retval = -1;
        return result;
    }

    int argc = 0;
    while (args[argc] != NULL) {
        argc++;
    }

    char ** k_argv = kmalloc(sizeof(char *) * argc);
    int err = 0;
    int i;
    for (i = 0; i < argc; i++) {
        k_argv[i] = kmalloc(sizeof(char) * (NAME_MAX + 1));
        result = copyinstr((const_userptr_t)args[i], k_argv[i], (NAME_MAX + 1), &actual_length);
        if (result) {
            err = result;
            goto done_sys_execv;
        }
    }
    err = process_execv(k_program, argc, k_argv); // Return normally should not happen, indicates error

done_sys_execv:
    for (j = 0; j < i; j++) {
        kfree(k_argv[j]);
    }
    kfree(k_argv);
    *retval = -1;
    return err;
}

int
sys_fork(struct trapframe *tf, pid_t *retval)
{
    pid_t child_pid;
    int fork_result = process_fork("child_process", tf, &child_pid);
    if (fork_result) { // Fork resulted non-zero, fork failed
        *retval = 0;
        return fork_result;
    }
    *retval = child_pid;
    return 0;
}

int
sys_waitpid(pid_t pid, int *status, int options, pid_t *retval)
{
    *retval = pid; // Waitpid success return the pid that we were waiting for
    if (options != 0) { // Check options
        *retval = -1;
        return EINVAL;
    }
    int wait_status;
    int exit_code = process_wait(pid, &wait_status);
    if (wait_status) {
        *retval = -1;
        return wait_status;
    }
    int err = copyout((const void *)&exit_code, (userptr_t)status, sizeof(int));
    if (err) {
        *retval = -1;
        return err;
    }
    return 0;
}

int
sys_read(int fd, void *buf, size_t buflen, int *retval)
{
    if (fd != STDIN_FILENO) {
        //kprintf("Right now read system call only support read from STDIN\n");
        *retval = -1;
        return EBADF;
    }
    if (buflen != 1) {
        //kprintf("Right now read system call only support read one character\n");
        *retval = -1;
        return EUNIMP;
    }
    char temp = getch();
    int err = copyout((const void *)&temp, (userptr_t)buf, buflen);
    if (err) {
        *retval = -1;
        return err;
    }
    kprintf("%c\n", temp);
    *retval = 1; // Read 1 byte
    return 0;
}

int
sys_write(int fd, const void *buf, size_t nbytes, int *retval)
{
    if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
        //kprintf("Right now write system call only support write to STDOUT and STDERR\n");
        *retval = -1;
        return EBADF;
    }
    char *kern_buf = (char*)kmalloc(sizeof(char) * (nbytes + 1)); // Additional space for NULL at the end
    int err = copyin((const_userptr_t)buf, (void *)kern_buf, nbytes);
    if (err) {
        *retval = -1;
        return err;
    }
    kern_buf[nbytes] = '\0'; // Append NULL
    kprintf("%s", kern_buf);
    kfree(kern_buf);
    *retval = nbytes;
    return 0;
}

int
sys_sbrk(intptr_t amount, void **retval)
{
    struct addrspace *as = curthread->t_vmspace;
    if ((int)as->as_heapsize + (int)amount < 0) { // Heapsize cannot be negative
        *retval = ((void *)-1);
        return EINVAL;
    } else if ((int)as->as_heapsize + (int)amount > (int)(HEAPPAGES * PAGE_SIZE)) {
        *retval = ((void *)-1);
        return ENOMEM;
    }

    // TODO: Change this after implemented swap so btree -h can work
    if (amount > 0 && ((unsigned int)(amount >> PAGE_SHIFT) > coremap_get_avail_page_count())) { // Exceed max number of page
        *retval = ((void *)-1);
        return ENOMEM;
    }
    *retval = (void *)(as->as_heapbase + as->as_heapsize);
    as->as_heapsize = amount + (int)as->as_heapsize;
    return 0;
}

int
sys_getpid(pid_t *retval)
{
    *retval = curthread->p_process->pid;
    return 0;
}

int
sys___time(time_t *seconds, unsigned long *nanoseconds, time_t *retval)
{
    time_t sec;
    u_int32_t nano_sec;
    gettime(&sec, &nano_sec);

    *retval = sec;
    int err;
    if (seconds != NULL) {
        err = copyout((const void *)&sec, (userptr_t)seconds, sizeof(time_t));
        if (err) {
            return err;
        }
    }
    if (nanoseconds != NULL) {
        err = copyout((const void *)&nano_sec, (userptr_t)nanoseconds, sizeof(u_int32_t));
        if (err) {
            return err;
        }
    }
    return 0;
}
