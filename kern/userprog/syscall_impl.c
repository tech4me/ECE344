#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <syscall.h>

int
sys_read(int fd, void *buf, size_t buflen, int *retval)
{
    if (fd != STDIN_FILENO) {
        kprintf("Right now read system call only support read from STDIN\n");
        *retval = -1;
        return EBADF;
    }
    if (buflen != 1) {
        kprintf("Right now read system call only support read one character\n");
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
    if (fd != STDOUT_FILENO) {
        kprintf("Right now write system call only support write to STDOUT\n");
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
