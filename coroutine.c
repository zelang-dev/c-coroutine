#include "coroutine.h"

static thread_local co_routine_t co_active_buffer[64];
/* Variable holding the current running coroutine per thread. */
static thread_local co_routine_t *co_active_handle = NULL;
/* Variable holding the main target that gets called once an coroutine
function fully completes and return. */
static thread_local co_routine_t *co_main_handle = NULL;
/* Variable holding the previous running coroutine per thread. */
static thread_local co_routine_t *co_current_handle = NULL;
static void(fastcall *co_swap)(co_routine_t *, co_routine_t *) = 0;

static int main_argc;
static char **main_argv;

/* coroutine unique id generator */
static int co_id_generate;

static co_queue_t sleeping;
static int sleeping_counted;
static int started_wait;


/* Exception handling with longjmp() */
jmp_buf exception_buffer;
int exception_status;

int exiting = 0;

/* number of other coroutine that ran while the current coroutine was waiting.*/
int n_co_switched;
int n_all_coroutine;

/* track the number of coroutines used */
int co_count;

/* record which coroutine is executing for scheduler */
co_routine_t *co_running;

/* coroutines's FIFO scheduler queue */
co_queue_t co_run_queue;

/* scheduler tracking for all coroutines */
co_routine_t **all_coroutine;

volatile C_ERROR_FRAME_T CExceptionFrames[C_ERROR_NUM_ID] = {{0}};

void throw(C_ERROR_T ExceptionID)
{
    unsigned int _ID = co_active()->cid;
    CExceptionFrames[_ID].Exception = ExceptionID;
    if (CExceptionFrames[_ID].pFrame)
    {
        longjmp(*CExceptionFrames[_ID].pFrame, 1);
    }
    C_ERROR_NO_CATCH_HANDLER(ExceptionID);
}

/* called only if co_routine_t func returns */
static void co_done()
{
    co_active()->halt = true;
    co_active()->status = CO_DEAD;
    co_switch(coroutine_active() ? co_main_handle : co_running);
}

static void co_awaitable()
{
    co_routine_t *co = co_active();
    co->results = co->func(co->args);
    co->status = CO_NORMAL;
    co_deferred_free(co);
}

static void co_func(co_routine_t handle)
{
    co_awaitable();
    co_done(); /* called only if coroutine function returns */
}

/* Utility for aligning addresses. */
static CO_FORCE_INLINE size_t _co_align_forward(size_t addr, size_t align)
{
    return (addr + (align - 1)) & ~(align - 1);
}

static CO_FORCE_INLINE int co_deferred_array_init(defer_t *array)
{
    return co_array_init((co_array_t *)array);
}

static CO_FORCE_INLINE void co_deferred_array_sort(defer_t *array, int (*cmp)(const void *a, const void *b))
{
    co_array_sort((co_array_t *)array, sizeof(defer_func_t), cmp);
}

static CO_FORCE_INLINE defer_t *co_deferred_array_new(co_routine_t *coro)
{
    return (defer_t *)co_array_new(coro);
}

static CO_FORCE_INLINE defer_func_t *co_deferred_array_append(defer_t *array)
{
    return (defer_func_t *)co_array_append((co_array_t *)array, sizeof(defer_func_t));
}

static CO_FORCE_INLINE int co_deferred_array_reset(defer_t *array)
{
    return co_array_reset((co_array_t *)array);
}

static CO_FORCE_INLINE co_array_t *co_deferred_array_get_array(defer_t *array)
{
    return (co_array_t *)array->base.base;
}

static CO_FORCE_INLINE size_t co_deferred_array_get_index(const defer_t *array, co_array_t *elem)
{
    CO_ASSERT(elem >= (co_array_t *)array->base.base);
    CO_ASSERT(elem < (co_array_t *)array->base.base + array->base.elements);
    return (size_t)(elem - (co_array_t *)array->base.base);
}

static CO_FORCE_INLINE co_array_t *co_deferred_array_get_element(const defer_t *array, size_t index)
{
    CO_ASSERT(index <= array->base.elements);
    return &((co_array_t *)array->base.base)[index];
}

static CO_FORCE_INLINE size_t co_deferred_array_len(const defer_t *array)
{
    return array->base.elements;
}

#ifdef CO_MPROTECT
    alignas(4096)
#else
    section(text)
#endif

