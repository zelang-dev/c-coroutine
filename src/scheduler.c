#include "coroutine.h"

typedef struct {
    int is_main;

    /* has the coroutine sleep/wait system started. */
    int sleep_activated;

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

    void_t data[3];

    /* record which coroutine is executing for scheduler */
    routine_t *running;

    /* record which coroutine sleeping in scheduler */
    scheduler_t sleep_queue[1];

    /* coroutines's FIFO scheduler queue */
    scheduler_t run_queue[1];

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

#ifdef UV_H
    uv_args_t *interrupt_args;
    uv_loop_t *interrupt_handle;
    uv_loop_t *interrupt_default;
    uv_loop_t interrupt_buffer[1];
#endif
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
static scheduler_t *SCHED_EMPTY_T = (scheduler_t *)0x200;

/* These are non-NULL pointers that will result in page faults under normal
 * circumstances, used to verify that nobody uses non-initialized entries.
 */
routine_t *EMPTY_T = (routine_t *)0x100;

/* Global queue system atomic data. */
atomic_deque_t gq_sys = {0};

static void sched_cleanup(void);
static int thrd_main(void_t args);
static int coroutine_loop(int);
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

/* Check `thread` local coroutine use count for zero. */
CO_FORCE_INLINE bool sched_empty(void) {
    return sched_count() == 0;
}

/* Check `local` run queue `head` for not `NULL`. */
CO_FORCE_INLINE bool sched_active(void) {
    return thread()->run_queue->head != NULL;
}

/* Check `global` run queue count for zero. */
CO_FORCE_INLINE bool sched_is_empty(void) {
    return atomic_load_explicit(&gq_sys.active_count, memory_order_relaxed) == 0;
}

/* Check `global` run queue count for tasks available over threshold for assigning. */
CO_FORCE_INLINE bool sched_is_takeable(void) {
    return !gq_sys.is_takeable && atomic_load_explicit(&gq_sys.active_count, memory_order_relaxed) > 0;
}

CO_FORCE_INLINE bool sched_is_sleeping(void) {
    return (thread()->sleeping_counted > 0 && sched_active());
}

static void sched_init(bool is_main, u32 thread_id) {
    thread()->is_main = is_main;
    thread()->exiting = 0;
    thread()->thrd_id = thread_id;
    thread()->sleep_activated = false;
    thread()->sleeping_counted = 0;
    thread()->used_count = 0;
    thread()->sleep_id = 0;
    thread()->active_handle = NULL;
    thread()->main_handle = NULL;
    thread()->current_handle = NULL;
    thread()->interrupt_args = NULL;
    thread()->interrupt_handle = NULL;
    thread()->interrupt_default = NULL;
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

string_t sched_uname(void) {
    if (is_str_empty(gq_sys.powered_by)) {
        uv_utsname_t buffer[1];
        uv_os_uname(buffer);
        string_t powered_by = str_concat_by(8, "Beta - ",
                                            co_itoa(thrd_cpu_count()), " cores, ",
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
    routine_t *co = co_active();
    if (!co->interrupt_active) {
        co->halt = true;
        co->status = CO_DEAD;
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
        if (co->scope->is_recovered || co_catch("sig_winch"))
            ex_flags_reset();
        else
            atomic_flag_clear(&gq_sys.is_errorless);
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

CO_FORCE_INLINE uv_loop_t *co_loop(void) {
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

static void co_init(void) {}

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

#elif defined(__ARM_EABI__)
void swap_context(routine_t *from, routine_t *to);
__asm__(
    ".text\n"
#ifdef __APPLE__
    ".globl _swap_context\n"
    "_swap_context:\n"
#else
    ".globl swap_context\n"
    ".type swap_context #function\n"
    ".hidden swap_context\n"
    "swap_context:\n"
#endif

#ifndef __SOFTFP__
    "vstmia r0!, {d8-d15}\n"
#endif
    "stmia r0, {r4-r11, lr}\n"
    ".byte 0xE5, 0x80,  0xD0, 0x24\n" /* should be "str sp, [r0, #9*4]\n", it's causing vscode display issue */
#ifndef __SOFTFP__
    "vldmia r1!, {d8-d15}\n"
#endif
    ".byte 0xE5, 0x91, 0xD0, 0x24\n" /* should be "ldr sp, [r1, #9*4]\n", it's causing vscode display issue */
    "ldmia r1, {r4-r11, pc}\n"
#ifndef __APPLE__
    ".size swap_context, .-swap_context\n"
#endif
);

static void co_init(void) {}

routine_t *co_derive(void_t memory, size_t size) {
    routine_t *ctx = (routine_t *)memory;
    if (!co_swap) {
        co_swap = (void (*)(routine_t *, routine_t *))swap_context;
    }

    ctx->d[0] = memory;
    ctx->d[1] = (void *)(co_awaitable);
    ctx->d[2] = (void *)(co_done);
    ctx->lr = (void *)(co_awaitable);
    ctx->sp = (void *)((size_t)memory + size);
#ifdef USE_VALGRIND
    size_t stack_addr = _co_align_forward((size_t)memory + sizeof(routine_t), 16);
    ctx->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif

    return ctx;
}

#elif defined(__riscv)
void swap_context(routine_t *from, routine_t *to);
__asm__(
    ".text\n"
    ".globl swap_context\n"
    ".type swap_context @function\n"
    ".hidden swap_context\n"
    "swap_context:\n"
#if __riscv_xlen == 64
    "  sd s0, 0x00(a0)\n"
    "  sd s1, 0x08(a0)\n"
    "  sd s2, 0x10(a0)\n"
    "  sd s3, 0x18(a0)\n"
    "  sd s4, 0x20(a0)\n"
    "  sd s5, 0x28(a0)\n"
    "  sd s6, 0x30(a0)\n"
    "  sd s7, 0x38(a0)\n"
    "  sd s8, 0x40(a0)\n"
    "  sd s9, 0x48(a0)\n"
    "  sd s10, 0x50(a0)\n"
    "  sd s11, 0x58(a0)\n"
    "  sd ra, 0x60(a0)\n"
    "  sd ra, 0x68(a0)\n" // pc
    "  sd sp, 0x70(a0)\n"
#ifdef __riscv_flen
#if __riscv_flen == 64
    "  fsd fs0, 0x78(a0)\n"
    "  fsd fs1, 0x80(a0)\n"
    "  fsd fs2, 0x88(a0)\n"
    "  fsd fs3, 0x90(a0)\n"
    "  fsd fs4, 0x98(a0)\n"
    "  fsd fs5, 0xa0(a0)\n"
    "  fsd fs6, 0xa8(a0)\n"
    "  fsd fs7, 0xb0(a0)\n"
    "  fsd fs8, 0xb8(a0)\n"
    "  fsd fs9, 0xc0(a0)\n"
    "  fsd fs10, 0xc8(a0)\n"
    "  fsd fs11, 0xd0(a0)\n"
    "  fld fs0, 0x78(a1)\n"
    "  fld fs1, 0x80(a1)\n"
    "  fld fs2, 0x88(a1)\n"
    "  fld fs3, 0x90(a1)\n"
    "  fld fs4, 0x98(a1)\n"
    "  fld fs5, 0xa0(a1)\n"
    "  fld fs6, 0xa8(a1)\n"
    "  fld fs7, 0xb0(a1)\n"
    "  fld fs8, 0xb8(a1)\n"
    "  fld fs9, 0xc0(a1)\n"
    "  fld fs10, 0xc8(a1)\n"
    "  fld fs11, 0xd0(a1)\n"
#else
#error "Unsupported RISC-V FLEN"
#endif
#endif //  __riscv_flen
    "  ld s0, 0x00(a1)\n"
    "  ld s1, 0x08(a1)\n"
    "  ld s2, 0x10(a1)\n"
    "  ld s3, 0x18(a1)\n"
    "  ld s4, 0x20(a1)\n"
    "  ld s5, 0x28(a1)\n"
    "  ld s6, 0x30(a1)\n"
    "  ld s7, 0x38(a1)\n"
    "  ld s8, 0x40(a1)\n"
    "  ld s9, 0x48(a1)\n"
    "  ld s10, 0x50(a1)\n"
    "  ld s11, 0x58(a1)\n"
    "  ld ra, 0x60(a1)\n"
    "  ld a2, 0x68(a1)\n" // pc
    "  ld sp, 0x70(a1)\n"
    "  jr a2\n"
#elif __riscv_xlen == 32
    "  sw s0, 0x00(a0)\n"
    "  sw s1, 0x04(a0)\n"
    "  sw s2, 0x08(a0)\n"
    "  sw s3, 0x0c(a0)\n"
    "  sw s4, 0x10(a0)\n"
    "  sw s5, 0x14(a0)\n"
    "  sw s6, 0x18(a0)\n"
    "  sw s7, 0x1c(a0)\n"
    "  sw s8, 0x20(a0)\n"
    "  sw s9, 0x24(a0)\n"
    "  sw s10, 0x28(a0)\n"
    "  sw s11, 0x2c(a0)\n"
    "  sw ra, 0x30(a0)\n"
    "  sw ra, 0x34(a0)\n" // pc
    "  sw sp, 0x38(a0)\n"
#ifdef __riscv_flen
#if __riscv_flen == 64
    "  fsd fs0, 0x3c(a0)\n"
    "  fsd fs1, 0x44(a0)\n"
    "  fsd fs2, 0x4c(a0)\n"
    "  fsd fs3, 0x54(a0)\n"
    "  fsd fs4, 0x5c(a0)\n"
    "  fsd fs5, 0x64(a0)\n"
    "  fsd fs6, 0x6c(a0)\n"
    "  fsd fs7, 0x74(a0)\n"
    "  fsd fs8, 0x7c(a0)\n"
    "  fsd fs9, 0x84(a0)\n"
    "  fsd fs10, 0x8c(a0)\n"
    "  fsd fs11, 0x94(a0)\n"
    "  fld fs0, 0x3c(a1)\n"
    "  fld fs1, 0x44(a1)\n"
    "  fld fs2, 0x4c(a1)\n"
    "  fld fs3, 0x54(a1)\n"
    "  fld fs4, 0x5c(a1)\n"
    "  fld fs5, 0x64(a1)\n"
    "  fld fs6, 0x6c(a1)\n"
    "  fld fs7, 0x74(a1)\n"
    "  fld fs8, 0x7c(a1)\n"
    "  fld fs9, 0x84(a1)\n"
    "  fld fs10, 0x8c(a1)\n"
    "  fld fs11, 0x94(a1)\n"
#elif __riscv_flen == 32
    "  fsw fs0, 0x3c(a0)\n"
    "  fsw fs1, 0x40(a0)\n"
    "  fsw fs2, 0x44(a0)\n"
    "  fsw fs3, 0x48(a0)\n"
    "  fsw fs4, 0x4c(a0)\n"
    "  fsw fs5, 0x50(a0)\n"
    "  fsw fs6, 0x54(a0)\n"
    "  fsw fs7, 0x58(a0)\n"
    "  fsw fs8, 0x5c(a0)\n"
    "  fsw fs9, 0x60(a0)\n"
    "  fsw fs10, 0x64(a0)\n"
    "  fsw fs11, 0x68(a0)\n"
    "  flw fs0, 0x3c(a1)\n"
    "  flw fs1, 0x40(a1)\n"
    "  flw fs2, 0x44(a1)\n"
    "  flw fs3, 0x48(a1)\n"
    "  flw fs4, 0x4c(a1)\n"
    "  flw fs5, 0x50(a1)\n"
    "  flw fs6, 0x54(a1)\n"
    "  flw fs7, 0x58(a1)\n"
    "  flw fs8, 0x5c(a1)\n"
    "  flw fs9, 0x60(a1)\n"
    "  flw fs10, 0x64(a1)\n"
    "  flw fs11, 0x68(a1)\n"
#else
#error "Unsupported RISC-V FLEN"
#endif
#endif // __riscv_flen
    "  lw s0, 0x00(a1)\n"
    "  lw s1, 0x04(a1)\n"
    "  lw s2, 0x08(a1)\n"
    "  lw s3, 0x0c(a1)\n"
    "  lw s4, 0x10(a1)\n"
    "  lw s5, 0x14(a1)\n"
    "  lw s6, 0x18(a1)\n"
    "  lw s7, 0x1c(a1)\n"
    "  lw s8, 0x20(a1)\n"
    "  lw s9, 0x24(a1)\n"
    "  lw s10, 0x28(a1)\n"
    "  lw s11, 0x2c(a1)\n"
    "  lw ra, 0x30(a1)\n"
    "  lw a2, 0x34(a1)\n" // pc
    "  lw sp, 0x38(a1)\n"
    "  jr a2\n"
#else
#error "Unsupported RISC-V XLEN"
#endif // __riscv_xlen
    ".size swap_context, .-swap_context\n"
);

static void co_init(void) {}

routine_t *co_derive(void_t memory, size_t size) {
    routine_t *ctx = (routine_t *)memory;
    if (!co_swap) {
        co_swap = (void (*)(routine_t *, routine_t *))swap_context;
    }

    ctx->s[0] = memory;
    ctx->s[1] = (void *)(co_awaitable);
    ctx->pc = (void *)(co_awaitable);
    ctx->ra = (void *)(co_done);
    ctx->sp = (void *)((size_t)memory + size);
#ifdef USE_VALGRIND
    size_t stack_addr = _co_align_forward((size_t)memory + sizeof(routine_t), 16);
    ctx->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + size);
#endif

    return ctx;
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
    if (atomic_flag_load(&gq_sys.is_interruptable) && !thread()->interrupt_handle) {
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
    co->is_group = false;
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
    register routine_t *co_previous_handle = thread()->active_handle;
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

CO_FORCE_INLINE void sched_enqueue(routine_t *t) {
    t->ready = true;
    sched_add(thread()->run_queue, t);
}

CO_FORCE_INLINE routine_t *sched_dequeue(scheduler_t *l) {
    routine_t *t = NULL;
    if (l->head != NULL) {
        t = l->head;
        sched_remove(l, t);
    }

    return t;
}

u32 create_routine(callable_t fn, void_t arg, u32 stack, run_states code) {
    u32 id;
    routine_t *t = co_create(stack, fn, arg);
    routine_t *c = co_active();

    if (c->interrupt_active || c->event_active)
        t->run_code = RUN_EVENT;
    else
        t->run_code = code;

    t->cid = (u32)atomic_fetch_add(&gq_sys.id_generate, 1) + 1;
    t->taken = true;
    thread()->used_count++;
    id = t->cid;
    if (c->event_active && !is_empty(c->event_group)) {
        t->event_active = true;
        t->is_waiting = true;
        hash_put(c->event_group, co_itoa(id), t);
        c->event_active = false;
    } else if (c->wait_active && !is_empty(c->wait_group) && !c->is_group_finish) {
        t->is_waiting = true;
        t->is_address = true;
        hash_put(c->wait_group, co_itoa(id), t);
    }

    if (c->interrupt_active) {
        c->interrupt_active = false;
        t->interrupt_active = true;
        t->context = c;
    }

    sched_enqueue(t);
    return id;
}

CO_FORCE_INLINE void stack_set(u32 size) {
    atomic_thread_fence(memory_order_seq_cst);
    gq_sys.stacksize = size;
}

u32 go_for(callable_args_t fn, size_t num_of_args, ...) {
    va_list ap;
    size_t i;

    args_t params = args_for_ex(co_scope(), 0);
    va_start(ap, num_of_args);
    for (i = 0; i < num_of_args; i++)
        vector_push_back(params, va_arg(ap, void_t));
    va_end(ap);

    return go((callable_t)fn, (void_t)params);
}

CO_FORCE_INLINE u32 go(callable_t fn, void_t arg) {
    return create_routine(fn, arg, gq_sys.stacksize, RUN_NORMAL);
}

static awaitable_t async_ex(callable_t fn, void_t arg) {
    awaitable_t awaitable = try_calloc(1, sizeof(struct awaitable_s));
    routine_t *c = co_active();
    wait_group_t wg = ht_wait_init(2);
    wg->grouping = ht_result_init();
    wg->resize_free = false;
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

awaitable_t async_for(callable_args_t fn, size_t num_of_args, ...) {
    va_list ap;
    size_t i;

    args_t params = args_for_ex(co_scope(), 0);
    va_start(ap, num_of_args);
    for (i = 0; i < num_of_args; i++)
        vector_push_back(params, va_arg(ap, void_t));
    va_end(ap);

    return async((callable_t)fn, (void_t)params);
}

awaitable_t async(callable_t fn, void_t arg) {
    awaitable_t awaitable = try_calloc(1, sizeof(struct awaitable_s));
    routine_t *c = co_active();
    wait_group_t wg = ht_wait_init(2);
    wg->grouping = ht_result_init();
    wg->resize_free = false;
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

CO_FORCE_INLINE void co_yield(void) {
    sched_enqueue(thread()->running);
    co_suspend();
}

static size_t nsec(void) {
    struct timeval tv;

    if (gettimeofday(&tv, 0) < 0)
        return -1;

    return (size_t)tv.tv_sec * 1000 * 1000 * 1000 + tv.tv_usec * 1000;
}

static CO_FORCE_INLINE int coroutine_loop(int mode) {
    return !atomic_flag_load(&gq_sys.is_interruptable) ? -1 : CO_EVENT_LOOP(co_loop(), mode);
}

CO_FORCE_INLINE void coroutine_interrupt(void) {
    coroutine_loop(UV_RUN_NOWAIT);
}

static void_t coroutine_wait(void_t v) {
    routine_t *t;
    size_t now;

    coroutine_system();
    if (sched_is_main())
        coroutine_name("coroutine_wait");
    else
        coroutine_name("coroutine_wait #%d", (int)thread()->thrd_id);

    for (;;) {
        /* let everyone else run */
        while (sched_yielding() > 0)
            ;
        now = nsec();
        co_info(co_active(), 1);
        while ((t = thread()->sleep_queue->head) && now >= t->alarm_time) {
            sched_remove(thread()->sleep_queue, t);
            if (!t->system && --thread()->sleeping_counted == 0)
                thread()->used_count--;

            sched_enqueue(t);
        }
    }
}

u32 sleep_for(u32 ms) {
    size_t when, now;
    routine_t *t;

    if (!thread()->sleep_activated) {
        thread()->sleep_activated = true;
        thread()->sleep_id = create_routine(coroutine_wait, NULL, CO_STACK_SIZE, RUN_SYSTEM);
    }

    now = nsec();
    when = now + (size_t)ms * 1000000;
    for (t = thread()->sleep_queue->head; !is_empty(t) && t->alarm_time < when; t = t->next)
        ;

    if (t) {
        thread()->running->prev = t->prev;
        thread()->running->next = t;
    } else {
        thread()->running->prev = thread()->sleep_queue->tail;
        thread()->running->next = NULL;
    }

    t = thread()->running;
    t->alarm_time = when;
    if (t->prev)
        t->prev->next = t;
    else
        thread()->sleep_queue->head = t;

    if (t->next)
        t->next->prev = t;
    else
        thread()->sleep_queue->tail = t;

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
    routine_t *t = thread()->running;
    va_start(args, fmt);
    vsnprintf(t->state, sizeof t->name, fmt, args);
    va_end(args);
}

void coroutine_name(char *fmt, ...) {
    va_list args;
    routine_t *t = thread()->running;
    va_start(args, fmt);
    vsnprintf(t->name, sizeof t->name, fmt, args);
    va_end(args);
}

CO_FORCE_INLINE char *coroutine_get_state(void) {
    return co_active()->state;
}

CO_FORCE_INLINE int sched_is_valid(void) {
    return thread() != NULL;
}

CO_FORCE_INLINE int sched_count(void) {
    return thread()->used_count;
}

CO_FORCE_INLINE u32 sched_id(void) {
    return thread()->thrd_id;
}

CO_FORCE_INLINE void sched_dec(void) {
    --thread()->used_count;
}

CO_FORCE_INLINE bool sched_is_main(void) {
    return thread()->is_main;
}

CO_FORCE_INLINE bool sched_is_assignable(size_t active) {
    return (active / gq_sys.thread_count) > gq_sys.cpu_count;
}

static void sched_shutdown_interrupt(bool cleaning) {
#ifdef UV_H
    if (atomic_flag_load(&gq_sys.is_interruptable)) {
        if (cleaning && atomic_flag_load(&gq_sys.is_errorless))
            interrupt_cleanup(NULL);

        if (thread()->interrupt_handle) {
            if (!cleaning && atomic_flag_load(&gq_sys.is_errorless)) {
                uv_stop(thread()->interrupt_handle);
                uv_run(thread()->interrupt_handle, UV_RUN_DEFAULT);
            }

            uv_loop_close(thread()->interrupt_handle);
            thread()->interrupt_handle = NULL;
        } else if (thread()->interrupt_default) {
            if (!cleaning && atomic_flag_load(&gq_sys.is_errorless)) {
                uv_stop(thread()->interrupt_handle);
                uv_run(thread()->interrupt_handle, UV_RUN_DEFAULT);
            }

            uv_loop_close(thread()->interrupt_default);
            thread()->interrupt_default = NULL;
        }
    }
#endif
}

CO_FORCE_INLINE uv_args_t *interrupt_listen_args(void) {
    return thread()->interrupt_args;
}

CO_FORCE_INLINE void interrupt_listen_set(uv_args_t uv_args) {
    thread()->interrupt_args = &uv_args;
}

void co_interrupt_on(void) {
#ifdef UV_H
    if (!thread()->interrupt_handle) {
        thread()->interrupt_handle = thread()->interrupt_buffer;
        if (uv_loop_init(thread()->interrupt_handle)) {
            fprintf(stderr, "Event loop creation failed in file %s at line # %d", __FILE__, __LINE__);
            thread()->interrupt_handle = NULL;
        }
    }
#endif

    atomic_flag_test_and_set(&gq_sys.is_interruptable);
    gq_sys.stacksize = CO_STACK_SIZE;
}

void co_interrupt_off(void) {
    sched_shutdown_interrupt(true);
    atomic_flag_clear(&gq_sys.is_interruptable);
    gq_sys.stacksize = CO_STACK_SIZE/2;
}

void coroutine_system(void) {
    if (!thread()->running->system) {
        thread()->running->system = true;
        thread()->running->tid = thread()->thrd_id;
        --thread()->used_count;
    }
}

void sched_exit(int val) {
    thread()->exiting = val;
    thread()->running->exiting = true;
    co_deferred_free(thread()->running);
    co_scheduler();
}

static void sched_cleanup(void) {
    if (sched_is_main()) {
        if (!can_cleanup)
            return;

        atomic_thread_fence(memory_order_seq_cst);
        can_cleanup = false;
        sched_shutdown_interrupt(true);
        chan_collector_free();
        co_collector_free();
    }
}

static void scheduler(void) {
    routine_t *t = NULL;
    scheduler_t *l = NULL;

    for (;;) {
        if (sched_empty() || !sched_active() || l == SCHED_EMPTY_T) {
            if (sched_is_main() && (l == SCHED_EMPTY_T || atomic_flag_load(&gq_sys.is_finish)
                                    || !atomic_flag_load(&gq_sys.is_errorless)
                                    || sched_is_empty())) {
                sched_cleanup();
                if (sched_count() > 0) {
                    RAII_INFO("\nNo runnable coroutines! %d stalled\n", sched_count());

                    exit(1);
                } else {
                    RAII_LOG("\nCoroutine scheduler exited");

                    exit(0);
                }
            }
        }

        t = sched_dequeue(thread()->run_queue);
        if (t == NULL)
            continue;

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

            if (!t->is_waiting && !t->is_channeling && !t->is_event_err) {
                co_delete(t);
            } else if (t->is_channeling) {
                co_collector(t);
            } else if (t->is_event_err && sched_empty()) {
                interrupt_cleanup(t);
            }
        }
    }
}

static int thrd_scheduler(void) {
    routine_t *t = NULL;
    scheduler_t *l = NULL;

    for (;;) {
        if (sched_empty() || l == SCHED_EMPTY_T
            || atomic_flag_load(&gq_sys.is_finish) || !atomic_flag_load(&gq_sys.is_errorless)) {
            RAII_INFO("Thrd #%zx waiting to exit.\033[0K\n", thrd_self());
            /* Wait for global exit signal */
            while (!atomic_flag_load(&gq_sys.is_finish))
                thrd_yield();

            RAII_INFO("Thrd #%zx exiting, %d runnable coroutines.\033[0K\n", thrd_self(), sched_count());
            return thread()->exiting;
        }

        t = sched_dequeue(thread()->run_queue);
        if (t == NULL) {
            l = SCHED_EMPTY_T;
            continue;
        }

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

            if (!t->is_waiting && !t->is_channeling && !t->is_event_err) {
                co_delete(t);
            } else if (t->is_channeling) {
                co_collector(t);
            } else if (t->is_event_err && sched_empty()) {
                interrupt_cleanup(t);
            }
        }
    }
}

static void_t thrd_main_main(void_t v) {
    (void)v;
    coroutine_name("co_thrd_main #%d", (int)sched_id());
    co_info(co_active(), -1);
    while (!sched_empty() && atomic_flag_load(&gq_sys.is_errorless) && !atomic_flag_load(&gq_sys.is_finish)) {
        if (sched_count() > 1) {
            co_yield();
        } else {
            break;
        }
    }

    return 0;
}

static int thrd_main(void_t args) {
    int res = 0, id = *(int *)args;
    CO_FREE(args);

    sched_init(false, id);
    create_routine(thrd_main_main, NULL, gq_sys.stacksize * 3, RUN_THRD);
    res = thrd_scheduler();
    sched_shutdown_interrupt(false);
    chan_collector_free();
    co_collector_free();

    return 0;
}

static void_t main_main(void_t v) {
    coroutine_name("co_main");
    thread()->exiting = co_main(main_argc, main_argv);
    return 0;
}

int main(int argc, char **argv) {
    main_argc = argc;
    main_argv = argv;

    rpmalloc_init();
    atomic_thread_fence(memory_order_seq_cst);
    exception_setup_func = sched_unwind_setup;
    exception_unwind_func = (ex_unwind_func)co_deferred_free;
    exception_ctrl_c_func = (ex_terminate_func)sched_cleanup;
    exception_terminate_func = (ex_terminate_func)sched_cleanup;

    gq_sys.is_takeable = 0;
    gq_sys.stacksize = CO_STACK_SIZE;
    gq_sys.capacity = HASH_INIT_CAPACITY;
    gq_sys.cpu_count = thrd_cpu_count();
    gq_sys.thread_count = gq_sys.cpu_count + 1;
    gq_sys.thread_invalid = gq_sys.thread_count + 61;

    atomic_init(&gq_sys.id_generate, 0);
    atomic_init(&gq_sys.active_count, 0);
    atomic_init(&gq_sys.take_count, 0);
    atomic_flag_clear(&gq_sys.is_finish);
    atomic_flag_test_and_set(&gq_sys.is_errorless);
    atomic_flag_test_and_set(&gq_sys.is_interruptable);
    ex_signal_setup();

#ifdef UV_H
    RAII_INFO("%s\n\n", sched_uname());
#endif

    json_set_allocation_functions(try_malloc, CO_FREE);
    sched_init(true, gq_sys.cpu_count);

#ifdef UV_H
    uv_replace_allocator(rp_malloc, rp_realloc, rp_calloc, rpfree);
#endif

    create_routine(main_main, NULL, gq_sys.stacksize * 4, RUN_MAIN);
    scheduler();
    unreachable;

    return thread()->exiting;
}
