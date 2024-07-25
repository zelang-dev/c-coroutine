/* task_sched: preemptive multitasking in userspace based on SIGALRM signal.
 *
 * This program starts 3 sorting routines, execution of each is preempted by
 * SIGALRM signal, simulating an OS timer interrupt.  Each routine is an
 * execution context, which can do a voluntary scheduling (calling schedule()
 * directly) or be preempted by a timer, and in that case nonvoluntary
 * scheduling occurs.
 *
 * The default time slice is 10ms, that means that each 10ms SIGALRM fires and
 * next context is scheduled by round robin algorithm.
 */

#define _GNU_SOURCE
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/time.h>

#include "list.h"

static int preempt_count = 0;
static void preempt_disable(void) {
    preempt_count++;
}
static void preempt_enable(void) {
    preempt_count--;
}

static void local_irq_save(sigset_t *sig_set) {
    sigset_t block_set;
    sigfillset(&block_set);
    sigdelset(&block_set, SIGINT);
    sigprocmask(SIG_BLOCK, &block_set, sig_set);
}

static void local_irq_restore(sigset_t *sig_set) {
    sigprocmask(SIG_SETMASK, sig_set, NULL);
}

#define task_printf(...)     \
    ({                       \
        preempt_disable();   \
        printf(__VA_ARGS__); \
        preempt_enable();    \
    })

typedef void(task_callback_t)(void *arg);

typedef struct task_struct {
    list_head_t list;
    ucontext_t context;
    void *stack;
    task_callback_t *callback;
    void *arg;
    bool reap_self;
} task_struct_t;

static task_struct_t *task_current, task_main;
static LIST_HEAD(task_reap);

static void task_init(void) {
    INIT_LIST_HEAD(&task_main.list);
    task_current = &task_main;
}

static task_struct_t *task_alloc(task_callback_t *func, void *arg) {
    task_struct_t *task = calloc(1, sizeof(*task));
    task->stack = calloc(1, 1 << 20);
    task->callback = func;
    task->arg = arg;
    return task;
}

static void task_destroy(task_struct_t *task) {
    list_del(&task->list);
    free(task->stack);
    free(task);
}

static void task_switch_to(task_struct_t *from, task_struct_t *to) {
    task_current = to;
    swapcontext(&from->context, &to->context);
}

static void schedule(void) {
    sigset_t set;
    local_irq_save(&set);

    task_struct_t *next_task =
        list_first_entry(&task_current->list, task_struct_t, list);
    if (next_task) {
        if (task_current->reap_self)
            list_move(&task_current->list, &task_reap);
        task_switch_to(task_current, next_task);
    }

    task_struct_t *task, *tmp;
    list_for_each_entry_safe(task_struct_t, task, tmp, &task_reap, list) /* clean reaps */
        task_destroy(task);

    local_irq_restore(&set);
}

union task_ptr {
    void *p;
    int i[2];
};

static void local_irq_restore_trampoline(task_struct_t *task) {
    sigdelset(&task->context.uc_sigmask, SIGALRM);
    local_irq_restore(&task->context.uc_sigmask);
}

__attribute__((noreturn)) static void task_trampoline(int i0, int i1) {
    union task_ptr ptr = {.i = {i0, i1}};
    task_struct_t *task = ptr.p;

    /* We switch to trampoline with blocked timer.  That is safe.
     * So the first thing that we have to do is to unblock timer signal.
     * Paired with task_add().
     */
    local_irq_restore_trampoline(task);
    task->callback(task->arg);
    task->reap_self = true;
    schedule();
    __builtin_unreachable(); /* shall not reach here */
}

