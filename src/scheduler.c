#include "coroutine.h"

typedef struct {
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

    uv_loop_t interrupt_buffer[1];
    uv_loop_t *interrupt_handle;
} coroutine_task_t;
thrd_static(coroutine_task_t, task, NULL)

typedef struct {
    bool is_main;

    bool info_log;

    /* coroutine unique id generator */
    int id_generate;

    /* number of coroutines waiting in sleep mode. */
    int sleeping_counted;

    /* has the coroutine sleep/wait system started. */
    int started_wait;

    /* indicator for thread termination. */
    int exiting;

    /* track the number of coroutines used */
    int used_count;

    /* number of other coroutine that ran while the current coroutine was waiting.*/
    int num_others_ran;
    /* record which coroutine sleeping in scheduler */
    scheduler_t sleeping;

    /* record which coroutine is executing for scheduler */
    routine_t *running;

    /* coroutines's FIFO scheduler queue */
    scheduler_t run_queue;

    uv_args_t *uv_args;
} thread_processor_t;
thrd_static(thread_processor_t, thread, NULL)

static void(fastcall *co_swap)(routine_t *, routine_t *) = 0;
static char error_message[ERROR_SCRAPE_SIZE] = {0};

static volatile sig_atomic_t can_cleanup = true;
static int main_argc;
static char **main_argv;
static string co_powered_by = NULL;

/* These are non-NULL pointers that will result in page faults under normal
 * circumstances, used to verify that nobody uses non-initialized entries.
 */
static scheduler_t *EMPTY = (scheduler_t *)0x100, *ABORT = (scheduler_t *)0x200;

atomic_deque_t atomic_queue = {0};

int thrd_main(void_t args);
int coroutine_loop(int);
void coroutine_interrupt(void);

/* Initializes new coroutine, platform specific. */
routine_t *co_derive(void_t, size_t);

/* Create new coroutine. */
routine_t *co_create(size_t, callable_t, void_t);

/* Create a new coroutine running func(arg) with stack size. */
int create_routine(callable_t, void_t, unsigned int);

/* Sets the current coroutine's name.*/
void coroutine_name(char *, ...);

/* Mark the current coroutine as a ``system`` coroutine. These are ignored for the
purposes of deciding the program is done running */
void coroutine_system(void);

/* Sets the current coroutine's state name.*/
void coroutine_state(char *, ...);

/* Returns the current coroutine's state name. */
char *coroutine_get_state(void);

#ifdef _WIN32
static size_t sched_count(void) {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return (size_t)system_info.dwNumberOfProcessors;
}
#elif (defined(__linux__) || defined(__linux))
#include <sched.h>
static size_t sched_count(void) {
    // return sysconf(_SC_NPROCESSORS_ONLN);
    cpu_set_t cpuset;
    sched_getaffinity(0, sizeof(cpuset), &cpuset);
    return CPU_COUNT(&cpuset);
}
#else
static size_t sched_count(void) {
    return 1;
}
#endif

