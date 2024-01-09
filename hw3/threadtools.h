#ifndef THREADTOOL
#define THREADTOOL
#include <setjmp.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/signal.h>

#define THREAD_MAX 16  // maximum number of threads created
#define BUF_SIZE 512
struct tcb {
    int id;  // the thread id
    jmp_buf environment;  // where the scheduler should jump to
    int arg;  // argument to the function
    int fd;  // file descriptor for the thread
    char buf[BUF_SIZE];  // buffer for the thread
    int i, x, y;  // declare the variables you wish to keep between switches
};
extern int aaa;
extern int timeslice;
extern jmp_buf sched_buf;
extern struct tcb *ready_queue[THREAD_MAX], *waiting_queue[THREAD_MAX];
/*
 * rq_size: size of the ready queue
 * rq_current: current thread in the ready queue
 * wq_size: size of the waiting queue
 */
extern int rq_size, rq_current, wq_size;
/*
* base_mask: blocks both SIGTSTP and SIGALRM
* tstp_mask: blocks only SIGTSTP
* alrm_mask: blocks only SIGALRM
*/
extern sigset_t base_mask, tstp_mask, alrm_mask;
/*
 * Use this to access the running thread.
 */
#define RUNNING (ready_queue[rq_current])

void sighandler(int signo);
void scheduler();

#define thread_create(func, id, arg) {  \
    func(id, arg);                      \
}

#define thread_setup(id, arg) {                                            \
    if (aaa == 1) {                                                        \
        rq_size = 0;                                                       \
        rq_current = 0;                                                    \
        wq_size = 0;                                                       \
        aaa++;                                                             \
    }                                                                      \
    ready_queue[rq_size] = (struct tcb *)(malloc(sizeof(struct tcb)));  \
    ready_queue[rq_size]->id = id;                                      \
    ready_queue[rq_size]->arg = arg;                                    \
    char str[32];                                                          \
    sprintf(str, "%d_%s", id, __func__);                                   \
    mkfifo(str, 0777);                                                     \
	ready_queue[rq_size]->fd = open(str, O_RDONLY | O_NONBLOCK, 0777);  \
    rq_size++;                                                             \
    if(setjmp(ready_queue[rq_size-1]->environment) == 0) {                \
        return;                                                            \
    }                                                                      \
}

#define thread_exit() {                                             \
    char str[32];                                                   \
    sprintf(str, "%d_%s", ready_queue[rq_current]->id, __func__);   \
    unlink(str);                                                    \
    close(ready_queue[rq_current]->fd);                             \
    longjmp(sched_buf, 3);                                          \
}

#define thread_yield() {                                      \
    if (setjmp(ready_queue[rq_current]->environment) == 0) {  \
        sigprocmask(SIG_SETMASK, &alrm_mask, NULL);           \
        sigprocmask(SIG_SETMASK, &tstp_mask, NULL);           \
        sigprocmask(SIG_SETMASK, &base_mask, NULL);           \
    }                                                         \
}

#define async_read(count) ({                                                \
    if (setjmp(ready_queue[rq_current]->environment) == 0) {                \
        longjmp(sched_buf, 2);                                              \
    }                                                                      \
    memset(ready_queue[rq_current]->buf, 0, BUF_SIZE);                      \
    read(ready_queue[rq_current]->fd, ready_queue[rq_current]->buf, count); \
})

#endif // THREADTOOL
