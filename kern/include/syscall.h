#ifndef _SYSCALL_H_
#define _SYSCALL_H_

int sys_read(int fd, void *buf, size_t buflen, int *retval);

int sys_reboot(int code);

#endif /* _SYSCALL_H_ */
