#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

int hThreadHandle = 0;
UINT TimerId;

#ifdef _WIN32
static VOID CALLBACK preempt_handler(HWND hWnd, UINT nMsg, UINT nIDEvent, DWORD dwTime) {
    printf("TimerId this is a test\n");
    fflush(stdout);
}
#else
static void preempt_handler(int signo, siginfo_t *info, ucontext_t *ctx) {
    if (preempt_count) /* once preemption is disabled */
        return;

    /* We can schedule directly from sighandler because Linux kernel cares only
     * about proper sigreturn frame in the stack.
     */
    schedule();
}
#endif

#ifdef _WIN32
static int preempt_thread(void *args) {
    unsigned int ms = *(unsigned int *)args;
    free(args);
    MSG Msg;

    TimerId = SetTimer(NULL, 0, ms, (TIMERPROC)&preempt_handler);
    printf("TimerId: %d\n", TimerId);
    if (!TimerId)
        return 16;
    while (GetMessage(&Msg, NULL, 0, 0))
        DispatchMessage(&Msg);
}

void preempt_init(unsigned int usecs) {
    unsigned int *ms = malloc(sizeof(unsigned int));
    *ms = usecs;
    _beginthread((_beginthread_proc_type)preempt_thread, 0, ms);
}
#else
void preempt_init(unsigned int usecs) {
    struct itimerval timer;
    struct sigaction sa = {
        .sa_handler = (void (*)(int)) timer_handler,
        .sa_flags = SA_SIGINFO,
    };
    sigfillset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    /* Configure the timer to expire after `usecs` msec... */
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = usecs * 1000;
    /* ... and every `usecs` msec after that. */
    //timer.it_interval.tv_sec = 0;
    //timer.it_interval.tv_usec = usecs * 1000;
    setitimer(ITIMER_REAL, &timer, NULL);
}
#endif

int main(int argc, char *argv[], char *envp[]) {
    int Counter = 0;
    preempt_init(1000);
    usleep(5000);
    return 0;
}
