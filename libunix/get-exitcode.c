#include <sys/types.h>
#include <sys/wait.h>
#include "libunix.h"

// non-blocking check if <pid> exited cleanly.
// returns:
//   - 0 if not exited;
//   - 1 if exited cleanly (exitcode in <status>, 
//   - -1 if exited with a crash (status holds reason)
int child_clean_exit_noblk(int pid, int *status) {
    int ret = waitpid(pid, status, WNOHANG);
    
    // Error or no status change
    if(ret <= 0)
        return 0;
    
    // Child exited, check how
    if(WIFEXITED(*status)) {
        *status = WEXITSTATUS(*status);
        return 1;
    } else {
        // Child crashed or terminated by signal
        if(WIFSIGNALED(*status))
            *status = WTERMSIG(*status);
        return -1;
    }
}

/*
 * blocking check that child <pid> exited cleanly.
 * returns:
 *  - 1 if exited cleanly, exitcode in <status>
 *  - 0 if crashed, reason in <status> .
 */
int child_clean_exit(int pid, int *status) {
    if(waitpid(pid, status, 0) < 0)
        sys_die(waitpid, "waitpid failed");
    
    if(WIFEXITED(*status)) {
        *status = WEXITSTATUS(*status);
        return 1;
    } else {
        if(WIFSIGNALED(*status))
            *status = WTERMSIG(*status);
        return 0;
    }
}
