#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <types.h>
#include <machine/trapframe.h>

int sys__exit(int exitcode);

int sys_execv(const char *program, char **args, int *retval);

int sys_fork(struct trapframe *tf, pid_t *retval);

int sys_waitpid(pid_t pid, int *status, int options, pid_t *retval);

int sys_read(int fd, void *buf, size_t buflen, int *retval);

int sys_write(int fd, const void *buf, size_t nbytes, int *retval);

int sys_reboot(int code);

int sys_sbrk(intptr_t amount, void **retval);

int sys_getpid(pid_t *retval);

int sys___time(time_t *seconds, unsigned long *nanoseconds, time_t *retval);

#endif /* _SYSCALL_H_ */
