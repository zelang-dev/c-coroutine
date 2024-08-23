#include "coroutine.h"

typedef struct {
    bool is_main;

    bool info_log;

    /* has the coroutine sleep/wait system started. */
    bool sleep_activated;

    /* number of coroutines waiting in sleep mode. */
    int sleeping_counted;

    /* track the number of coroutines used */
    int used_count;

    /* indicator for thread termination. */
    u32 exiting;

    /* thread id assigned by `gq_sys` */
    u32 thrd_id;

    /* number of other coroutine that ran while the current coroutine was waiting.*/
    u32 num_others_ran;

    u32 sleep_id;

    routine_t *sleep_task;

    /* record which coroutine sleeping in scheduler */
    scheduler_t sleeping;

    /* record which coroutine is executing for scheduler */
    routine_t *running;

    /* coroutines's FIFO scheduler queue */
    scheduler_t run_queue;

    uv_loop_t interrupt_buffer[1];
    uv_loop_t *interrupt_handle;
    uv_loop_t *interrupt_default;
    uv_args_t *uv_args;

    /* Store/hold the registers of the default coroutine thread state,
    allows the ability to switch from any function, non coroutine context. */
    routine_t active_buffer[1];
    /* Variable holding the current running coroutine per thread. */
    routine_t *active_handle;
    /* Variable holding the main target that gets called once an coroutine
    function fully completes and return. */
    routine_t *main_handle;
    /* Variable holding the previous running coroutine per thread. */
    routine_t *current_handle;
} thread_processor_t;
thrd_static(thread_processor_t, thread, NULL)

static void(fastcall *co_swap)(routine_t *, routine_t *) = 0;
static char error_message[ERROR_SCRAPE_SIZE] = {0};

static volatile sig_atomic_t can_cleanup = true;
static int main_argc;
static char **main_argv;

/* These are non-NULL pointers that will result in page faults under normal
 * circumstances, used to verify that nobody uses non-initialized entries.
 */
static scheduler_t *EMPTY = (scheduler_t *)0x100, *ABORT = (scheduler_t *)0x200;

/* Global queue system atomic data. */
atomic_deque_t gq_sys = {0};

int thrd_main(void_t args);
int coroutine_loop(int);
void coroutine_interrupt(void);

/* Initializes new coroutine, platform specific. */
routine_t *co_derive(void_t, size_t);

/* Create new coroutine. */
routine_t *co_create(size_t, callable_t, void_t);

/* Create a new coroutine running func(arg) with stack size
and startup type: `RUN_NORMAL`, `RUN_MAIN`, `RUN_SYSTEM`, `RUN_EVENT`. */
u32 create_routine(callable_t, void_t, u32, run_states);

/* Sets the current coroutine's name.*/
void coroutine_name(char *, ...);

/* Mark the current coroutine as a ``system`` coroutine. These are ignored for the
purposes of deciding the program is done running */
void coroutine_system(void);

/* Sets the current coroutine's state name.*/
void coroutine_state(char *, ...);

/* Returns the current coroutine's state name. */
char *coroutine_get_state(void);

/* Check `local` run queue `head` for not `NULL`. */
CO_FORCE_INLINE bool sched_active(void) {
    return !is_empty(thread()->run_queue.head);
}

/* Check `global` run queue `head` for not `NULL`. */
CO_FORCE_INLINE bool sched_is_active(void) {
    return !is_empty(((scheduler_t *)atomic_load(&gq_sys.run_queue))->head);
}

CO_FORCE_INLINE bool sched_is_started(void) {
    return atomic_flag_load(&gq_sys.is_started);
}

/* Check `global` run queue count for zero. */
CO_FORCE_INLINE bool sched_is_empty(void) {
    return atomic_load(&gq_sys.used_count) == 0;
}

/* Check `global` run queue count for tasks available over threshold for assigning. */
CO_FORCE_INLINE bool sched_is_takeable(void) {
    return atomic_load(&gq_sys.used_count) > 0 && !gq_sys.is_takeable;
}

/* Check for available/assigned tasks for `current` thread. */
CO_FORCE_INLINE bool sched_is_available(void) {
    return gq_sys.is_multi && atomic_load(&gq_sys.available[thread()->thrd_id]) > 0;
}

/* Check for thread steal status marking,
the thread to receive another thread's `run queue`. */
CO_FORCE_INLINE bool sched_is_stealable(size_t id) {
    return gq_sys.is_multi
        && atomic_flag_load(&gq_sys.any_stealable[id])
        && atomic_load(&gq_sys.stealable_thread[id]) == thread()->thrd_id;
}

/* Check `current` thread steal status marking,
the thread giving up tasks to another thread's `run queue`. */
CO_FORCE_INLINE bool sched_is_stolen(void) {
    return gq_sys.is_multi
        && atomic_flag_load(&gq_sys.any_stealable[thread()->thrd_id])
        && atomic_load(&gq_sys.stealable_thread[thread()->thrd_id]) == gq_sys.thread_invalid;
}

static bool sched_is_running(void) {
    if (!gq_sys.is_multi)
        return false;

    size_t i;
    int *has;
    for (i = 0; i < gq_sys.cpu_count; i++) {
        has = (int *)atomic_load(&gq_sys.count[i]);
        if (is_empty(has) || (!is_empty(has) && *has <= 0))
            continue;

        return true;
    }

    return false;
}

/* Check all threads run queue count for tasks to steal,
if so mark the other thread stealable and pause `current` thread,
until other thread post available tasks to `global` run queue. */
static bool sched_is_any_available(void) {
    if (!gq_sys.is_multi || gq_sys.is_finish)
        return false;

    int *has;
    size_t i, has_items, id = 0, last = 0;
    for (i = 0; i < gq_sys.cpu_count; i++) {
        has = (int *)atomic_load(&gq_sys.count[i]);
        if (is_empty(has) || (!is_empty(has) && *has <= 0))
            continue;

        has_items = *has;
        if (has_items > 3 && has_items > last) {
            id = i;
            last = has_items;
        }
    }

    if (last != 0) {
        atomic_store(&gq_sys.stealable_thread[id], gq_sys.thread_invalid);
        atomic_store(&gq_sys.stealable_thread[thread()->thrd_id], id);

        atomic_flag_test_and_set(&gq_sys.any_stealable[id]);
        atomic_flag_test_and_set(&gq_sys.any_stealable[thread()->thrd_id]);
        return true;
    }

    return false;
}

/* Transfer an thread's `marked` local run queue to `global` run queue,
the amount/count was set by `sched_is_any_available`. */
void sched_post_stolen(thread_processor_t *r) {
    if (!gq_sys.is_multi)
        return;

    routine_t *t;
    size_t i, id, available, amount = 0, used_count = r->used_count;
    for (i = 0; i < gq_sys.thread_count; i++) {
        if (sched_is_stealable(i)) {
            id = i;
            break;
        }
    }

    available = used_count / 2;
    for (i = 0; i < used_count; i++) {
        if (&r->run_queue.head != NULL) {
            if (available == 0)
                break;

            t = sched_dequeue(&r->run_queue);
            if (t->run_code == RUN_NORMAL) {
                r->used_count--;
                available--;
                amount++;
                t->taken = false;
                sched_enqueue(t);
            } else {
                t->taken = true;
                sched_enqueue(t);
            }
        }
    }

    if (amount > 0)
        gq_sys.is_takeable++;

    atomic_fetch_add(&gq_sys.used_count, amount);
    atomic_store(&gq_sys.stealable_amount[id], amount);
    atomic_flag_clear(&gq_sys.any_stealable[id]);
    thrd_yield();

    atomic_store(&gq_sys.stealable_thread[thread()->thrd_id], 0);
    atomic_flag_clear(&gq_sys.any_stealable[thread()->thrd_id]);
}