#if ((defined(__clang__) || defined(__GNUC__)) && defined(__i386__)) || (defined(_MSC_VER) && defined(_M_IX86))
    /* ABI: fastcall */
    static const unsigned char co_swap_function[4096] = {
        0x89, 0x22,        /* mov [edx],esp    */
        0x8b, 0x21,        /* mov esp,[ecx]    */
        0x58,              /* pop eax          */
        0x89, 0x6a, 0x04,  /* mov [edx+ 4],ebp */
        0x89, 0x72, 0x08,  /* mov [edx+ 8],esi */
        0x89, 0x7a, 0x0c,  /* mov [edx+12],edi */
        0x89, 0x5a, 0x10,  /* mov [edx+16],ebx */
        0x8b, 0x69, 0x04,  /* mov ebp,[ecx+ 4] */
        0x8b, 0x71, 0x08,  /* mov esi,[ecx+ 8] */
        0x8b, 0x79, 0x0c,  /* mov edi,[ecx+12] */
        0x8b, 0x59, 0x10,  /* mov ebx,[ecx+16] */
        0xff, 0xe0,        /* jmp eax          */
    };

    #ifdef _WIN32
        #include <windows.h>
        static void co_init(void) {
            #ifdef CO_MPROTECT
                DWORD old_privileges;
                VirtualProtect((void*)co_swap_function, sizeof co_swap_function, PAGE_EXECUTE_READ, &old_privileges);
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
                mprotect((void*)base, size, PROT_READ | PROT_EXEC);
            #endif
        }
    #endif

    co_routine_t *co_derive(void* memory, size_t size, co_callable_t func, void *args) {
        co_routine_t *handle;
        if(!co_swap) {
            co_init();
            co_swap = (void(fastcall *)(co_routine_t *, co_routine_t *))co_swap_function;
        }
        if(!co_active_handle) co_active_handle = co_active_buffer;
        if(!co_main_handle) co_main_handle = co_active_handle;
        if(!co_current_handle) co_current_handle = co_active();

        if((handle = (co_routine_t *)memory)) {
            size_t stack_top = (size_t)handle + size;
            stack_top -= 32;
            stack_top &= ~((size_t)15);
            long *p = (long *)(stack_top);              /* seek to top of stack */
            *--p = (long)co_done;                       /* if func returns */
            *--p = (long)co_awaitable;                  /* start of function */
            *(long*)handle = (long)p;                   /* stack pointer */

#ifdef CO_USE_VALGRIND
            size_t stack_addr = _co_align_forward((size_t)handle + sizeof(co_routine_t), 16);
            handle->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + stack_size);
#endif
            handle->func = func;
            handle->status = CO_SUSPENDED;
            handle->stack_size = size;
            handle->halt = false;
            handle->args = args;
            handle->magic_number = CO_MAGIC_NUMBER;
        }

        return handle;
    }
