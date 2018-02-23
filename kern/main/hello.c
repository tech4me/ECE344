#include <types.h>
#include <lib.h>
#include <hello.h>

#include <syscall.h>

void hello()
{
    int ret;
    char a;
    //sys_read(0, &a, 1, ret);
    kprintf("Hello World\n");
}