static void task_add(task_callback_t *func, void *param) {
    task_struct_t *task = task_alloc(func, param);
    if (getcontext(&task->context) == -1)
        abort();

    task->context.uc_stack.ss_sp = task->stack;
    task->context.uc_stack.ss_size = 1 << 20;
    task->context.uc_stack.ss_flags = 0;
    task->context.uc_link = NULL;

    union task_ptr ptr = {.p = task};
    makecontext(&task->context, (void (*)(void)) task_trampoline, 2, ptr.i[0],
                ptr.i[1]);

    /* When we switch to it for the first time, timer signal must be blocked.
     * Paired with task_trampoline().
     */
    sigaddset(&task->context.uc_sigmask, SIGALRM);

    preempt_disable();
    list_add_tail(&task->list, &task_main.list);
    preempt_enable();
}

static void timer_handler(int signo, siginfo_t *info, ucontext_t *ctx) {
    if (preempt_count) /* once preemption is disabled */
        return;

    /* We can schedule directly from sighandler because Linux kernel cares only
     * about proper sigreturn frame in the stack.
     */
    schedule();
}

static void timer_init(void) {
    struct sigaction sa = {
        .sa_handler = (void (*)(int)) timer_handler,
        .sa_flags = SA_SIGINFO,
    };
    sigfillset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);
}

static void timer_create(unsigned int usecs) {
    struct itimerval timer;
    //alarm(usecs);
    /* Configure the timer to expire after 250 msec... */
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = usecs * 1000;
    /* ... and every 250 msec after that. */
    //timer.it_interval.tv_sec = 0;
    //timer.it_interval.tv_usec = usecs * 1000;
    setitimer(ITIMER_REAL, &timer, NULL);
}
static void timer_cancel(void) {
    //alarm(0);
    setitimer(ITIMER_REAL, NULL, NULL);
}

static void timer_wait(void) {
    sigset_t mask;
    sigprocmask(0, NULL, &mask);
    sigdelset(&mask, SIGALRM);
    sigsuspend(&mask);
}

static int cmp_u32(const void *a, const void *b, void *arg) {
    uint32_t x = *(uint32_t *)a, y = *(uint32_t *)b;
    uint32_t diff = x ^ y;
    if (!diff)
        return 0; /* *a == *b */
    diff = diff | (diff >> 1);
    diff |= diff >> 2;
    diff |= diff >> 4;
    diff |= diff >> 8;
    diff |= diff >> 16;
    diff ^= diff >> 1;
    return (x & diff) ? 1 : -1;
}

static inline uint32_t random_shuffle(uint32_t x) {
    /* by Chris Wellons, see: <https://nullprogram.com/blog/2018/07/31/> */
    x ^= x >> 16;
    x *= 0x7feb352dUL;
    x ^= x >> 15;
    x *= 0x846ca68bUL;
    x ^= x >> 16;
    return x;
}

#define ARR_SIZE 1000000
static void sort(void *arg) {
    char *name = arg;

    preempt_disable();
    uint32_t *arr = malloc(ARR_SIZE * sizeof(uint32_t));
    preempt_enable();

    task_printf("[%s] %s: begin\n", name, __func__);

    uint32_t r = getpid();
    for (int i = 0; i < ARR_SIZE; i++)
        arr[i] = (r = random_shuffle(r));

    task_printf("[%s] %s: start sorting\n", name, __func__);

    qsort_r(arr, ARR_SIZE, sizeof(uint32_t), cmp_u32, name);

    for (int i = 0; i < ARR_SIZE - 1; i++)
        if (arr[i] > arr[i + 1]) {
            task_printf("[%s] %s: failed: a[%d]=%u, a[%d]=%u\n", name, __func__,
                        i, arr[i], i + 1, arr[i + 1]);
            abort();
        }

    task_printf("[%s] %s: end\n", name, __func__);

    preempt_disable();
    free(arr);
    preempt_enable();
}

int main() {
    timer_init();
    task_init();

    task_add(sort, "1"), task_add(sort, "2"), task_add(sort, "3"), task_add(sort, "4"), task_add(sort, "5");

    preempt_disable();
    timer_create(10); /* 10 ms */

    while (!list_empty(&task_main.list) || !list_empty(&task_reap)) {
        preempt_enable();
        timer_wait();
        preempt_disable();
    }

    preempt_enable();
    timer_cancel();

    return 0;
}