#elif ((defined(__clang__) || defined(__GNUC__)) && defined(__amd64__)) || (defined(_MSC_VER) && defined(_M_AMD64))
    #ifdef _WIN32
        /* ABI: Win64 */
        static const unsigned char co_swap_function[4096] = {
            0x48, 0x89, 0x22,              /* mov [rdx],rsp           */
            0x48, 0x8b, 0x21,              /* mov rsp,[rcx]           */
            0x58,                          /* pop rax                 */
            0x48, 0x83, 0xe9, 0x80,        /* sub rcx,-0x80           */
            0x48, 0x83, 0xea, 0x80,        /* sub rdx,-0x80           */
            0x48, 0x89, 0x6a, 0x88,        /* mov [rdx-0x78],rbp      */
            0x48, 0x89, 0x72, 0x90,        /* mov [rdx-0x70],rsi      */
            0x48, 0x89, 0x7a, 0x98,        /* mov [rdx-0x68],rdi      */
            0x48, 0x89, 0x5a, 0xa0,        /* mov [rdx-0x60],rbx      */
            0x4c, 0x89, 0x62, 0xa8,        /* mov [rdx-0x58],r12      */
            0x4c, 0x89, 0x6a, 0xb0,        /* mov [rdx-0x50],r13      */
            0x4c, 0x89, 0x72, 0xb8,        /* mov [rdx-0x48],r14      */
            0x4c, 0x89, 0x7a, 0xc0,        /* mov [rdx-0x40],r15      */
        #if !defined(CO_NO_SSE)
            0x0f, 0x29, 0x72, 0xd0,        /* movaps [rdx-0x30],xmm6  */
            0x0f, 0x29, 0x7a, 0xe0,        /* movaps [rdx-0x20],xmm7  */
            0x44, 0x0f, 0x29, 0x42, 0xf0,  /* movaps [rdx-0x10],xmm8  */
            0x44, 0x0f, 0x29, 0x0a,        /* movaps [rdx],     xmm9  */
            0x44, 0x0f, 0x29, 0x52, 0x10,  /* movaps [rdx+0x10],xmm10 */
            0x44, 0x0f, 0x29, 0x5a, 0x20,  /* movaps [rdx+0x20],xmm11 */
            0x44, 0x0f, 0x29, 0x62, 0x30,  /* movaps [rdx+0x30],xmm12 */
            0x44, 0x0f, 0x29, 0x6a, 0x40,  /* movaps [rdx+0x40],xmm13 */
            0x44, 0x0f, 0x29, 0x72, 0x50,  /* movaps [rdx+0x50],xmm14 */
            0x44, 0x0f, 0x29, 0x7a, 0x60,  /* movaps [rdx+0x60],xmm15 */
        #endif
            0x48, 0x8b, 0x69, 0x88,        /* mov rbp,[rcx-0x78]      */
            0x48, 0x8b, 0x71, 0x90,        /* mov rsi,[rcx-0x70]      */
            0x48, 0x8b, 0x79, 0x98,        /* mov rdi,[rcx-0x68]      */
            0x48, 0x8b, 0x59, 0xa0,        /* mov rbx,[rcx-0x60]      */
            0x4c, 0x8b, 0x61, 0xa8,        /* mov r12,[rcx-0x58]      */
            0x4c, 0x8b, 0x69, 0xb0,        /* mov r13,[rcx-0x50]      */
            0x4c, 0x8b, 0x71, 0xb8,        /* mov r14,[rcx-0x48]      */
            0x4c, 0x8b, 0x79, 0xc0,        /* mov r15,[rcx-0x40]      */
        #if !defined(CO_NO_SSE)
            0x0f, 0x28, 0x71, 0xd0,        /* movaps xmm6, [rcx-0x30] */
            0x0f, 0x28, 0x79, 0xe0,        /* movaps xmm7, [rcx-0x20] */
            0x44, 0x0f, 0x28, 0x41, 0xf0,  /* movaps xmm8, [rcx-0x10] */
            0x44, 0x0f, 0x28, 0x09,        /* movaps xmm9, [rcx]      */
            0x44, 0x0f, 0x28, 0x51, 0x10,  /* movaps xmm10,[rcx+0x10] */
            0x44, 0x0f, 0x28, 0x59, 0x20,  /* movaps xmm11,[rcx+0x20] */
            0x44, 0x0f, 0x28, 0x61, 0x30,  /* movaps xmm12,[rcx+0x30] */
            0x44, 0x0f, 0x28, 0x69, 0x40,  /* movaps xmm13,[rcx+0x40] */
            0x44, 0x0f, 0x28, 0x71, 0x50,  /* movaps xmm14,[rcx+0x50] */
            0x44, 0x0f, 0x28, 0x79, 0x60,  /* movaps xmm15,[rcx+0x60] */
        #endif
        #if !defined(CO_NO_TIB)
            0x65, 0x4c, 0x8b, 0x04, 0x25,  /* mov r8,gs:0x30          */
            0x30, 0x00, 0x00, 0x00,
            0x41, 0x0f, 0x10, 0x40, 0x08,  /* movups xmm0,[r8+0x8]    */
            0x0f, 0x29, 0x42, 0x70,        /* movaps [rdx+0x70],xmm0  */
            0x0f, 0x28, 0x41, 0x70,        /* movaps xmm0,[rcx+0x70]  */
            0x41, 0x0f, 0x11, 0x40, 0x08,  /* movups [r8+0x8],xmm0    */
        #endif
            0xff, 0xe0,                    /* jmp rax                 */
        };

        #include <windows.h>

        static void co_init(void)
        {
        #ifdef CO_MPROTECT
            DWORD old_privileges;
            VirtualProtect((void *)co_swap_function, sizeof co_swap_function, PAGE_EXECUTE_READ, &old_privileges);
        #endif
        }
    #else
        /* ABI: SystemV */
        static const unsigned char co_swap_function[4096] = {
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

        static void co_init(void)
        {
        #ifdef CO_MPROTECT
            unsigned long long addr = (unsigned long long)co_swap_function;
            unsigned long long base = addr - (addr % sysconf(_SC_PAGESIZE));
            unsigned long long size = (addr - base) + sizeof co_swap_function;
            mprotect((void *)base, size, PROT_READ | PROT_EXEC);
        #endif
        }
#endif
    co_routine_t *co_derive(void *memory, size_t size, co_callable_t func, void *args)
    {
        co_routine_t *handle;
        if (!co_swap) {
            co_init();
            co_swap = (void (*)(co_routine_t *, co_routine_t *))co_swap_function;
        }
        if (!co_active_handle) co_active_handle = co_active_buffer;
        if (!co_main_handle) co_main_handle = co_active_handle;
        if (!co_current_handle) co_current_handle = co_active();

        if ((handle = (co_routine_t *)memory)) {
            size_t stack_top = (size_t)handle + size;
            stack_top -= 32;
            stack_top &= ~((size_t)15);
            long long *p = (long long *)(stack_top);               /* seek to top of stack */
            *--p = (long long)co_done;                            /* if coroutine returns */
            *--p = (long long)co_awaitable;
            *(long long *)handle = (long long)p;              /* stack pointer */
#if defined(_WIN32) && !defined(CO_NO_TIB)
            ((long long *)handle)[30] = (long long)handle + size; /* stack base */
            ((long long *)handle)[31] = (long long)handle;        /* stack limit */
#endif

#ifdef CO_USE_VALGRIND
            size_t stack_addr = _co_align_forward((size_t)handle + sizeof(co_routine_t), 16);
            handle->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + stack_size);
#endif
            handle->func = func;
            handle->status = CO_SUSPENDED;
            handle->stack_size = size;
            handle->halt = false;
            handle->args = args;
            handle->magic_number = CO_MAGIC_NUMBER;
        }

        return handle;
    }
