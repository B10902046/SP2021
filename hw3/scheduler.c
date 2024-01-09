#include "threadtools.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

fd_set readfds, master;
struct timeval timeout = {0, 0};
int aaa = 1;
int maxfd;
/*
 * Print out the signal you received.
 * If SIGALRM is received, reset the alarm here.
 * This function should not return. Instead, call longjmp(sched_buf, 1).
 */
void sighandler(int signo) {
    // TODO
    if (signo == SIGALRM)
   {
      printf("caught SIGALRM\n");
      alarm(timeslice);
   }
   else if (signo == SIGTSTP)
   {
      printf("caught SIGTSTP\n");
   }
   sigprocmask(SIG_SETMASK, &base_mask, NULL);
   longjmp(sched_buf, 1);
}

/*
 * Prior to calling this function, both SIGTSTP and SIGALRM should be blocked.
 */
void scheduler() {
    // TODO
    int status;
    status = setjmp(sched_buf);
    if (status == 0) {
        maxfd = getdtablesize();
        FD_ZERO(&readfds);
        FD_ZERO(&master);
        if (rq_size == 0) 
            return;
        longjmp(ready_queue[rq_current]->environment, 1);
    }
    memcpy(&master, &readfds, sizeof(fd_set));
    if (select(maxfd, &master, NULL, NULL, &timeout) > 0) {
        int i = 0;
        while(i < wq_size) {
            if(FD_ISSET(waiting_queue[i]->fd, &master) == 1){
                FD_CLR(waiting_queue[i]->fd, &readfds);
                FD_CLR(waiting_queue[i]->fd, &readfds);
                ready_queue[rq_size++] = waiting_queue[i];
                for (int j = i; j < wq_size-1; j++) {
                    waiting_queue[j] = waiting_queue[j+1];
                }
                wq_size--;
            }else {
                i++;
            } 
        }
    }
    if (status == 1) {
        if (rq_size > 1) { 
            if (rq_current == rq_size - 1) {
                rq_current = 0;
            }else {
                rq_current++;
            }
        }
        longjmp(ready_queue[rq_current]->environment, 1);
    }else if (status == 3) {
        // exit
        free(ready_queue[rq_current]);
        if (rq_size > 1) {
            if (rq_current != rq_size - 1) {
                ready_queue[rq_current] = ready_queue[rq_size - 1];
                ready_queue[rq_size - 1] = NULL;
                rq_size--;
                longjmp(ready_queue[rq_current]->environment, 1);
            }else {
                ready_queue[rq_current] = NULL;
                rq_size--;
                rq_current = 0;
                longjmp(ready_queue[rq_current]->environment, 1);
            }
        }else {
            ready_queue[rq_current] = NULL;
            rq_size--;
            if (wq_size == 0) {
                return;
            }else {
                memcpy(&master, &readfds, sizeof(fd_set));
                int ret = select(maxfd, &master, NULL, NULL, NULL);
                int i = 0;
                while(i < wq_size) {
                    if(FD_ISSET(waiting_queue[i]->fd, &master) == 1){
                        FD_CLR(waiting_queue[i]->fd, &readfds);
                        ready_queue[rq_size++] = waiting_queue[i];
                        for (int j = i; j < wq_size-1; j++) {
                            waiting_queue[j] = waiting_queue[j+1];
                        }
                        wq_size--;
                    }else {
                        i++;
                    } 
                }
                rq_current = 0;
                longjmp(ready_queue[rq_current]->environment, 1);
            }
        }
    }else if (status == 2) {
        FD_SET(ready_queue[rq_current]->fd, &readfds);
        waiting_queue[wq_size++] = ready_queue[rq_current];
        if (rq_size > 1) {
            if (rq_current != rq_size - 1) {
                ready_queue[rq_current] = ready_queue[rq_size-1];
                ready_queue[rq_size - 1] = NULL;
                rq_size--;
                longjmp(ready_queue[rq_current]->environment, 1);
            }else {
                ready_queue[rq_current] = NULL;
                rq_size--;
                rq_current = 0;
                longjmp(ready_queue[rq_current]->environment, 1);
            }
        }else {
            ready_queue[rq_current] = NULL;
            rq_size--;
            if (wq_size == 0) {
                return;
            }else {
                memcpy(&master, &readfds, sizeof(fd_set));
                int ret = select(maxfd, &master, NULL, NULL, NULL);
                int i = 0;
                while(i < wq_size) {
                    if(FD_ISSET(waiting_queue[i]->fd, &master) == 1){
                        FD_CLR(waiting_queue[i]->fd, &readfds);
                        ready_queue[rq_size++] = waiting_queue[i];
                        for (int j = i; j < wq_size-1; j++) {
                            waiting_queue[j] = waiting_queue[j+1];
                        }
                        wq_size--;
                    }else {
                        i++;
                    } 
                }
                rq_current = 0;
                longjmp(ready_queue[rq_current]->environment, 1);
            }
        }
    }
    return;
}