/* Calculate the amount each thread can take from `global` run queue,
the amount/count is over threshold set by `sched_is_takeable`. */
static void sched_post_available(void) {
    if (!gq_sys.is_multi)
        return;

    /* Return if previous posting wasn't taken yet. */
    if (atomic_load(&gq_sys.available[gq_sys.cpu_count]) > 0)
        return;

    size_t available, i, active = atomic_load_explicit(&gq_sys.used_count, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    available = active / gq_sys.thread_count;
    bool is_needed = available > gq_sys.cpu_count;
    for (i = 0; i < gq_sys.thread_count; i++) {
        atomic_fetch_add(&gq_sys.available[i], (is_needed ? available : 0));
        active -= (is_needed ? available : 0);
    }

    atomic_store_explicit(&gq_sys.used_count, (is_needed ? active : 0), memory_order_release);
    if (!is_needed)
        atomic_fetch_add(&gq_sys.available[gq_sys.cpu_count], active);
}

#ifdef _WIN32
static size_t sched_cpu_count(void) {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return (size_t)system_info.dwNumberOfProcessors;
}
#elif (defined(__linux__) || defined(__linux))
#include <sched.h>
static size_t sched_cpu_count(void) {
    cpu_set_t cpuset;
    sched_getaffinity(0, sizeof(cpuset), &cpuset);
    return CPU_COUNT(&cpuset);
}
#else
static size_t sched_cpu_count(void) {
    return sysconf(_SC_NPROCESSORS_ONLN);
}
#endif

static void sched_init(bool is_main, u32 thread_id) {
    thread()->is_main = is_main;
    thread()->info_log = true;
    thread()->uv_args = NULL;
    thread()->exiting = 0;
    thread()->thrd_id = thread_id;
    thread()->sleep_activated = false;
    thread()->sleep_task = NULL;
    thread()->sleeping_counted = 0;
    thread()->sleep_id  = 0;
    thread()->interrupt_handle = NULL;
    thread()->interrupt_default = NULL;
    thread()->active_handle = NULL;
    thread()->main_handle = NULL;
    thread()->current_handle = NULL;
    if (gq_sys.is_multi)
        atomic_store(&gq_sys.count[thread_id], &thread()->used_count);
}

void sched_unwind_setup(ex_context_t *ctx, const char *ex, const char *message) {
    routine_t *co = co_active();
    co->scope->err = (void_t)ex;
    co->scope->panic = message;
    ex_data_set(ctx, (void_t)co);
    ex_unwind_set(ctx, co->scope->is_protected);
}

#if defined(_MSC_VER) && defined(NO_GETTIMEOFDAY)
int gettimeofday(struct timeval *tp, struct timezone *tzp) {
    /*
     * Note: some broken versions only have 8 trailing zero's, the correct
     * epoch has 9 trailing zero's
     */
    static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    time = ((uint64_t)file_time.dwLowDateTime);
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec = (long)((time - EPOCH) / 10000000L);
    tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
    return 0;
}
#endif

C_API string_t sched_uname(void) {
    if (is_str_empty(gq_sys.powered_by)) {
        uv_utsname_t buffer[1];
        uv_os_uname(buffer);
        string_t powered_by = str_concat_by(8, "Beta - ",
                                      co_itoa(sched_cpu_count()), " cores, ",
                                      buffer->sysname, " ",
                                      buffer->machine, " ",
                                      buffer->release);

        strncpy((char *)gq_sys.powered_by, powered_by, SCRAPE_SIZE);
        CO_FREE((void_t)powered_by);
    }


    return gq_sys.powered_by;
}

/* called only if routine_t func returns */
static void co_done(void) {
    if (!co_active()->interrupt_active) {
        co_active()->halt = true;
        co_active()->status = CO_DEAD;
    }

    co_scheduler();
}

static void co_awaitable(void) {
    routine_t *co = co_active();
    try {
        if (co->interrupt_active) {
            co->func(co->args);
        } else {
            co_result_set(co, co->func(co->args));
            co_deferred_free(co);
        }
    } catch_if {
        co_deferred_free(co);
        if (co->scope->is_recovered)
           ex_flags_reset();
    } finally {
        if (co->is_event_err) {
            co->halt = true;
            co->status = CO_ERRED;
            co->interrupt_active = false;
        } else if (co->interrupt_active) {
            co->status = CO_EVENT;
        } else if (co->event_active) {
            co->is_waiting = false;
        } else {
            co->status = CO_NORMAL;
        }
    } end_try;
}

static void co_func(void) {
    co_awaitable();
    co_done(); /* called only if coroutine function returns */
}

/* Utility for aligning addresses. */
static CO_FORCE_INLINE size_t _co_align_forward(size_t addr, size_t align) {
    return (addr + (align - 1)) & ~(align - 1);
}

uv_loop_t *co_loop(void) {
    if (!is_empty(thread()->interrupt_handle))
        return thread()->interrupt_handle;
    else if (is_empty(thread()->interrupt_default))
        thread()->interrupt_default = uv_default_loop();

    return thread()->interrupt_default;
}

#ifdef CO_MPROTECT
alignas(4096)
#else
section(text)
#endif

#if ((defined(__clang__) || defined(__GNUC__)) && defined(__i386__)) || (defined(_MSC_VER) && defined(_M_IX86))
/* ABI: fastcall */
static const unsigned char co_swap_function[ 4096 ] = {
    0x89, 0x22,       /* mov [edx],esp    */
    0x8b, 0x21,       /* mov esp,[ecx]    */
    0x58,             /* pop eax          */
    0x89, 0x6a, 0x04, /* mov [edx+ 4],ebp */
    0x89, 0x72, 0x08, /* mov [edx+ 8],esi */
    0x89, 0x7a, 0x0c, /* mov [edx+12],edi */
    0x89, 0x5a, 0x10, /* mov [edx+16],ebx */
    0x8b, 0x69, 0x04, /* mov ebp,[ecx+ 4] */
    0x8b, 0x71, 0x08, /* mov esi,[ecx+ 8] */
    0x8b, 0x79, 0x0c, /* mov edi,[ecx+12] */
    0x8b, 0x59, 0x10, /* mov ebx,[ecx+16] */
    0xff, 0xe0,       /* jmp eax          */
};

#ifdef _WIN32
#include <windows.h>
static void co_init(void) {
#ifdef CO_MPROTECT
    DWORD old_privileges;
    VirtualProtect((void_t)co_swap_function, sizeof co_swap_function, PAGE_EXECUTE_READ, &old_privileges);
#endif
}
#else
#ifdef CO_MPROTECT
#include <unistd.h>
#include <sys/mman.h>
#endif

static void co_init(void) {
#ifdef CO_MPROTECT
    unsigned long addr = (unsigned long)co_swap_function;
    unsigned long base = addr - (addr % sysconf(_SC_PAGESIZE));
    unsigned long size = (addr - base) + sizeof co_swap_function;
    mprotect((void_t)base, size, PROT_READ | PROT_EXEC);
#endif
}
#endif

routine_t *co_derive(void_t memory, size_t size) {
    routine_t *handle;
    if (!co_swap) {
        co_init();
        co_swap = (void(fastcall *)(routine_t *, routine_t *))co_swap_function;
    }

    if ((handle = (routine_t *)memory)) {
        unsigned long stack_top = (unsigned long)handle + size;
        stack_top -= 32;
        stack_top &= ~((unsigned long)15);
        long *p = (long *)(stack_top); /* seek to top of stack */
        *--p = (long)co_done;          /* if func returns */
        *--p = (long)co_awaitable;     /* start of function */
        *(long *)handle = (long)p;     /* stack pointer */

#ifdef USE_VALGRIND
        size_t stack_addr = _co_align_forward((size_t)handle + sizeof(routine_t), 16);
        handle->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif
    }

    return handle;
}
#elif ((defined(__clang__) || defined(__GNUC__)) && defined(__amd64__)) || (defined(_MSC_VER) && defined(_M_AMD64))
#ifdef _WIN32
/* ABI: Win64 */
static const unsigned char co_swap_function[ 4096 ] = {
    0x48, 0x89, 0x22,             /* mov [rdx],rsp           */
    0x48, 0x8b, 0x21,             /* mov rsp,[rcx]           */
    0x58,                         /* pop rax                 */
    0x48, 0x83, 0xe9, 0x80,       /* sub rcx,-0x80           */
    0x48, 0x83, 0xea, 0x80,       /* sub rdx,-0x80           */
    0x48, 0x89, 0x6a, 0x88,       /* mov [rdx-0x78],rbp      */
    0x48, 0x89, 0x72, 0x90,       /* mov [rdx-0x70],rsi      */
    0x48, 0x89, 0x7a, 0x98,       /* mov [rdx-0x68],rdi      */
    0x48, 0x89, 0x5a, 0xa0,       /* mov [rdx-0x60],rbx      */
    0x4c, 0x89, 0x62, 0xa8,       /* mov [rdx-0x58],r12      */
    0x4c, 0x89, 0x6a, 0xb0,       /* mov [rdx-0x50],r13      */
    0x4c, 0x89, 0x72, 0xb8,       /* mov [rdx-0x48],r14      */
    0x4c, 0x89, 0x7a, 0xc0,       /* mov [rdx-0x40],r15      */
#if !defined(CO_NO_SSE)
        0x0f, 0x29, 0x72, 0xd0,       /* movaps [rdx-0x30],xmm6  */
        0x0f, 0x29, 0x7a, 0xe0,       /* movaps [rdx-0x20],xmm7  */
        0x44, 0x0f, 0x29, 0x42, 0xf0, /* movaps [rdx-0x10],xmm8  */
        0x44, 0x0f, 0x29, 0x0a,       /* movaps [rdx],     xmm9  */
        0x44, 0x0f, 0x29, 0x52, 0x10, /* movaps [rdx+0x10],xmm10 */
        0x44, 0x0f, 0x29, 0x5a, 0x20, /* movaps [rdx+0x20],xmm11 */
        0x44, 0x0f, 0x29, 0x62, 0x30, /* movaps [rdx+0x30],xmm12 */
        0x44, 0x0f, 0x29, 0x6a, 0x40, /* movaps [rdx+0x40],xmm13 */
        0x44, 0x0f, 0x29, 0x72, 0x50, /* movaps [rdx+0x50],xmm14 */
        0x44, 0x0f, 0x29, 0x7a, 0x60, /* movaps [rdx+0x60],xmm15 */
#endif
        0x48, 0x8b, 0x69, 0x88,       /* mov rbp,[rcx-0x78]      */
        0x48, 0x8b, 0x71, 0x90,       /* mov rsi,[rcx-0x70]      */
        0x48, 0x8b, 0x79, 0x98,       /* mov rdi,[rcx-0x68]      */
        0x48, 0x8b, 0x59, 0xa0,       /* mov rbx,[rcx-0x60]      */
        0x4c, 0x8b, 0x61, 0xa8,       /* mov r12,[rcx-0x58]      */
        0x4c, 0x8b, 0x69, 0xb0,       /* mov r13,[rcx-0x50]      */
        0x4c, 0x8b, 0x71, 0xb8,       /* mov r14,[rcx-0x48]      */
        0x4c, 0x8b, 0x79, 0xc0,       /* mov r15,[rcx-0x40]      */
#if !defined(CO_NO_SSE)
        0x0f, 0x28, 0x71, 0xd0,       /* movaps xmm6, [rcx-0x30] */
        0x0f, 0x28, 0x79, 0xe0,       /* movaps xmm7, [rcx-0x20] */
        0x44, 0x0f, 0x28, 0x41, 0xf0, /* movaps xmm8, [rcx-0x10] */
        0x44, 0x0f, 0x28, 0x09,       /* movaps xmm9, [rcx]      */
        0x44, 0x0f, 0x28, 0x51, 0x10, /* movaps xmm10,[rcx+0x10] */
        0x44, 0x0f, 0x28, 0x59, 0x20, /* movaps xmm11,[rcx+0x20] */
        0x44, 0x0f, 0x28, 0x61, 0x30, /* movaps xmm12,[rcx+0x30] */
        0x44, 0x0f, 0x28, 0x69, 0x40, /* movaps xmm13,[rcx+0x40] */
        0x44, 0x0f, 0x28, 0x71, 0x50, /* movaps xmm14,[rcx+0x50] */
        0x44, 0x0f, 0x28, 0x79, 0x60, /* movaps xmm15,[rcx+0x60] */
#endif
#if !defined(CO_NO_TIB)
        0x65, 0x4c, 0x8b, 0x04, 0x25, /* mov r8,gs:0x30          */
        0x30, 0x00, 0x00, 0x00,
        0x41, 0x0f, 0x10, 0x40, 0x08, /* movups xmm0,[r8+0x8]    */
        0x0f, 0x29, 0x42, 0x70,       /* movaps [rdx+0x70],xmm0  */
        0x0f, 0x28, 0x41, 0x70,       /* movaps xmm0,[rcx+0x70]  */
        0x41, 0x0f, 0x11, 0x40, 0x08, /* movups [r8+0x8],xmm0    */
#endif
        0xff, 0xe0,                   /* jmp rax                 */
};

#include <windows.h>

static void co_init(void) {
#ifdef CO_MPROTECT
    DWORD old_privileges;
    VirtualProtect((void_t)co_swap_function, sizeof co_swap_function, PAGE_EXECUTE_READ, &old_privileges);
#endif
}
#else
/* ABI: SystemV */
static const unsigned char co_swap_function[ 4096 ] = {
    0x48, 0x89, 0x26,       /* mov [rsi],rsp    */
    0x48, 0x8b, 0x27,       /* mov rsp,[rdi]    */
    0x58,                   /* pop rax          */
    0x48, 0x89, 0x6e, 0x08, /* mov [rsi+ 8],rbp */
    0x48, 0x89, 0x5e, 0x10, /* mov [rsi+16],rbx */
    0x4c, 0x89, 0x66, 0x18, /* mov [rsi+24],r12 */
    0x4c, 0x89, 0x6e, 0x20, /* mov [rsi+32],r13 */
    0x4c, 0x89, 0x76, 0x28, /* mov [rsi+40],r14 */
    0x4c, 0x89, 0x7e, 0x30, /* mov [rsi+48],r15 */
    0x48, 0x8b, 0x6f, 0x08, /* mov rbp,[rdi+ 8] */
    0x48, 0x8b, 0x5f, 0x10, /* mov rbx,[rdi+16] */
    0x4c, 0x8b, 0x67, 0x18, /* mov r12,[rdi+24] */
    0x4c, 0x8b, 0x6f, 0x20, /* mov r13,[rdi+32] */
    0x4c, 0x8b, 0x77, 0x28, /* mov r14,[rdi+40] */
    0x4c, 0x8b, 0x7f, 0x30, /* mov r15,[rdi+48] */
    0xff, 0xe0,             /* jmp rax          */
};

#ifdef CO_MPROTECT
#include <unistd.h>
#include <sys/mman.h>
#endif

static void co_init(void) {
#ifdef CO_MPROTECT
    unsigned long long addr = (unsigned long long)co_swap_function;
    unsigned long long base = addr - (addr % sysconf(_SC_PAGESIZE));
    unsigned long long size = (addr - base) + sizeof co_swap_function;
    mprotect((void_t)base, size, PROT_READ | PROT_EXEC);
#endif
}
#endif
routine_t *co_derive(void_t memory, size_t size) {
    routine_t *handle;
    if (!co_swap) {
        co_init();
        co_swap = (void (*)(routine_t *, routine_t *))co_swap_function;
    }

    if ((handle = (routine_t *)memory)) {
        size_t stack_top = (size_t)handle + size;
        stack_top -= 32;
        stack_top &= ~((size_t)15);
        int64_t *p = (int64_t *)(stack_top); /* seek to top of stack */
        *--p = (int64_t)co_done;               /* if coroutine returns */
        *--p = (int64_t)co_awaitable;
        *(int64_t *)handle = (int64_t)p;                  /* stack pointer */
#if defined(_WIN32) && !defined(CO_NO_TIB)
        ((int64_t *)handle)[ 30 ] = (int64_t)handle + size; /* stack base */
        ((int64_t *)handle)[ 31 ] = (int64_t)handle;        /* stack limit */
#endif

#ifdef USE_VALGRIND
        size_t stack_addr = _co_align_forward((size_t)handle + sizeof(routine_t), 16);
        handle->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif
    }

    return handle;
}
#elif defined(__clang__) || defined(__GNUC__)
#if defined(__arm__)
#ifdef CO_MPROTECT
#include <unistd.h>
#include <sys/mman.h>
#endif

static const size_t co_swap_function[ 1024 ] = {
    0xe8a16ff0, /* stmia r1!, {r4-r11,sp,lr} */
    0xe8b0aff0, /* ldmia r0!, {r4-r11,sp,pc} */
    0xe12fff1e, /* bx lr                     */
};

static void co_init(void) {
#ifdef CO_MPROTECT
    size_t addr = (size_t)co_swap_function;
    size_t base = addr - (addr % sysconf(_SC_PAGESIZE));
    size_t size = (addr - base) + sizeof co_swap_function;
    mprotect((void_t)base, size, PROT_READ | PROT_EXEC);
#endif
}

routine_t *co_derive(void_t memory, size_t size) {
    size_t *handle;
    routine_t *co;
    if (!co_swap) {
        co_init();
        co_swap = (void (*)(routine_t *, routine_t *))co_swap_function;
    }

    if ((handle = (size_t *)memory)) {
        size_t stack_top = (size_t)handle + size;
        stack_top &= ~((size_t)15);
        size_t *p = (size_t *)(stack_top);
        handle[ 8 ] = (size_t)p;
        handle[ 9 ] = (size_t)co_func;

        co = (routine_t *)handle;
#ifdef USE_VALGRIND
        size_t stack_addr = _co_align_forward((size_t)co + sizeof(routine_t), 16);
        co->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif
    }

    return co;
}
#elif defined(__aarch64__)
static const uint32_t co_swap_function[ 1024 ] = {
    0x910003f0, /* mov x16,sp           */
    0xa9007830, /* stp x16,x30,[x1]     */
    0xa9407810, /* ldp x16,x30,[x0]     */
    0x9100021f, /* mov sp,x16           */
    0xa9015033, /* stp x19,x20,[x1, 16] */
    0xa9415013, /* ldp x19,x20,[x0, 16] */
    0xa9025835, /* stp x21,x22,[x1, 32] */
    0xa9425815, /* ldp x21,x22,[x0, 32] */
    0xa9036037, /* stp x23,x24,[x1, 48] */
    0xa9436017, /* ldp x23,x24,[x0, 48] */
    0xa9046839, /* stp x25,x26,[x1, 64] */
    0xa9446819, /* ldp x25,x26,[x0, 64] */
    0xa905703b, /* stp x27,x28,[x1, 80] */
    0xa945701b, /* ldp x27,x28,[x0, 80] */
    0xf900303d, /* str x29,    [x1, 96] */
    0xf940301d, /* ldr x29,    [x0, 96] */
    0x6d072428, /* stp d8, d9, [x1,112] */
    0x6d472408, /* ldp d8, d9, [x0,112] */
    0x6d082c2a, /* stp d10,d11,[x1,128] */
    0x6d482c0a, /* ldp d10,d11,[x0,128] */
    0x6d09342c, /* stp d12,d13,[x1,144] */
    0x6d49340c, /* ldp d12,d13,[x0,144] */
    0x6d0a3c2e, /* stp d14,d15,[x1,160] */
    0x6d4a3c0e, /* ldp d14,d15,[x0,160] */
#if defined(_WIN32) && !defined(CO_NO_TIB)
    0xa940c650, /* ldp x16,x17,[x18, 8] */
    0xa90b4430, /* stp x16,x17,[x1,176] */
    0xa94b4410, /* ldp x16,x17,[x0,176] */
    0xa900c650, /* stp x16,x17,[x18, 8] */
#endif
    0xd61f03c0, /* br x30               */
};

#ifdef _WIN32
#include <windows.h>

static void co_init(void) {
#ifdef CO_MPROTECT
    DWORD old_privileges;
    VirtualProtect((void_t)co_swap_function, sizeof co_swap_function, PAGE_EXECUTE_READ, &old_privileges);
#endif
}
#else
#ifdef CO_MPROTECT
#include <unistd.h>
#include <sys/mman.h>
#endif

static void co_init(void) {
#ifdef CO_MPROTECT
    size_t addr = (size_t)co_swap_function;
    size_t base = addr - (addr % sysconf(_SC_PAGESIZE));
    size_t size = (addr - base) + sizeof co_swap_function;
    mprotect((void_t)base, size, PROT_READ | PROT_EXEC);
#endif
}
#endif

routine_t *co_derive(void_t memory, size_t size) {
    size_t *handle;
    routine_t *co;
    if (!co_swap) {
        co_init();
        co_swap = (void (*)(routine_t *, routine_t *))co_swap_function;
    }

    if ((handle = (size_t *)memory)) {
        size_t stack_top = (size_t)handle + size;
        stack_top &= ~((size_t)15);
        size_t *p = (size_t *)(stack_top);
        handle[ 0 ] = (size_t)p;              /* x16 (stack pointer) */
        handle[ 1 ] = (size_t)co_func;        /* x30 (link register) */
        handle[ 12 ] = (size_t)p;             /* x29 (frame pointer) */

#if defined(_WIN32) && !defined(CO_NO_TIB)
        handle[ 22 ] = (size_t)handle + size; /* stack base */
        handle[ 23 ] = (size_t)handle;        /* stack limit */
#endif

        co = (routine_t *)handle;
#ifdef USE_VALGRIND
        size_t stack_addr = _co_align_forward((size_t)co + sizeof(routine_t), 16);
        co->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif
    }

    return co;
}
#elif defined(__powerpc64__) && defined(_CALL_ELF) && _CALL_ELF == 2
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define ALIGN(p, x) ((void_t)((uintptr_t)(p) & ~((x)-1)))

#define MIN_STACK 0x10000lu
#define MIN_STACK_FRAME 0x20lu
#define STACK_ALIGN 0x10lu

void swap_context(routine_t *read, routine_t *write);
__asm__(
    ".text\n"
    ".align 4\n"
    ".type swap_context @function\n"
    "swap_context:\n"
    ".cfi_startproc\n"

    /* save GPRs */
    "std 1, 8(4)\n"
    "std 2, 16(4)\n"
    "std 12, 96(4)\n"
    "std 13, 104(4)\n"
    "std 14, 112(4)\n"
    "std 15, 120(4)\n"
    "std 16, 128(4)\n"
    "std 17, 136(4)\n"
    "std 18, 144(4)\n"
    "std 19, 152(4)\n"
    "std 20, 160(4)\n"
    "std 21, 168(4)\n"
    "std 22, 176(4)\n"
    "std 23, 184(4)\n"
    "std 24, 192(4)\n"
    "std 25, 200(4)\n"
    "std 26, 208(4)\n"
    "std 27, 216(4)\n"
    "std 28, 224(4)\n"
    "std 29, 232(4)\n"
    "std 30, 240(4)\n"
    "std 31, 248(4)\n"

    /* save LR */
    "mflr 5\n"
    "std 5, 256(4)\n"

    /* save CCR */
    "mfcr 5\n"
    "std 5, 264(4)\n"

    /* save FPRs */
    "stfd 14, 384(4)\n"
    "stfd 15, 392(4)\n"
    "stfd 16, 400(4)\n"
    "stfd 17, 408(4)\n"
    "stfd 18, 416(4)\n"
    "stfd 19, 424(4)\n"
    "stfd 20, 432(4)\n"
    "stfd 21, 440(4)\n"
    "stfd 22, 448(4)\n"
    "stfd 23, 456(4)\n"
    "stfd 24, 464(4)\n"
    "stfd 25, 472(4)\n"
    "stfd 26, 480(4)\n"
    "stfd 27, 488(4)\n"
    "stfd 28, 496(4)\n"
    "stfd 29, 504(4)\n"
    "stfd 30, 512(4)\n"
    "stfd 31, 520(4)\n"

#ifdef __ALTIVEC__
    /* save VMX */
    "li 5, 528\n"
    "stvxl 20, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 21, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 22, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 23, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 24, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 25, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 26, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 27, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 28, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 29, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 30, 4, 5\n"
    "addi 5, 5, 16\n"
    "stvxl 31, 4, 5\n"
    "addi 5, 5, 16\n"

    /* save VRSAVE */
    "mfvrsave 5\n"
    "stw 5, 736(4)\n"
#endif

    /* restore GPRs */
    "ld 1, 8(3)\n"
    "ld 2, 16(3)\n"
    "ld 12, 96(3)\n"
    "ld 13, 104(3)\n"
    "ld 14, 112(3)\n"
    "ld 15, 120(3)\n"
    "ld 16, 128(3)\n"
    "ld 17, 136(3)\n"
    "ld 18, 144(3)\n"
    "ld 19, 152(3)\n"
    "ld 20, 160(3)\n"
    "ld 21, 168(3)\n"
    "ld 22, 176(3)\n"
    "ld 23, 184(3)\n"
    "ld 24, 192(3)\n"
    "ld 25, 200(3)\n"
    "ld 26, 208(3)\n"
    "ld 27, 216(3)\n"
    "ld 28, 224(3)\n"
    "ld 29, 232(3)\n"
    "ld 30, 240(3)\n"
    "ld 31, 248(3)\n"

    /* restore LR */
    "ld 5, 256(3)\n"
    "mtlr 5\n"

    /* restore CCR */
    "ld 5, 264(3)\n"
    "mtcr 5\n"

    /* restore FPRs */
    "lfd 14, 384(3)\n"
    "lfd 15, 392(3)\n"
    "lfd 16, 400(3)\n"
    "lfd 17, 408(3)\n"
    "lfd 18, 416(3)\n"
    "lfd 19, 424(3)\n"
    "lfd 20, 432(3)\n"
    "lfd 21, 440(3)\n"
    "lfd 22, 448(3)\n"
    "lfd 23, 456(3)\n"
    "lfd 24, 464(3)\n"
    "lfd 25, 472(3)\n"
    "lfd 26, 480(3)\n"
    "lfd 27, 488(3)\n"
    "lfd 28, 496(3)\n"
    "lfd 29, 504(3)\n"
    "lfd 30, 512(3)\n"
    "lfd 31, 520(3)\n"

#ifdef __ALTIVEC__
    /* restore VMX */
    "li 5, 528\n"
    "lvxl 20, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 21, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 22, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 23, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 24, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 25, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 26, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 27, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 28, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 29, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 30, 3, 5\n"
    "addi 5, 5, 16\n"
    "lvxl 31, 3, 5\n"
    "addi 5, 5, 16\n"

    /* restore VRSAVE */
    "lwz 5, 720(3)\n"
    "mtvrsave 5\n"
#endif

    /* branch to LR */
    "blr\n"

    ".cfi_endproc\n"
    ".size swap_context, .-swap_context\n");

routine_t *co_derive(void_t memory, size_t size) {
    uint8_t *sp;
    routine_t *context = (routine_t *)memory;
    if (!co_swap) {
        co_swap = (void (*)(routine_t *, routine_t *))swap_context;
    }

    /* save current context into new context to initialize it */
    swap_context(context, context);

    /* align stack */
    sp = (uint8_t *)memory + size - STACK_ALIGN;
    sp = (uint8_t *)ALIGN(sp, STACK_ALIGN);

    /* write 0 for initial backchain */
    *(uint64_t *)sp = 0;

    /* create new frame with backchain */
    sp -= MIN_STACK_FRAME;
    *(uint64_t *)sp = (uint64_t)(sp + MIN_STACK_FRAME);

    /* update context with new stack (r1) and func (r12, lr) */
    context->gprs[ 1 ] = (uint64_t)sp;
    context->gprs[ 12 ] = (uint64_t)co_func;
    context->lr = (uint64_t)co_func;

#ifdef USE_VALGRIND
    size_t stack_addr = _co_align_forward((size_t)context + sizeof(routine_t), 16);
    context->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif
    return context;
}
#else
#define USE_OTHER 1
#endif
#else
#define USE_OTHER 1
#endif

#if defined(USE_OTHER) && defined(_WIN32)
int getcontext(ucontext_t *ucp) {
    int ret;

    /* Retrieve the full machine context */
    ucp->uc_mcontext.ContextFlags = CONTEXT_FULL;
    ret = GetThreadContext(GetCurrentThread(), &ucp->uc_mcontext);

    return (ret == 0) ? -1 : 0;
}

int setcontext(const ucontext_t *ucp) {
    int ret;

    /* Restore the full machine context (already set) */
    ret = SetThreadContext(GetCurrentThread(), &ucp->uc_mcontext);
    return (ret == 0) ? -1 : 0;
}

int makecontext(ucontext_t *ucp, void (*func)(), int argc, ...) {
    int i;
    va_list ap;
    char *sp;

    /* Stack grows down */
    sp = (char *)(size_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size;

    if (sp < (char *)ucp->uc_stack.ss_sp) {
        /* errno = ENOMEM;*/
        return -1;
    }

    /* Set the instruction and the stack pointer */
#if defined(_X86_)
    ucp->uc_mcontext.Eip = (unsigned long long)co_func;
    ucp->uc_mcontext.Esp = (unsigned long long)(sp - 4);
#else
    ucp->uc_mcontext.Rip = (unsigned long long)co_func;
    ucp->uc_mcontext.Rsp = (unsigned long long)(sp - 40);
#endif
    /* Save/Restore the full machine context */
    ucp->uc_mcontext.ContextFlags = CONTEXT_FULL;

    return 0;
}

int swapcontext(routine_t *oucp, const routine_t *ucp) {
    int ret;

    if (is_empty(oucp) || is_empty(ucp)) {
        /*errno = EINVAL;*/
        return -1;
    }

    ret = getcontext((ucontext_t *)oucp);
    if (ret == 0) {
        ret = setcontext((ucontext_t *)ucp);
    }
    return ret;
}
#endif

#if defined(USE_OTHER)
routine_t *co_derive(void_t memory, size_t size) {
    if (!thread()->active_handle)
        thread()->active_handle = thread()->active_buffer;

    ucontext_t *thread = (ucontext_t *)memory;
    memory = (unsigned char *)memory + sizeof(routine_t);
    size -= sizeof(routine_t);
    if ((!getcontext(thread) && !(thread->uc_stack.ss_sp = 0)) && (thread->uc_stack.ss_sp = memory)) {
        thread->uc_link = (ucontext_t *)thread()->active_handle;
        thread->uc_stack.ss_size = size;
        makecontext(thread, co_func, 0);
    } else {
        co_panic("getcontext failed!");
    }

    return (routine_t *)thread;
}
#endif

CO_FORCE_INLINE routine_t *co_active(void) {
    if (!thread()->active_handle) {
        thread()->active_handle = thread()->active_buffer;
    }

    return thread()->active_handle;
}

CO_FORCE_INLINE memory_t *co_scope(void) {
    return co_active()->scope;
}

CO_FORCE_INLINE routine_t *co_current(void) {
    return thread()->current_handle;
}

CO_FORCE_INLINE routine_t *co_coroutine(void) {
    return thread()->running;
}

CO_FORCE_INLINE void co_scheduler(void) {
    co_switch(thread()->main_handle);
}

void co_stack_check(int n) {
    routine_t *t = thread()->running;
    if ((char *)&t <= (char *)t->stack_base || (char *)&t - (char *)t->stack_base < 256 + n || t->magic_number != CO_MAGIC_NUMBER) {
        snprintf(error_message, ERROR_SCRAPE_SIZE, "coroutine stack overflow: &t=%p stack=%p n=%d\n", &t, t->stack_base, 256 + n);
        co_panic(error_message);
    }
}

routine_t *co_create(size_t size, callable_t func, void_t args) {
    /* Stack size should be at least `CO_STACK_SIZE`. */
    if ((size != 0 && size < CO_STACK_SIZE) || size == 0)
        size = CO_STACK_SIZE;

    size = _co_align_forward(size + sizeof(routine_t), 16); /* Stack size should be aligned to 16 bytes. */
    void_t memory = try_calloc(1, size + sizeof(channel_t) + sizeof(values_t));
    routine_t *co = co_derive(memory, size);
    if (!thread()->current_handle)
        thread()->current_handle = co_active();

    if (!thread()->main_handle)
        thread()->main_handle = thread()->active_handle;

#ifdef UV_H
    if (!thread()->interrupt_handle) {
        thread()->interrupt_handle = thread()->interrupt_buffer;
        if (uv_loop_init(thread()->interrupt_handle)) {
            fprintf(stderr, "Event loop creation failed in file %s at line # %d", __FILE__, __LINE__);
            thread()->interrupt_handle = NULL;
        }
    }
#endif

    if (UNLIKELY(raii_deferred_init(&co->scope->defer) < 0)) {
        CO_FREE(co);
        return (routine_t *)-1;
    }

    co->func = func;
    co->status = CO_SUSPENDED;
    co->stack_size = size + sizeof(channel_t) + sizeof(values_t);
    co->is_channeling = false;
    co->halt = false;
    co->ready = false;
    co->flagged = false;
    co->run_code = RUN_NORMAL;
    co->taken = false;
    co->wait_active = false;
    co->wait_group = NULL;
    co->event_group = NULL;
    co->interrupt_active = false;
    co->event_active = false;
    co->process_active = false;
    co->is_event_err = false;
    co->is_plain = false;
    co->is_address = false;
    co->is_waiting = false;
    co->is_multi_wait = false;
    co->is_group_finish = true;
    co->event_err_code = 0;
    co->args = args;
    co->user_data = NULL;
    co->cycles = 0;
    co->results = NULL;
    co->plain_results = -1;
    co->scope->is_protected = false;
    co->stack_base = (unsigned char *)(co + 1);
    co->magic_number = CO_MAGIC_NUMBER;

    return co;
}

void co_switch(routine_t *handle) {
#if defined(_M_X64) || defined(_M_IX86)
    routine_t *co_previous_handle = thread()->active_handle;
#else
    routine_t *co_previous_handle = thread()->active_handle;
#endif
    thread()->active_handle = handle;
    thread()->active_handle->status = CO_RUNNING;
    thread()->current_handle = co_previous_handle;
    thread()->current_handle->status = CO_NORMAL;
#if !defined(USE_UCONTEXT)
    co_swap(thread()->active_handle, co_previous_handle);
#else
    swapcontext((ucontext_t *)co_previous_handle, (ucontext_t *)thread()->active_handle);
#endif
}

/* Add coroutine to scheduler queue, appending. */
static void sched_add(scheduler_t *l, routine_t *t) {
    if (l->tail) {
        l->tail->next = t;
        t->prev = l->tail;
    } else {
        l->head = t;
        t->prev = NULL;
    }

    l->tail = t;
    t->next = NULL;
}

/* Remove coroutine from scheduler queue. */
static void sched_remove(scheduler_t *l, routine_t *t) {
    if (t->prev)
        t->prev->next = t->next;
    else
        l->head = t->next;

    if (t->next)
        t->next->prev = t->prev;
    else
        l->tail = t->prev;
}

static void sched_enqueue_atomic(routine_t *t) {
    scheduler_t *list = (scheduler_t *)atomic_load_explicit(&gq_sys.run_queue, memory_order_acquire);
    sched_add(list, t);
    atomic_store_explicit(&gq_sys.run_queue, list, memory_order_release);
}

CO_FORCE_INLINE void sched_enqueue(routine_t *t) {
    t->ready = true;
    atomic_thread_fence(memory_order_seq_cst);
    if (gq_sys.is_multi && is_false(t->taken))
        sched_enqueue_atomic(t);
    else
        sched_add(&thread()->run_queue, t);
}

CO_FORCE_INLINE routine_t *sched_dequeue(scheduler_t *l) {
    routine_t *t = l->head;
    sched_remove(l, t);
    return t;
}

/* Transfer `count` tasks from `global` run queue to `local` run queue. */
static void sched_steal(atomic_deque_t *q, thread_processor_t *r, int count) {
    int i;
    routine_t *t;
    scheduler_t *l = ((scheduler_t *)atomic_load_explicit(&q->run_queue, memory_order_acquire));
    atomic_thread_fence(memory_order_seq_cst);
    for (i = 0; i < count; i++) {
        atomic_fetch_sub(&gq_sys.used_count, 1);
        t = sched_dequeue(l);
        r->used_count++;
        sched_add(&r->run_queue, t);
    }

    atomic_store_explicit(&q->run_queue, l, memory_order_release);
}

/* Transfer tasks from `global` run queue to current thread's `local` run queue,
the amount/count was set by `sched_is_takeable` and `sched_post_available`. */
static void sched_steal_available(atomic_deque_t *q) {
    routine_t *t;
    size_t i, available = atomic_load_explicit(&q->available[thread()->thrd_id], memory_order_acquire);
    scheduler_t *l = ((scheduler_t *)atomic_load_explicit(&q->run_queue, memory_order_acquire));
    atomic_thread_fence(memory_order_seq_cst);
    for (i = 0; i < available; i++) {
        t = sched_dequeue(l);
        t->taken = true;
        thread()->used_count++;
        sched_add(&thread()->run_queue, t);
    }

    atomic_store_explicit(&q->run_queue, l, memory_order_release);
    atomic_store_explicit(&q->available[thread()->thrd_id], 0, memory_order_release);
}

static void sched_adjust(atomic_deque_t *q, routine_t *t) {
    size_t generate_id = atomic_load_explicit(&q->id_generate, memory_order_acquire);
    size_t coroutines_num_all = atomic_load_explicit(&q->n_all_coroutine, memory_order_acquire);
    routine_t **coroutines_all = (routine_t **)atomic_load_explicit(&q->all_coroutine, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    if (coroutines_num_all % 64 == 0) {
        coroutines_all = CO_REALLOC(coroutines_all, (coroutines_num_all + 64) * sizeof(coroutines_all[0]));
        if (is_empty(coroutines_all))
            co_panic("realloc() failed");
    }

    t->cid = ++generate_id;
    t->all_coroutine_slot = coroutines_num_all;
    coroutines_all[coroutines_num_all++] = t;
    coroutines_all[coroutines_num_all] = NULL;
    atomic_store_explicit(&q->id_generate, generate_id, memory_order_release);
    atomic_store_explicit(&q->n_all_coroutine, coroutines_num_all, memory_order_release);
    if (gq_sys.is_multi && t->run_code == RUN_NORMAL) {
        atomic_fetch_add(&gq_sys.used_count, 1);
    } else {
        t->taken = true;
        thread()->used_count++;
    }

    sched_enqueue(t);
    atomic_store_explicit(&q->all_coroutine, coroutines_all, memory_order_release);
}

u32 create_routine(callable_t fn, void_t arg, u32 stack, run_states code) {
    u32 id;
    routine_t *t = co_create(stack, fn, arg);
    routine_t *c = co_active();

    if (c->interrupt_active || c->event_active)
        t->run_code = RUN_EVENT;
    else
        t->run_code = code;
    sched_adjust(&gq_sys, t);
    id = t->cid;
    if (c->event_active && !is_empty(c->event_group)) {
        t->event_active = true;
        t->is_waiting = true;
        hash_put(c->event_group, co_itoa(id), t);
        c->event_active = false;
    } else if (c->wait_active && !is_empty(c->wait_group) && !c->is_group_finish) {
        t->is_waiting = true;
        t->is_address = true;
        t->is_multi_wait = gq_sys.is_multi;
        hash_put(c->wait_group, co_itoa(id), t);
        c->wait_counter++;
    }

    if (c->interrupt_active) {
        c->interrupt_active = false;
        t->interrupt_active = true;
        t->context = c;
    }

    return id;
}

CO_FORCE_INLINE void go_stack_set(u32 size) {
    atomic_thread_fence(memory_order_seq_cst);
    gq_sys.stacksize = size;
}

CO_FORCE_INLINE u32 go(callable_t fn, void_t arg) {
    return create_routine(fn, arg, gq_sys.stacksize, RUN_NORMAL);
}

awaitable_t *async(callable_t fn, void_t arg) {
    awaitable_t *awaitable = try_calloc(1, sizeof(awaitable_t));
    routine_t *c = co_active();
    wait_group_t *wg = ht_wait_init(2);
    c->wait_active = true;
    c->wait_group = wg;
    c->is_group_finish = false;
    u32 cid = create_routine(fn, arg, gq_sys.stacksize, RUN_ASYNC);
    c->wait_group = NULL;
    awaitable->wg = wg;
    awaitable->cid = cid;
    awaitable->type = CO_ROUTINE;

    return awaitable;
}

void sched_checker_stealer(void) {
    if (thread()->is_main && gq_sys.is_multi) {
        if (sched_is_takeable())
            sched_post_available();

        if (!atomic_flag_load(&gq_sys.is_started))
            atomic_flag_test_and_set(&gq_sys.is_started);
    }

    if (sched_is_available())
        sched_steal_available(&gq_sys);
}

CO_FORCE_INLINE void co_yield(void) {
    sched_checker_stealer();
    sched_enqueue(thread()->running);
    co_suspend();
}

static size_t nsec(void) {
    struct timeval tv;

    if (gettimeofday(&tv, 0) < 0)
        return -1;

    return (size_t)tv.tv_sec * 1000 * 1000 * 1000 + tv.tv_usec * 1000;
}

CO_FORCE_INLINE int coroutine_loop(int mode) {
    return CO_EVENT_LOOP(co_loop(), mode);
}

CO_FORCE_INLINE void coroutine_interrupt(void) {
    coroutine_loop(UV_RUN_NOWAIT);
}

static void_t coroutine_wait(void_t v) {
    int *has;
    routine_t *t;
    size_t now;

    coroutine_system();
    if (gq_sys.is_multi && is_empty(thread()->sleep_task))
        thread()->sleep_task = co_active();

    coroutine_name("coroutine_wait");
    for (;;) {
        /* let everyone else run */
        while (sched_yielding() > 0)
            ;
        now = nsec();
        co_info(co_active(), 1);
        while ((t = thread()->sleeping.head) && now >= t->alarm_time) {
            sched_remove(&thread()->sleeping, t);
            if (!t->system && --thread()->sleeping_counted == 0)
                thread()->used_count--;

            sched_enqueue(t);
        }

        if (gq_sys.is_multi && !thread()->is_main && gq_sys.is_finish) {
            has = (int *)atomic_load(&gq_sys.count[thread()->thrd_id]);
            if (!is_empty(has)) {
                atomic_store(&gq_sys.count[thread()->thrd_id], NULL);
                thread()->sleep_task->halt = true;
                co_deferred_free(thread()->sleep_task);
            }
        }
    }
}

u32 sleep_for(u32 ms) {
    size_t when, now;
    routine_t *t;

    if (!thread()->sleep_activated) {
        sched_checker_stealer();
        thread()->sleep_activated = true;
        thread()->sleep_id = create_routine(coroutine_wait, NULL, gq_sys.stacksize, RUN_SYSTEM);
    }

    now = nsec();
    when = now + (size_t)ms * 1000000;
    for (t = thread()->sleeping.head; !is_empty(t) && t->alarm_time < when; t = t->next)
        ;

    if (t) {
        thread()->running->prev = t->prev;
        thread()->running->next = t;
    } else {
        thread()->running->prev = thread()->sleeping.tail;
        thread()->running->next = NULL;
    }

    t = thread()->running;
    t->alarm_time = when;
    if (t->prev)
        t->prev->next = t;
    else
        thread()->sleeping.head = t;

    if (t->next)
        t->next->prev = t;
    else
        thread()->sleeping.tail = t;

    if (!t->system && thread()->sleeping_counted++ == 0)
        thread()->used_count++;

    co_switch(co_current());

    return (u32)(nsec() - now) / 1000000;
}

int sched_yielding(void) {
    int n = thread()->num_others_ran;
    coroutine_state("yielded");
    co_yield();
    coroutine_state("running");
    return thread()->num_others_ran - n - 1;
}

void coroutine_state(char *fmt, ...) {
    va_list args;
    routine_t *t;

    t = thread()->running;
    va_start(args, fmt);
    vsnprintf(t->state, sizeof t->name, fmt, args);
    va_end(args);
}

void coroutine_name(char *fmt, ...) {
    va_list args;
    routine_t *t;

    t = thread()->running;
    va_start(args, fmt);
    vsnprintf(t->name, sizeof t->name, fmt, args);
    va_end(args);
}

CO_FORCE_INLINE char *coroutine_get_state(void) {
    return co_active()->state;
}

CO_FORCE_INLINE void sched_dec(void) {
    --thread()->used_count;
}

CO_FORCE_INLINE uv_args_t *sched_server_args(void) {
    return thread()->uv_args;
}

CO_FORCE_INLINE void sched_server_set(uv_args_t uv_args) {
    thread()->uv_args = &uv_args;
}

void coroutine_system(void) {
    if (!thread()->running->system) {
        thread()->running->system = true;
        --thread()->used_count;
    }
}

void sched_exit(int val) {
    thread()->exiting = val;
    thread()->running->exiting = true;
    co_deferred_free(thread()->running);
    co_scheduler();
}

void sched_destroy(void) {
    if (gq_sys.is_multi && thread()->is_main && gq_sys.threads) {
        size_t i;
        atomic_thread_fence(memory_order_seq_cst);
        gq_sys.is_finish = true;
        while (sched_is_running())
            thrd_yield();

        for (i = 0; i < gq_sys.thread_count; i++) {
            atomic_store(&gq_sys.count[i], NULL);
            atomic_store(&gq_sys.available[i], 0);
            atomic_store(&gq_sys.stealable_amount[i], 0);
            atomic_flag_clear(&gq_sys.any_stealable[i]);
        }

        atomic_flag_clear(&gq_sys.is_started);
        for (i = 0; i < gq_sys.cpu_count; i++) {
            if (thread()->sleeping_counted > 0)
                pthread_cancel(gq_sys.threads[i]);

            if (thrd_join(gq_sys.threads[i], NULL) != thrd_success)
                fprintf(stderr, "`thrd_join` failed!\n");
        }

        CO_FREE(gq_sys.threads);
        atomic_thread_fence(memory_order_seq_cst);
        gq_sys.threads = NULL;
    }
}

void sched_update(routine_t *t) {
    size_t coroutines_num_all = atomic_load_explicit(&gq_sys.n_all_coroutine, memory_order_acquire);
    routine_t **coroutines_all = (routine_t **)atomic_load_explicit(&gq_sys.all_coroutine, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    int i = t->all_coroutine_slot;
    coroutines_all[i] = coroutines_all[--coroutines_num_all];
    coroutines_all[i]->all_coroutine_slot = i;
    atomic_store_explicit(&gq_sys.n_all_coroutine, coroutines_num_all, memory_order_release);
    atomic_store_explicit(&gq_sys.all_coroutine, coroutines_all, memory_order_release);
}

void sched_cleanup(void) {
    routine_t *t;
    u32 i;
    if (!can_cleanup)
        return;

    atomic_thread_fence(memory_order_seq_cst);
    can_cleanup = false;
    sched_destroy();
    chan_collector_free();
    co_collector_free();
    size_t coroutines_num_all = atomic_load(&gq_sys.n_all_coroutine);
    routine_t **coroutines_all = (routine_t **)atomic_load(&gq_sys.all_coroutine);

    if (coroutines_num_all) {
        if (thread()->used_count)
            coroutines_num_all--;

        for (i = 0; i < (coroutines_num_all + (thread()->used_count != 0)); i++) {
            t = coroutines_all[coroutines_num_all - i];
            if (t && t->magic_number == CO_MAGIC_NUMBER) {
                t->magic_number = -1;
                CO_FREE(t);
            }
        }
    }

    if (!is_empty(coroutines_all)) {
        CO_FREE(coroutines_all);
        coroutines_all = NULL;
    }
#ifdef UV_H
    if (!is_empty(thread()->interrupt_handle)) {
        uv_loop_close(thread()->interrupt_handle);
        thread()->interrupt_handle = NULL;
    }
#endif
    CO_FREE((void_t)atomic_load(&gq_sys.run_queue));
    if (gq_sys.is_multi) {
        CO_FREE((void_t)atomic_load(&gq_sys.count));
        CO_FREE((void_t)atomic_load(&gq_sys.available));
        CO_FREE((void_t)atomic_load(&gq_sys.stealable_thread));
        CO_FREE((void_t)atomic_load(&gq_sys.stealable_amount));
        CO_FREE((void_t)atomic_load(&gq_sys.any_stealable));
    }
}

static int thrd_scheduler(void) {
    routine_t *t = NULL;
    scheduler_t *l = NULL;
    int stole = 0;

    for (;;) {
        if (sched_is_stolen())
            sched_post_stolen(thread());

        if (thread()->used_count == 0 || !sched_active() || l == EMPTY) {
            if (thread()->is_main && (l == EMPTY || gq_sys.is_finish
                                      || (!sched_is_active() && sched_is_empty() && !sched_is_running()))) {
                sched_cleanup();
                if (thread()->used_count > 0) {
                    RAII_INFO("\nNo runnable coroutines! %d stalled\n", thread()->used_count);
                    exit(1);
                } else {
                    RAII_LOG("\nCoroutine scheduler exited");
                    exit(0);
                }
            } else if (thread()->used_count == 0
                       && (sched_is_active() && (!sched_is_empty() || sched_is_available()) && l != EMPTY)) {
                if (sched_is_available())
                    sched_steal_available(&gq_sys);
                else
                    sched_steal(&gq_sys, thread(), 1);
            } else if (l != EMPTY && gq_sys.is_multi
                       && thread()->sleeping_counted > 0 && sched_active() && !gq_sys.is_finish) {
                thread()->used_count++;
            } else if (l != EMPTY && !gq_sys.is_finish && sched_is_any_available()) {
                while (atomic_flag_load(&gq_sys.any_stealable[thread()->thrd_id]))
                    thrd_yield();

                l = NULL;
                stole = (int)atomic_load(&gq_sys.stealable_amount[thread()->thrd_id]);
                if (is_zero(stole)) {
                    l = EMPTY;
                    continue;
                }

                sched_steal(&gq_sys, thread(), stole);
                gq_sys.is_takeable--;
                atomic_store(&gq_sys.stealable_amount[thread()->thrd_id], 0);
            } else if (!thread()->is_main && gq_sys.is_multi) {
                atomic_thread_fence(memory_order_seq_cst);
                RAII_INFO("Thrd #%lx waiting to exit.\n", co_async_self());
                /* Wait for global exit signal */
                while (!gq_sys.is_finish)
                    thrd_yield();
#ifdef UV_H
                if (thread()->interrupt_default) {
                    uv_loop_close(thread()->interrupt_default);
                    thread()->interrupt_default = NULL;
                }
#endif
                RAII_INFO("Thrd #%lx exiting.\n", co_async_self());
                return thread()->exiting;
            } else {
                l = EMPTY;
                continue;
            }
        }

        t = sched_dequeue(&thread()->run_queue);
        t->ready = false;
        thread()->running = t;
        thread()->num_others_ran++;
        t->cycles++;

        coroutine_interrupt();
        if (!is_status_invalid(t) && !t->halt)
            co_switch(t);

        thread()->running = NULL;
        if (t->halt || t->exiting) {
            if (!t->system)
                --thread()->used_count;

            sched_update(t);
            if (!t->is_waiting && !t->is_channeling && !t->is_event_err) {
                co_delete(t);
            } else if (t->is_channeling) {
                co_collector(t);
            } else if (t->is_event_err && thread()->used_count == 0) {
                sched_event_cleanup(t);
            }
        }
    }
}

static void_t main_main(void_t v) {
    coroutine_name("co_main");
    thread()->exiting = co_main(main_argc, main_argv);
    atomic_thread_fence(memory_order_seq_cst);
    gq_sys.is_finish = true;
    return 0;
}

int thrd_main(void_t args) {
    int id = *(int *)args;
    CO_FREE(args);

    /* Wait for global start signal */
    while (!atomic_flag_load(&gq_sys.is_started))
        thrd_yield();

    atomic_thread_fence(memory_order_seq_cst);
    if (gq_sys.is_multi) {
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        sched_init(false, id);
        co_info(co_active(), -1);
        thrd_scheduler();
        thrd_exit(thread()->exiting);
    }

    return 0;
}

int main(int argc, char **argv) {
    main_argc = argc;
    main_argv = argv;
    int i;

    exception_setup_func = sched_unwind_setup;
    exception_unwind_func = (ex_unwind_func)co_deferred_free;
    exception_ctrl_c_func = (ex_terminate_func)sched_destroy;
    exception_terminate_func = (ex_terminate_func)sched_cleanup;
    ex_signal_setup();

    atomic_thread_fence(memory_order_seq_cst);
    gq_sys.is_multi = CO_MT_STATE;
    gq_sys.is_finish = false;
    gq_sys.is_takeable = 0;
    gq_sys.stacksize = CO_STACK_SIZE;
    gq_sys.cpu_count = sched_cpu_count();
    gq_sys.thread_count = gq_sys.cpu_count + 1;
    gq_sys.thread_invalid = gq_sys.thread_count + 61;
    atomic_init(&gq_sys.id_generate, 0);
    atomic_init(&gq_sys.used_count, 0);
    atomic_init(&gq_sys.run_queue, try_calloc(1, sizeof(atomic_scheduler_t)));
    atomic_flag_clear(&gq_sys.is_started);

#ifdef UV_H
    if (!gq_sys.is_multi)
        uv_replace_allocator(rp_malloc, rp_realloc, rp_calloc, rp_free);
    RAII_INFO("%s", sched_uname());
#endif
    if (gq_sys.is_multi && (gq_sys.threads = try_calloc(gq_sys.cpu_count, sizeof(thrd_t)))) {
        RAII_INFO(", initialized %zu threads.\n", gq_sys.cpu_count);
        for (i = 0; i < gq_sys.cpu_count; i++) {
            int *n = try_malloc(sizeof(int));
            *n = i;
            if (thrd_create(&(gq_sys.threads[i]), thrd_main, n) != thrd_success) {
                fprintf(stderr, "`thrd_create` for `thrd_main` failed to start,\nfalling back to single thread mode.\n\n");
                atomic_thread_fence(memory_order_seq_cst);
                gq_sys.is_multi = false;
                atomic_flag_test_and_set(&gq_sys.is_started);
                CO_FREE(gq_sys.threads);
                CO_FREE(n);
                gq_sys.threads = NULL;
                break;
            }
        }

        if (gq_sys.is_multi) {
            atomic_init(&gq_sys.count, try_calloc(gq_sys.thread_count, sizeof(atomic_int)));
            atomic_init(&gq_sys.available, try_calloc(gq_sys.thread_count, sizeof(atomic_size_t)));
            atomic_init(&gq_sys.stealable_thread, try_calloc(gq_sys.thread_count, sizeof(atomic_size_t)));
            atomic_init(&gq_sys.stealable_amount, try_calloc(gq_sys.thread_count, sizeof(atomic_size_t)));
            atomic_init(&gq_sys.any_stealable, try_calloc(gq_sys.thread_count, sizeof(atomic_flag)));
            for (i = 0; i < gq_sys.thread_count; i++) {
                atomic_init(&gq_sys.count[i], NULL);
                atomic_init(&gq_sys.available[i], 0);
                atomic_init(&gq_sys.stealable_thread[i], 0);
                atomic_init(&gq_sys.stealable_amount[i], 0);
                atomic_flag_clear(&gq_sys.any_stealable[i]);
            }
        }
    }

    RAII_LOG("");
    json_set_allocation_functions(try_malloc, CO_FREE);
    sched_init(true, gq_sys.cpu_count);
    create_routine(main_main, NULL, gq_sys.stacksize * 4, RUN_MAIN);
    thrd_scheduler();
    unreachable;

    return thread()->exiting;
}