#elif defined(__clang__) || defined(__GNUC__)
  #if defined(__arm__)
    #ifdef CO_MPROTECT
        #include <unistd.h>
        #include <sys/mman.h>
    #endif

    static const size_t co_swap_function[1024] = {
        0xe8a16ff0,  /* stmia r1!, {r4-r11,sp,lr} */
        0xe8b0aff0,  /* ldmia r0!, {r4-r11,sp,pc} */
        0xe12fff1e,  /* bx lr                     */
    };

    static void co_init(void) {
    #ifdef CO_MPROTECT
        size_t addr = (size_t)co_swap_function;
        size_t base = addr - (addr % sysconf(_SC_PAGESIZE));
        size_t size = (addr - base) + sizeof co_swap_function;
        mprotect((void*)base, size, PROT_READ | PROT_EXEC);
    #endif
    }

    co_routine_t *co_derive(void *memory, size_t size, co_callable_t func, void *args) {
        size_t *handle;
        co_routine_t *co;
        if (!co_swap) {
            co_init();
            co_swap = (void (*)(co_routine_t *, co_routine_t *))co_swap_function;
        }
        if (!co_active_handle) co_active_handle = co_active_buffer;
        if (!co_main_handle) co_main_handle = co_active_handle;
        if (!co_current_handle) co_current_handle = co_active();

        if((handle = (size_t *)memory)) {
            size_t stack_top = (size_t)handle + size;
            stack_top &= ~((size_t)15);
            size_t *p = (size_t *)(stack_top);
            handle[8] = (size_t)p;
            handle[9] = (size_t)co_func;

            co = (co_routine_t *)handle;
#ifdef CO_USE_VALGRIND
            size_t stack_addr = _co_align_forward((size_t)co + sizeof(co_routine_t), 16);
            handle->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + stack_size);
#endif
            co->func = func;
            co->status = CO_SUSPENDED;
            co->stack_size = size;
            co->halt = false;
            co->args = args;
            co->magic_number = CO_MAGIC_NUMBER;
        }

        return co;
    }