static void sched_init(bool is_main) {
    thread()->is_main = is_main;
    thread()->info_log = true;
    thread()->uv_args = NULL;
    thread()->exiting = 0;
    thread()->id_generate = 0;

    task()->active_handle = NULL;
    task()->main_handle = NULL;
    task()->current_handle = NULL;
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

C_API string sched_uname(void) {
    if (is_empty(co_powered_by)) {
        uv_utsname_t buffer[1];
        uv_os_uname(buffer);
        co_powered_by = str_concat_by(8, "Beta - Cpu: ",
                                      co_itoa(sched_count()), " core, ",
                                      buffer->sysname, " ",
                                      buffer->machine, " ",
                                      buffer->release);
    }

    return co_powered_by;
}

/* called only if routine_t func returns */
static void co_done(void) {
    if (!co_active()->loop_active) {
        co_active()->halt = true;
        co_active()->status = CO_DEAD;
    }

    co_scheduler();
}

static void co_awaitable(void) {
    routine_t *co = co_active();
    try {
        if (co->loop_active) {
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
        if (co->loop_erred) {
            co->halt = true;
            co->status = CO_ERRED;
            co->loop_active = false;
        } else if (co->loop_active) {
            co->status = CO_EVENT;
        } else if (co->event_active) {
            co->synced = false;
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
    if (!is_empty(task()->interrupt_handle))
        return task()->interrupt_handle;

    return uv_default_loop();
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
    if (!task()->active_handle)
        task()->active_handle = task()->active_buffer;

    ucontext_t *thread = (ucontext_t *)memory;
    memory = (unsigned char *)memory + sizeof(routine_t);
    size -= sizeof(routine_t);
    if ((!getcontext(thread) && !(thread->uc_stack.ss_sp = 0)) && (thread->uc_stack.ss_sp = memory)) {
        thread->uc_link = (ucontext_t *)task()->active_handle;
        thread->uc_stack.ss_size = size;
        makecontext(thread, co_func, 0);
    } else {
        co_panic("getcontext failed!");
    }

    return (routine_t *)thread;
}
#endif

routine_t *co_active(void) {
    if (!task()->active_handle)
        task()->active_handle = task()->active_buffer;
    return task()->active_handle;
}

memory_t *co_scope(void) {
    return co_active()->scope;
}

routine_t *co_current(void) {
    return task()->current_handle;
}

routine_t *co_coroutine(void) {
    return thread()->running;
}

CO_FORCE_INLINE void co_scheduler(void) {
    co_switch(task()->main_handle);
}

void co_stack_check(int n) {
    routine_t *t;

    t = thread()->running;
    if ((char *)&t <= (char *)t->stack_base || (char *)&t - (char *)t->stack_base < 256 + n || t->magic_number != CO_MAGIC_NUMBER) {
        snprintf(error_message, ERROR_SCRAPE_SIZE, "coroutine stack overflow: &t=%p stack=%p n=%d\n", &t, t->stack_base, 256 + n);
        co_panic(error_message);
    }
}

routine_t *co_create(size_t size, callable_t func, void_t args) {
    if (size != 0) {
        /* Stack size should be at least `CO_STACK_SIZE`. */
        if (size < CO_STACK_SIZE) {
            size = CO_STACK_SIZE;
        }
    } else {
        size = CO_STACK_SIZE;
    }

    size = _co_align_forward(size + sizeof(routine_t), 16); /* Stack size should be aligned to 16 bytes. */
    void_t memory = try_calloc(1, size + sizeof(channel_t) + sizeof(values_t));
    routine_t *co = co_derive(memory, size);
    if (!task()->current_handle)
        task()->current_handle = co_active();

    if (!task()->main_handle)
        task()->main_handle = task()->active_handle;

#ifdef UV_H
    if (!task()->interrupt_handle) {
        task()->interrupt_handle = task()->interrupt_buffer;
        if (uv_loop_init(task()->interrupt_handle))
            fprintf(stderr, "Event loop creation failed in file %s at line # %d", __FILE__, __LINE__);
    }
#endif

    if (UNLIKELY(raii_deferred_init(&co->scope->defer) < 0)) {
        CO_FREE(co);
        return (routine_t *)-1;
    }

    co->func = func;
    co->status = CO_SUSPENDED;
    co->stack_size = size + sizeof(channel_t) + sizeof(values_t);
    co->channeled = false;
    co->halt = false;
    co->synced = false;
    co->wait_active = false;
    co->wait_group = NULL;
    co->event_group = NULL;
    co->loop_active = false;
    co->event_active = false;
    co->process_active = false;
    co->loop_erred = false;
    co->is_plain = false;
    co->is_address = false;
    co->loop_code = 0;
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
    register routine_t *co_previous_handle = task()->active_handle;
#else
    routine_t *co_previous_handle = task()->active_handle;
#endif
    task()->active_handle = handle;
    task()->active_handle->status = CO_RUNNING;
    task()->current_handle = co_previous_handle;
    task()->current_handle->status = CO_NORMAL;
#if !defined(USE_UCONTEXT)
    co_swap(task()->active_handle, co_previous_handle);
#else
    swapcontext((ucontext_t *)co_previous_handle, (ucontext_t *)task()->active_handle);
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

static void sched_adjust(atomic_deque_t *q, routine_t *t) {
    size_t generate_id = atomic_load_explicit(&q->id_generate, memory_order_relaxed);
    size_t coroutines_num_all = atomic_load_explicit(&q->n_all_coroutine, memory_order_relaxed);
    routine_t **coroutines_all = (routine_t **)atomic_load_explicit(&q->all_coroutine, memory_order_acquire);
    if (coroutines_num_all % 64 == 0) {
        coroutines_all = CO_REALLOC(coroutines_all, (coroutines_num_all + 64) * sizeof(coroutines_all[0]));
        if (is_empty(coroutines_all))
            co_panic("realloc() failed");
    }

    t->cid = ++generate_id;
    t->all_coroutine_slot = coroutines_num_all;
    coroutines_all[coroutines_num_all++] = t;
    coroutines_all[coroutines_num_all] = NULL;
    atomic_store(&q->id_generate, generate_id);
    atomic_store(&q->n_all_coroutine, coroutines_num_all);
    sched_enqueue(t);
    atomic_store_explicit(&q->all_coroutine, coroutines_all, memory_order_release);
}

int create_routine(callable_t fn, void_t arg, unsigned int stack) {
    int id;
    routine_t *t = co_create(stack, fn, arg);
    routine_t *c = co_active();

    thread()->used_count++;
    atomic_fetch_add(&atomic_queue.used_count, 1);
    sched_adjust(&atomic_queue, t);
    id = t->cid;
    if (c->event_active && !is_empty(c->event_group)) {
        t->event_active = true;
        t->synced = true;
        hash_put(c->event_group, co_itoa(id), t);
        c->event_active = false;
    } else if (c->wait_active && !is_empty(c->wait_group)) {
        t->synced = true;
        t->is_address = true;
        hash_put(c->wait_group, co_itoa(id), t);
        c->wait_counter++;
    }

    if (c->loop_active) {
        c->loop_active = false;
        t->loop_active = true;
        t->context = c;
    }

    return id;
}

CO_FORCE_INLINE int co_go(callable_t fn, void_t arg) {
    return create_routine(fn, arg, CO_STACK_SIZE);
}

void co_yield(void) {
    sched_enqueue(thread()->running);
    co_suspend();
}

CO_FORCE_INLINE void co_execute(func_t fn, void_t arg) {
    create_routine((callable_t)fn, arg, CO_STACK_SIZE);
    co_yield();
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
    int ms;
    routine_t *t;
    size_t now;

    coroutine_system();
    coroutine_name("coroutine_wait");
    for (;;) {
        /* let everyone else run */
        while (sched_yielding() > 0)
            ;
        /* we're the only one runnable - check for i/o, the event loop */
        coroutine_state("event loop");
        if ((t = thread()->sleeping.head) == NULL) {
            ms = -1;
        } else {
            /* sleep at most 5s */
            now = nsec();
            if (now >= t->alarm_time)
                ms = 0;
            else if (now + 5 * 1000 * 1000 * 1000LL >= t->alarm_time)
                ms = (int)(t->alarm_time - now) / 1000000;
            else
                ms = 5000;
        }

        now = nsec();
        while ((t = thread()->sleeping.head) && now >= t->alarm_time) {
            sched_remove(&thread()->sleeping, t);
            if (!t->system && --thread()->sleeping_counted == 0)
                thread()->used_count--;

            sched_enqueue(t);
        }
    }
}

unsigned int co_sleep(unsigned int ms) {
    size_t when, now;
    routine_t *t;

    if (!thread()->started_wait) {
        thread()->started_wait = 1;
        create_routine(coroutine_wait, 0, CO_STACK_SIZE);
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

    return (unsigned int)(nsec() - now) / 1000000;
}

void sched_enqueue(routine_t *t) {
    t->ready = 1;
    sched_add(&thread()->run_queue, t);
}

int sched_yielding(void) {
    int n;
    n = thread()->num_others_ran;
    sched_enqueue(thread()->running);
    coroutine_state("yield");
    co_suspend();
    return thread()->num_others_ran - n - 1;
}

bool sched_active(void) {
    return !is_empty(thread()->run_queue.head);
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

CO_FORCE_INLINE void sched_log_reset(void) {
    thread()->info_log = false;
}

CO_FORCE_INLINE uv_args_t *sched_event_args(void) {
    return thread()->uv_args;
}

void coroutine_system(void) {
    if (!thread()->running->system) {
        thread()->running->system = 1;
        --thread()->used_count;
    }
}

void sched_exit(int val) {
    thread()->exiting = val;
    thread()->running->exiting = true;
    co_deferred_free(thread()->running);
    co_scheduler();
}

void sched_info(void) {
    int i;
    routine_t *t;
    char *extra;

    size_t coroutines_num_all = atomic_load(&atomic_queue.n_all_coroutine);
    routine_t **coroutines_all = (routine_t **)atomic_load(&atomic_queue.all_coroutine);
    fprintf(stderr, "Coroutine list:\n");
    for (i = 0; i < coroutines_num_all; i++) {
        t = coroutines_all[i];
        if (t == thread()->running)
            extra = " (running)";
        else if (t->ready)
            extra = " (ready)";
        else
            extra = "";

        fprintf(stderr, "%6d%c %-20s %s%s cycles: %zu %d\n", t->cid, t->system ? 's' : ' ', t->name, t->state, extra, t->cycles, t->status);
    }
    fprintf(stderr, "\n\n");
}

void sched_update(routine_t *t) {
    size_t coroutines_num_all = atomic_load(&atomic_queue.n_all_coroutine);
    routine_t **coroutines_all = (routine_t **)atomic_load_explicit(&atomic_queue.all_coroutine, memory_order_acquire);
    int i = t->all_coroutine_slot;
    coroutines_all[i] = coroutines_all[--coroutines_num_all];
    coroutines_all[i]->all_coroutine_slot = i;
    atomic_store_explicit(&atomic_queue.all_coroutine, coroutines_all, memory_order_release);
    atomic_store(&atomic_queue.n_all_coroutine, coroutines_num_all);
}

void sched_cleanup(void) {
    routine_t *t;
    if (!can_cleanup)
        return;

    can_cleanup = false;
    gc_channel_free();
    gc_coroutine_free();
    size_t coroutines_num_all = atomic_load(&atomic_queue.n_all_coroutine);
    routine_t **coroutines_all = (routine_t **)atomic_load(&atomic_queue.all_coroutine);

    if (coroutines_num_all) {
        if (thread()->used_count)
            coroutines_num_all--;

        for (int i = 0; i < (coroutines_num_all + (thread()->used_count != 0)); i++) {
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
    if (!is_empty(task()->interrupt_handle)) {
        uv_loop_close(task()->interrupt_handle);
        task()->interrupt_handle = NULL;
    }

    if (!is_empty(co_powered_by)) {
        CO_FREE(co_powered_by);
        co_powered_by = NULL;
    }
#endif

    atomic_store(&atomic_queue.all_coroutine, (c89atomic_uint64)NULL);
    atomic_store(&atomic_queue.n_all_coroutine, 0);
    free((void *)atomic_load(&atomic_queue.run_queue));
}

static void thrd_scheduler(void) {
    routine_t *t;

    for (;;) {
        // Todo: Add additional check for active separate running threads.
        if (thread()->used_count == 0 || !sched_active()) {
            if (thread()->is_main) {
                sched_cleanup();
                if (thread()->used_count > 0) {
                    CO_INFO("No runnable coroutines! %d stalled\n", thread()->used_count);
                    exit(1);
                } else {
                    CO_LOG("Coroutine scheduler exited");
                    exit(thread()->exiting);
                }
            } else {
                // Todo: In separate running thread, so steal work or exit thread.
            }
        }

        t = thread()->run_queue.head;
        sched_remove(&thread()->run_queue, t);
        t->ready = 0;
        thread()->running = t;
        thread()->num_others_ran++;
        t->cycles++;
        if (thread()->info_log)
            CO_INFO("Thread #%lx running coroutine id: %d (%s) status: %d cycles: %zu\n", co_async_self(), t->cid,
                    ((!is_empty(t->name) && t->cid > 0) ? t->name : !t->channeled ? "" : "channel"), t->status, t->cycles);

        coroutine_interrupt();
        if (!is_status_invalid(t) && !t->halt) {
            co_switch(t);
        }

        if (thread()->info_log)
            CO_LOG("Back at coroutine scheduling");

        thread()->running = NULL;
        if (t->halt || t->exiting) {
            if (!t->system)
                --thread()->used_count;

            sched_update(t);
            if (!t->synced && !t->channeled && !t->loop_erred) {
                co_delete(t);
            } else if (t->channeled) {
                gc_coroutine(t);
            } else if (t->loop_erred && thread()->used_count == 0) {
                coroutine_event_cleanup(t);
            }
        }
    }
}

static void_t coroutine_main(void_t v) {
    coroutine_name("co_main");
    thread()->exiting = co_main(main_argc, main_argv);
    return 0;
}

int thrd_main(void_t args) {
    return 0;
}

int main(int argc, char **argv) {
    main_argc = argc;
    main_argv = argv;

    sched_init(true);
    atomic_queue.cpu_count = sched_count();
    atomic_init(&atomic_queue.id_generate, 0);
    atomic_init(&atomic_queue.used_count, 0);
    atomic_init(&atomic_queue.run_queue, try_calloc(1, sizeof(atomic_scheduler_t)));
    atomic_init(&atomic_queue.run_queue.head, NULL);
    atomic_init(&atomic_queue.run_queue.tail, NULL);

    exception_setup_func = sched_unwind_setup;
    exception_unwind_func = (ex_unwind_func)co_deferred_free;
    exception_ctrl_c_func = (ex_terminate_func)sched_info;
    exception_terminate_func = (ex_terminate_func)sched_cleanup;
#ifdef UV_H
    CO_INFO("%s\n\n", sched_uname());
#endif
    ex_signal_setup();
    json_set_allocation_functions(try_malloc, CO_FREE);
    create_routine(coroutine_main, NULL, CO_MAIN_STACK);
    thrd_scheduler();
    unreachable;

    return thread()->exiting;
}
