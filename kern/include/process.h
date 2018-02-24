#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <types.h>
#include <array.h>
#include <thread.h>

struct process {
    pid_t pid;
    pid_t ppid;
    int exited_flag;
    int exit_code;
    struct thread* p_thread;
};

// Get a pid return 0 if no pid can be allocated
pid_t allocate_pid();

#endif
