#include <types.h>
#include <lib.h>
#include <hello.h>

#include <syscall.h>

void hello()
{
    int ret;
    sys_read(10, NULL, 0, ret);
    kprintf("Hello World\n");
}