#elif defined(__aarch64__)
#include <stdint.h>
static const uint32_t co_swap_function[1024] = {
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

    static void co_init() {
    #ifdef CO_MPROTECT
        DWORD old_privileges;
        VirtualProtect((void*)co_swap_function, sizeof co_swap_function, PAGE_EXECUTE_READ, &old_privileges);
    #endif
    }
#else
    #ifdef CO_MPROTECT
        #include <unistd.h>
        #include <sys/mman.h>
    #endif

    static void co_init(void)
    {
    #ifdef CO_MPROTECT
        size_t addr = (size_t)co_swap_function;
        size_t base = addr - (addr % sysconf(_SC_PAGESIZE));
        size_t size = (addr - base) + sizeof co_swap_function;
        mprotect((void *)base, size, PROT_READ | PROT_EXEC);
    #endif
    }
#endif
    co_routine_t *co_derive(void *memory, size_t size, co_callable_t func, void *args)
    {
        size_t *handle;
        co_routine_t *co;
        if (!co_swap) {
            co_init();
            co_swap = (void (*)(co_routine_t *, co_routine_t *))co_swap_function;
        }
        if (!co_active_handle) co_active_handle = co_active_buffer;
        if (!co_main_handle) co_main_handle = co_active_handle;
        if (!co_current_handle) co_current_handle = co_active();

        if ((handle = (size_t *)memory)) {
            size_t stack_top = (size_t)handle + size;
            stack_top &= ~((size_t)15);
            size_t *p = (size_t *)(stack_top);
            handle[0] = (size_t)p;                 /* x16 (stack pointer) */
            handle[1] = (size_t)co_func;           /* x30 (link register) */
            handle[2] = (size_t)co_awaitable;      /* x19 (entry point) */
            handle[12] = (size_t)p;                /* x29 (frame pointer) */

#if defined(_WIN32) && !defined(CO_NO_TIB)
            handle[22] = (size_t)handle + size;    /* stack base */
            handle[23] = (size_t)handle;           /* stack limit */
#endif

            co = (co_routine_t *)handle;
#ifdef CO_USE_VALGRIND
            size_t stack_addr = _co_align_forward((size_t)co + sizeof(co_routine_t), 16);
            co->vg_stack_id = VALGRIND_STACK_REGISTER(stack_addr, stack_addr + stack_size);
#endif
            co->func = func;
            co->status = CO_SUSPENDED;
            co->stack_size = size;
            co->halt = false;
            co->args = args;
            co->magic_number = CO_MAGIC_NUMBER;
        }

        return co;
    }
#elif defined(__powerpc64__) && defined(_CALL_ELF) && _CALL_ELF == 2
    #define USE_NATIVE
    #include "ppc64v2.c"
#elif defined(_ARCH_PPC) && !defined(__LITTLE_ENDIAN__)
    #define USE_NATIVE
    #include "ppc.c"
#elif defined(_WIN32)
    #define USE_FIBER
#else
    #define USE_NATIVE
    #include "sjlj.c"
#endif
#elif defined(_MSC_VER)
  #define USE_FIBER
#else
  #error "Unsupported processor, compiler or operating system"
#endif

#ifdef USE_FIBER
    #define WINVER 0x0400
    #define _WIN32_WINNT 0x0400
    #include <windows.h>

    static thread_local co_routine_t *co_active_ = NULL;
    static void __stdcall co_thunk(void *func)
    {
        ((void (*)(void))func)();
    }

    co_routine_t *co_active(void)
    {
        if (!co_active_) {
            ConvertThreadToFiber(0);
            co_active_ = (co_routine_t *)GetCurrentFiber();
        }
        return co_active_;
    }

    co_routine_t *co_derive(void *memory, size_t heapsize, co_callable_t func, void *args)
    {
        /* Windows fibers do not allow users to supply their own memory */
        return (co_routine_t *)0;
    }

    co_routine_t *co_create(size_t heapsize, co_callable_t func, void *args)
    {
        if (!co_active_) {
            ConvertThreadToFiber(0);
            co_active_ = (co_routine_t *)GetCurrentFiber();
        }
        return (co_routine_t *)CreateFiber(heapsize, co_thunk, (void *)func);
    }

    void co_delete(co_routine_t *thread)
    {
        DeleteFiber((LPVOID)thread);
        handle->status = CO_DEAD;
    }

    void co_switch(co_routine_t *thread)
    {
        co_active_ = thread;
        SwitchToFiber((LPVOID)thread);
    }

    bool co_serializable(void)
    {
        return false;
    }
#elif !defined(USE_NATIVE)
    co_routine_t *co_active(void)
    {
        if (!co_active_handle)
            co_active_handle = co_active_buffer;
        return co_active_handle;
    }

    co_routine_t *co_create(size_t size, co_callable_t func, void *args)
    {
        if (size != 0)  {
            /* Stack size should be at least `CO_STACK_SIZE`. */
            if (size < CO_STACK_SIZE) {
                size = CO_STACK_SIZE;
            }
        } else {
            size = CO_STACK_SIZE;
        }

        size = _co_align_forward(size + sizeof(co_routine_t), 16); /* Stack size should be aligned to 16 bytes. */
        void *memory = CO_MALLOC(size);
        if (!memory)
            return (co_routine_t *)0;

        memset(memory, 0, size);
        co_routine_t *co = co_derive(memory, size, func, args);
        if (UNLIKELY(co_deferred_array_init(&co->defer) < 0)) {
            free(co);
            return (co_routine_t *)-1;
        }

        co->stack_base = (unsigned char *)(co + 1);
        return co;
    }

    void co_delete(co_routine_t *handle)
    {
        if (!handle) {
            CO_LOG("attempt to delete an invalid coroutine");
        } else if (!(handle->status == CO_NORMAL || handle->status == CO_DEAD) && !handle->exiting) {
            CO_LOG("attempt to delete a coroutine that is not dead or suspended");
        } else {
#ifdef CO_USE_VALGRIND
            if (handle->vg_stack_id != 0) {
                VALGRIND_STACK_DEREGISTER(handle->vg_stack_id);
                handle->vg_stack_id = 0;
            }
#endif
            CO_FREE(handle);
            handle->status = CO_DEAD;
        }
    }

    void co_switch(co_routine_t *handle)
    {
        co_routine_t *co_previous_handle = co_active_handle;
        co_active_handle = handle;
        co_active_handle->status = CO_RUNNING;
        co_current_handle->status = CO_NORMAL;
        co_current_handle = co_previous_handle;
        co_swap(co_active_handle, co_previous_handle);
    }

    bool co_serializable(void)
    {
        return true;
    }
#endif

void *co_user_data(co_routine_t *co)
{
  if(co != NULL) {
    return co->user_data;
  }
  return NULL;
}

co_state co_status(co_routine_t *co)
{
  if(co != NULL) return co->status;

  return CO_DEAD;
}

co_routine_t *co_current(void)
{
  return co_current_handle;
}

value_t co_value(void *data)
{
  if (data) return ((co_value_t *)data)->value;

  CO_LOG("attempt to get value on null");
  return ((co_value_t *)0)->value;
}

co_routine_t *co_start(co_callable_t func, void *args)
{
  size_t stack_size = CO_STACK_SIZE;
  stack_size = _co_align_forward(stack_size + sizeof(co_routine_t), 16); /* Stack size should be aligned to 16 bytes. */
  void *memory = CO_MALLOC(stack_size);
  if (!memory) return (co_routine_t *)0;

  memset(memory, 0, stack_size);
  co_routine_t *co = co_derive(memory, stack_size, func, args);
  if (UNLIKELY(co_deferred_array_init(&co->defer) < 0)) {
    free(co);
    return (co_routine_t *)-1;
  }

  co->stack_base = (unsigned char *)(co + 1);
  return co;
}

void co_suspend(void)
{
  co_suspend_set(NULL);
}

void co_suspend_set(void *data)
{
  co_stack_check(0);
  co_active()->yield_value = data;
  co_switch(co_current());
}

void co_yielding(co_routine_t *handle, void *data)
{
  co_stack_check(0);
  handle->yield_value = data;
  co_switch(handle);
}

void *co_yielding_get(co_routine_t *handle, void *data)
{
  co_yielding(handle, data);
  return handle->resume_value;
}

void *co_resuming(co_routine_t *handle)
{
  return co_resuming_set(handle, NULL);
}

void *co_resuming_set(co_routine_t *handle, void *data)
{
  if (co_terminated(handle)) return (void *)-1;
  handle->resume_value = data;
  co_switch(handle);
  return handle->yield_value;
}

value_t co_returning(co_routine_t *co)
{
  return co_value(co->results);
}

bool co_terminated(co_routine_t *co)
{
  return co->halt;
}

void *co_results(co_routine_t *co)
{
  return co->results;
}

int co_array_init(co_array_t *a)
{
  if (UNLIKELY(!a))
      return -EINVAL;

  a->base = NULL;
  a->elements = 0;

  return 0;
}

int co_array_reset(co_array_t *a)
{
  if (UNLIKELY(!a))
      return -EINVAL;

  CO_FREE(a->base);
  a->base = NULL;
  a->elements = 0;

  return 0;
}

#if !defined(HAS_REALLOC_ARRAY)

#if !defined(HAVE_BUILTIN_MUL_OVERFLOW)
/*
 * This is sqrt(SIZE_MAX+1), as s1*s2 <= SIZE_MAX
 * if both s1 < MUL_NO_OVERFLOW and s2 < MUL_NO_OVERFLOW
 */
#define MUL_NO_OVERFLOW ((size_t)1 << (sizeof(size_t) * 4))

static inline bool umull_overflow(size_t a, size_t b, size_t *out)
{
  if ((a >= MUL_NO_OVERFLOW || b >= MUL_NO_OVERFLOW) && a > 0 && SIZE_MAX / a < b)
      return true;
  *out = a * b;
  return false;
}
#else
#define umull_overflow __builtin_mul_overflow
#endif

void *realloc_array(void *optr, size_t nmemb, size_t size)
{
  size_t total_size;
  if (UNLIKELY(umull_overflow(nmemb, size, &total_size)))
  {
      errno = ENOMEM;
      return NULL;
  }
  return realloc(optr, total_size);
}

#endif /* HAS_REALLOC_ARRAY */

#if !defined(HAVE_BUILTIN_ADD_OVERFLOW)
static inline bool add_overflow(size_t a, size_t b, size_t *out)
{
  if (UNLIKELY(a > 0 && b > SIZE_MAX - a))
      return true;

  *out = a + INCREMENT;
  return false;
}
#else
#define add_overflow __builtin_add_overflow
#endif

void *co_array_append(co_array_t *a, size_t element_size)
{
  if (!(a->elements % INCREMENT))
  {
      void *new_base;
      size_t new_cap;

      if (UNLIKELY(add_overflow(a->elements, INCREMENT, &new_cap)))
      {
      errno = EOVERFLOW;
      return NULL;
      }

      new_base = realloc_array(a->base, new_cap, element_size);
      if (UNLIKELY(!new_base))
      return NULL;

      a->base = new_base;
  }

  return ((unsigned char *)a->base) + a->elements++ * element_size;
}

void co_array_sort(co_array_t *a, size_t element_size, int (*cmp)(const void *a, const void *b))
{
  if (LIKELY(a->elements))
      qsort(a->base, a->elements - 1, element_size, cmp);
}

static void co_array_free(void *data)
{
  co_array_t *array = data;

  co_array_reset(array);
  CO_FREE(array);
}

co_array_t *co_array_new(co_routine_t *coro)
{
  co_array_t *array;

  array = co_malloc_full(coro, sizeof(*array), co_array_free);
  if (LIKELY(array))
      co_array_init(array);

  return array;
}

void co_deferred_run(co_routine_t *coro, size_t generation)
{
  co_array_t *array = (co_array_t *)&coro->defer;
  defer_func_t *defers = array->base;
  size_t i;

  for (i = array->elements; i != generation; i--)
  {
      defer_func_t *defer = &defers[i - 1];

      defer->func(defer->data1, defer->data2);
  }

  array->elements = generation;
}

size_t co_deferred_count(const co_routine_t *coro)
{
  const co_array_t *array = (co_array_t *)&coro->defer;

  return array->elements;
}

void co_deferred_free(co_routine_t *coro)
{
  CO_ASSERT(coro);
  co_deferred_run(coro, 0);
  co_deferred_array_reset(&coro->defer);
}

static void co_deferred_any(co_routine_t *coro, defer_func func, void *data1, void *data2)
{
  defer_func_t *defer;

  CO_ASSERT(func);

  defer = co_deferred_array_append(&coro->defer);
  if (UNLIKELY(!defer)) {
      CO_LOG("Could not add new deferred function.");
  } else {
      defer->func = func;
      defer->data1 = data1;
      defer->data2 = data2;
  }
}

void co_defer(defer_func func, void *data)
{
  co_deferred(co_active(), func, data);
}

void co_deferred(co_routine_t *coro, defer_func func, void *data)
{
  co_deferred_any(coro, func, data, NULL);
}

void co_deferred2(co_routine_t *coro, defer_func2 func, void *data1, void *data2)
{
  co_deferred_any(coro, func, data1, data2);
}

void *co_malloc_full(co_routine_t *coro, size_t size, defer_func func)
{
  void *ptr = CO_MALLOC(size);

  if (LIKELY(ptr))
      co_deferred(coro, func, ptr);

  return ptr;
}

void *co_new(size_t size)
{
  return co_malloc_full(co_active(), size, CO_FREE);
}

void *co_malloc(co_routine_t *coro, size_t size)
{
  return co_malloc_full(coro, size, CO_FREE);
}

char *co_strndup(co_routine_t *coro, const char *str, size_t max_len)
{
  const size_t len = strnlen(str, max_len) + 1;
  char *dup = co_memdup(coro, str, len);

  if (LIKELY(dup))
      dup[len - 1] = '\0';

  return dup;
}

char *co_strdup(co_routine_t *coro, const char *str)
{
  return co_memdup(coro, str, strlen(str) + 1);
}

#if defined(_WIN32) || defined(_WIN64)
int vasprintf(char **str_p, const char *fmt, va_list ap)
{
  va_list ap_copy;
  int formattedLength, actualLength;
  size_t requiredSize;

  // be paranoid
  *str_p = NULL;

  // copy va_list, as it is used twice
  va_copy(ap_copy, ap);

  // compute length of formatted string, without NULL terminator
  formattedLength = _vscprintf(fmt, ap_copy);
  va_end(ap_copy);

  // bail out on error
  if (formattedLength < 0) {
      return -1;
  }

  // allocate buffer, with NULL terminator
  requiredSize = ((size_t)formattedLength) + 1;
  *str_p = (char *)CO_MALLOC(requiredSize);

  // bail out on failed memory allocation
  if (*str_p == NULL) {
      errno = ENOMEM;
      return -1;
  }

  // write formatted string to buffer, use security hardened _s function
  actualLength = vsnprintf_s(*str_p, requiredSize, requiredSize - 1, fmt, ap);

  // again, be paranoid
  if (actualLength != formattedLength) {
      CO_FREE(*str_p);
      *str_p = NULL;
      errno = EOTHER;
      return -1;
  }

  return formattedLength;
}

int asprintf(char **str_p, const char *fmt, ...)
{
  int result;

  va_list ap;
  va_start(ap, fmt);
  result = vasprintf(str_p, fmt, ap);
  va_end(ap);

  return result;
}
#endif

char *co_printf(co_routine_t *coro, const char *fmt, ...)
{
  va_list values;
  int len;
  char *tmp_str;

  va_start(values, fmt);
  len = vasprintf(&tmp_str, fmt, values);
  va_end(values);

  if (UNLIKELY(len < 0))
      return NULL;

  co_deferred(coro, CO_FREE, tmp_str);
  return tmp_str;
}

void *co_memdup(co_routine_t *coro, const void *src, size_t len)
{
  void *ptr = co_malloc(coro, len);

  return LIKELY(ptr) ? memcpy(ptr, src, len) : NULL;
}

void co_stack_check(int n)
{
  co_routine_t *t;

  t = co_running;
  if ((char *)&t <= (char *)t->stack_base || (char *)&t - (char *)t->stack_base < 256 + n || t->magic_number != CO_MAGIC_NUMBER)
  {
      CO_INFO("coroutine stack overflow: &t=%p stack=%p n=%d\n", &t, t->stack_base, 256 + n);
      abort();
  }
}

void coroutine_add(co_queue_t *l, co_routine_t *t)
{
  if (l->tail)
  {
      l->tail->next = t;
      t->prev = l->tail;
  }
  else
  {
      l->head = t;
      t->prev = NULL;
  }

  l->tail = t;
  t->next = NULL;
}

void coroutine_remove(co_queue_t *l, co_routine_t *t)
{
  if (t->prev)
      t->prev->next = t->next;
  else
      l->head = t->next;

  if (t->next)
      t->next->prev = t->prev;
  else
      l->tail = t->prev;
}

int coroutine_create(co_callable_t fn, void *arg, unsigned int stack)
{
  int id;
  co_routine_t *t;

  t = co_create(stack, fn, arg);
  t->cid = ++co_id_generate;
  co_count++;
  id = t->cid;
  if (n_all_coroutine % 64 == 0) {
      all_coroutine = realloc(all_coroutine, (n_all_coroutine + 64) * sizeof(all_coroutine[0]));
      if (all_coroutine == NULL) {
        CO_LOG("out of memory");
        abort();
      }
  }

  t->all_coroutine_slot = n_all_coroutine;
  all_coroutine[n_all_coroutine++] = t;
  coroutine_ready(t);

  return id;
}

#if defined(_WIN32) || defined(_WIN64)
int gettimeofday(struct timeval *tp, struct timezone *tzp)
{
  // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
  // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
  // until 00:00:00 January 1, 1970
  static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

  SYSTEMTIME system_time;
  FILETIME file_time;
  uint64_t time;

  GetSystemTime(&system_time);
  SystemTimeToFileTime(&system_time, &file_time);
  time = ((uint64_t)file_time.dwLowDateTime);
  time += ((uint64_t)file_time.dwHighDateTime) << 32;

  tp->tv_sec = (long)((time - EPOCH) / 10000000L);
  tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
  return 0;
}
#endif

static size_t nsec(void)
{
  struct timeval tv;

  if (gettimeofday(&tv, 0) < 0)
      return -1;

  return (size_t)tv.tv_sec * 1000 * 1000 * 1000 + tv.tv_usec * 1000;
}

void coroutine_loop(int mode)
{
  CO_EVENT_LOOP(co_running->loop, mode);
}

void *coroutine_wait(void *v)
{
  int ms;
  co_routine_t *t;
  size_t now;

  coroutine_system();
  coroutine_name("coroutine_wait");
  for (;;)
  {
      /* let everyone else run */
      while (coroutine_yield() > 0);
      /* we're the only one runnable - check for i/o the event loop */
      coroutine_state("event loop");
      if ((t = sleeping.head) == NULL) {
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

      coroutine_loop(2);
      now = nsec();
      while ((t = sleeping.head) && now >= t->alarm_time) {
        coroutine_remove(&sleeping, t);
        if (!t->system && --sleeping_counted == 0)
            co_count--;

        coroutine_ready(t);
      }
  }
}

unsigned int coroutine_delay(unsigned int ms)
{
	size_t when, now;
    co_routine_t *t;

    if (!started_wait) {
      started_wait = 1;
      coroutine_create(coroutine_wait, 0, CO_STACK_SIZE);
    }

    now = nsec();
	when = now + (size_t)ms * 1000000;
	for (t = sleeping.head; t != NULL && t->alarm_time < when; t = t->next);

	if (t) {
		co_running->prev = t->prev;
		co_running->next = t;
	} else {
		co_running->prev = sleeping.tail;
		co_running->next = NULL;
	}

	t = co_running;
	t->alarm_time = when;
	if (t->prev)
		t->prev->next = t;
	else
		sleeping.head = t;

	if (t->next)
		t->next->prev = t;
	else
		sleeping.tail = t;

	if (!t->system && sleeping_counted++ == 0)
		co_count++;

    co_switch(co_running);

    return (unsigned int)(nsec() - now) / 1000000;
}

void coroutine_ready(co_routine_t *t)
{
    t->ready = 1;
    coroutine_add(&co_run_queue, t);
}

int coroutine_yield()
{
    int n;
    n = n_co_switched;
    coroutine_ready(co_running);
    coroutine_state("yield");
    co_suspend();
    return n_co_switched - n - 1;
}

bool coroutine_active()
{
    return co_run_queue.head != NULL;
}

void coroutine_state(char *fmt, ...)
{
    va_list args;
    co_routine_t *t;

    t = co_running;
    va_start(args, fmt);
    vsnprintf(t->state, sizeof t->name, fmt, args);
    va_end(args);
}

char *coroutine_get_state()
{
    return co_running->state;
}

void coroutine_name(char *fmt, ...)
{
    va_list args;
    co_routine_t *t;

    t = co_running;
    va_start(args, fmt);
    vsnprintf(t->name, sizeof t->name, fmt, args);
    va_end(args);
}

char *coroutine_get_name()
{
    return co_running->name;
}

unsigned int coroutine_id()
{
    return co_running->cid;
}

void coroutine_system(void)
{
    if (!co_running->system) {
      co_running->system = 1;
      --co_count;
    }
}

void coroutine_exit(int val)
{
    exiting = val;
    co_running->exiting = true;
    co_deferred_free(co_running);
    co_switch(co_main_handle);
}

void coroutine_info()
{
    int i;
    co_routine_t *t;
    char *extra;

    puts("coroutine list:");
    for (i = 0; i < n_all_coroutine; i++)
    {
      t = all_coroutine[i];
      if (t == co_running)
        extra = " (running)";
      else if (t->ready)
        extra = " (ready)";
      else
        extra = "";

      printf("%6d%c %-20s %s%s%d\n", t->cid, t->system ? 's' : ' ', t->name, t->state, extra, exiting);
    }
}

/*
 * startup
 */
static void coroutine_scheduler(void)
{
    int i;
    co_routine_t *t;

    for (;;)
    {
      if (co_count == 0) {
        CO_LOG("Coroutine scheduler exited");
        exit(exiting);
      }

      t = co_run_queue.head;
      if (t == NULL) {
        CO_INFO("No runnable coroutines! %d stalled\n", co_count);
        exit(1);
      }

      coroutine_remove(&co_run_queue, t);
      t->ready = 0;
      co_running = t;
      n_co_switched++;
      CO_INFO("Running coroutine id: %d (%s)\n", t->cid, t->name);
      co_switch(t);
      CO_LOG("Back at coroutine scheduling");
      co_running = NULL;
      if (t->halt || t->exiting) {
        if (!t->system)
            --co_count;
        i = t->all_coroutine_slot;
        all_coroutine[i] = all_coroutine[--n_all_coroutine];
        all_coroutine[i]->all_coroutine_slot = i;
        co_delete(t);
      }
    }
}

static void *coroutine_main(void *v)
{
    coroutine_name("co_main");
    exiting = co_main(main_argc, main_argv);
    co_active()->exiting = true;
    co_deferred_free(co_active());
    co_switch(co_main_handle);

    return 0;
}

int main(int argc, char **argv)
{
  main_argc = argc;
  main_argv = argv;

  coroutine_create(coroutine_main, NULL, CO_MAIN_STACK);
  coroutine_scheduler();
  CO_LOG("Coroutine scheduler returned to main, when it shouldn't have!");
  abort();
  return exiting;
}
