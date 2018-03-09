#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <syscall.h>
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
sys_getpid(pid_t *retval)
{
    *retval = curthread->p_process->pid;
    return 0;
}
