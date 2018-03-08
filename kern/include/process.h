#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <types.h>
#include <synch.h>
#include <thread.h>

struct process {
    pid_t pid;
    pid_t ppid;
    int exited_flag;
    int exit_code;
    struct thread *p_thread;
    struct semaphore *sem_exit;
};

// Boot process start sequence
void process_bootstrap(void);

// Helper to create new process
struct process * process_create(struct thread *thread);

// Process exit, change status and save return value
void process_exit(int exit_code);

// Remove signle process structure
void process_destroy(struct process *process);

// Completely stop processes
void process_shutdown(void);

// Helper function to fork process
int process_fork(const char *name, struct trapframe *tf, pid_t *child_pid);

// Helper function to wait for other process
int process_wait(pid_t pid, int *status);

#endif
