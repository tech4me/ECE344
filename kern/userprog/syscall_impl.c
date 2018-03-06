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
    int err = copyout(&temp, buf, buflen);
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
    int err = copyin(buf, kern_buf, nbytes);
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
