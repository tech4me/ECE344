#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <types.h>

int sys_fork(pid_t *retval);

int sys_read(int fd, void *buf, size_t buflen, int *retval);

int sys_write(int fd, const void *buf, size_t nbytes, int *retval);

int sys_reboot(int code);

#endif /* _SYSCALL_H_ */
