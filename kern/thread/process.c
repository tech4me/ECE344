#include <process.h>

static pid_t pid_counter = 1;

pid_t allocate_pid()
{
    if (pid_counter == 0) {
        return 0; // Unable to allocate a pid
    }

    return pid_counter++;
}
