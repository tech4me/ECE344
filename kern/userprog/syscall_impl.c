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
        *retval = EUNIMP;
        return -1;
    }
    if (buflen != 1) {
        kprintf("Right now read system call only support read one character\n");
        *retval = EUNIMP;
        return -1;
    }
    char temp = getch();
    *(char *)buf = temp;
    kprintf("%c\n", temp);
    return 0;
}
