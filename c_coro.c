#include "c_coro.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(__clang__)
  #pragma clang diagnostic ignored "-Wparentheses"

  /* placing code in section(text) does not mark it executable with Clang. */
  #undef  CO_MPROTECT
  #define CO_MPROTECT
#endif

#if (defined(__clang__) || defined(__GNUC__)) && defined(__i386__)
  #define fastcall __attribute__((fastcall))
#elif defined(_MSC_VER) && defined(_M_IX86)
  #define fastcall __fastcall
#else
  #define fastcall
#endif

static thread_local co_routine_t co_active_buffer[64];
/* Variable holding the current running coroutine per thread. */
static thread_local co_routine_t *co_active_handle = NULL;
/* Variable holding the main target that gets called once an coroutine
function fully completes and return. */
static thread_local co_routine_t *co_main_handle = NULL;
/* Variable holding the previous running coroutine per thread. */
static thread_local co_routine_t *co_current_handle = NULL;
static void(fastcall *co_swap)(co_routine_t *, co_routine_t *) = 0;

/* called only if co_routine_t func returns */
static void co_done() {
    co_active()->halt = 1;
    co_active()->state = CO_DEAD;
    co_switch(co_main_handle);
}

static void co_awaitable()
{
    co_routine_t *co = co_active();
    co->results =  co->func(co->args);
    co->state = CO_NORMAL;
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

            size_t context_addr = _co_align_forward((size_t)handle + sizeof(co_routine_t), 16);
            size_t storage_addr = _co_align_forward(context_addr, 16);
            handle->storage_size = CO_DEFAULT_STORAGE_SIZE;

            /* Initialize storage. */
            unsigned char *storage = (unsigned char *)storage_addr;
            memset(storage, 0, handle->storage_size);
            handle->bytes_stored = 0;

            handle->func = func;
            handle->state = CO_SUSPENDED;
            handle->stack_size = size;
            handle->storage = storage;
            handle->halt = 0;
            handle->args = args;
            co->magic_number = CO_MAGIC_NUMBER;
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

            size_t context_addr = _co_align_forward((size_t)handle + sizeof(co_routine_t), 16);
            handle->storage_size = CO_DEFAULT_STORAGE_SIZE;
            size_t storage_addr = _co_align_forward(context_addr, 16);

            /* Initialize storage. */
            unsigned char *storage = (unsigned char *)storage_addr;
            memset(storage, 0, handle->storage_size);
            handle->bytes_stored = 0;

            handle->func = func;
            handle->state = CO_SUSPENDED;
            handle->stack_size = size;
            handle->storage = storage;
            handle->halt = 0;
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
            size_t context_addr = _co_align_forward((size_t)co + sizeof(co_routine_t), 16);
            size_t storage_addr = _co_align_forward(context_addr, 16);
            co->storage_size = CO_DEFAULT_STORAGE_SIZE;

            /* Initialize storage. */
            unsigned char *storage = (unsigned char *)storage_addr;
            memset(storage, 0, co->storage_size);
            co->bytes_stored = 0;

            co->func = func;
            co->state = CO_SUSPENDED;
            co->stack_size = size;
            co->storage = storage;
            co->halt = 0;
            co->args = args;
            co->magic_number = CO_MAGIC_NUMBER;
        }

        return co;
    }
  #elif defined(__aarch64__)
    #include <stdint.h>
    static const uint32_t co_swap_function[1024] = {
        0x910003f0,  /* mov x16,sp           */
        0xa9007830,  /* stp x16,x30,[x1]     */
        0xa9407810,  /* ldp x16,x30,[x0]     */
        0x9100021f,  /* mov sp,x16           */
        0xa9015033,  /* stp x19,x20,[x1, 16] */
        0xa9415013,  /* ldp x19,x20,[x0, 16] */
        0xa9025835,  /* stp x21,x22,[x1, 32] */
        0xa9425815,  /* ldp x21,x22,[x0, 32] */
        0xa9036037,  /* stp x23,x24,[x1, 48] */
        0xa9436017,  /* ldp x23,x24,[x0, 48] */
        0xa9046839,  /* stp x25,x26,[x1, 64] */
        0xa9446819,  /* ldp x25,x26,[x0, 64] */
        0xa905703b,  /* stp x27,x28,[x1, 80] */
        0xa945701b,  /* ldp x27,x28,[x0, 80] */
        0xf900303d,  /* str x29,    [x1, 96] */
        0xf940301d,  /* ldr x29,    [x0, 96] */
        0x6d072428,  /* stp d8, d9, [x1,112] */
        0x6d472408,  /* ldp d8, d9, [x0,112] */
        0x6d082c2a,  /* stp d10,d11,[x1,128] */
        0x6d482c0a,  /* ldp d10,d11,[x0,128] */
        0x6d09342c,  /* stp d12,d13,[x1,144] */
        0x6d49340c,  /* ldp d12,d13,[x0,144] */
        0x6d0a3c2e,  /* stp d14,d15,[x1,160] */
        0x6d4a3c0e,  /* ldp d14,d15,[x0,160] */
        #if defined(_WIN32) && !defined(CO_NO_TIB)
        0xa940c650,  /* ldp x16,x17,[x18, 8] */
        0xa90b4430,  /* stp x16,x17,[x1,176] */
        0xa94b4410,  /* ldp x16,x17,[x0,176] */
        0xa900c650,  /* stp x16,x17,[x18, 8] */
        #endif
        0xd61f03c0,  /* br x30               */
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
            size_t context_addr = _co_align_forward((size_t)co + sizeof(co_routine_t), 16);
            size_t storage_addr = _co_align_forward(context_addr, 16);
            co->storage_size = CO_DEFAULT_STORAGE_SIZE;

            /* Initialize storage. */
            unsigned char *storage = (unsigned char *)storage_addr;
            memset(storage, 0, co->storage_size);
            co->bytes_stored = 0;

            co->func = func;
            co->state = CO_SUSPENDED;
            co->stack_size = size;
            co->storage = storage;
            co->halt = 0;
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
        handle->state = CO_DEAD;
    }

    void co_switch(co_routine_t *thread)
    {
        co_active_ = thread;
        SwitchToFiber((LPVOID)thread);
    }

    unsigned char co_serializable(void)
    {
        return 0;
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
            /* Stack size should be at least `CO_MIN_STACK_SIZE`. */
            if (size < CO_MIN_STACK_SIZE) {
                size = CO_MIN_STACK_SIZE;
            }
        } else {
            size = CO_DEFAULT_STACK_SIZE;
        }

        void *memory = CO_MALLOC(size);
        if (!memory)
            return (co_routine_t *)0;
        return co_derive(memory, size, func, args);
    }

    void co_delete(co_routine_t *handle)
    {
        if (!handle) {
            CO_LOG("attempt to delete an invalid coroutine");
        } else if (!(handle->state == CO_NORMAL || handle->state == CO_DEAD)) {
            CO_LOG("attempt to delete a coroutine that is not dead or suspended");
        } else {
            CO_FREE(handle);
            handle->state = CO_DEAD;
        }
    }

    void co_switch(co_routine_t *handle)
    {
        co_routine_t *co_previous_handle = co_active_handle;
        co_active_handle = handle;
        co_active_handle->state = CO_RUNNING;
        co_current_handle->state = CO_NORMAL;
        co_current_handle = co_previous_handle;
        co_swap(co_active_handle, co_previous_handle);
    }

    unsigned char co_serializable(void)
    {
        return 1;
    }
#endif

co_result co_push(co_routine_t *co, const void *src, size_t len) {
  if(!co) {
    CO_LOG("attempt to use an invalid coroutine");
    return CO_INVALID_COROUTINE;
  } else if(len > 0) {
    size_t bytes_stored = co->bytes_stored + len;
    if(bytes_stored > co->storage_size) {
      CO_LOG("attempt to push too many bytes into coroutine storage");
      return CO_NOT_ENOUGH_SPACE;
    }
    if(!src) {
      CO_LOG("attempt push a null pointer into coroutine storage");
      return CO_INVALID_POINTER;
    }
    memcpy(&co->storage[co->bytes_stored], src, len);
    co->bytes_stored = bytes_stored;
  }
  return CO_SUCCESS;
}

co_result co_pop(co_routine_t *co, void *dest, size_t len) {
  if(!co) {
    CO_LOG("attempt to use an invalid coroutine");
    return CO_INVALID_COROUTINE;
  } else if(len > 0) {
    if(len > co->bytes_stored) {
      CO_LOG("attempt to pop too many bytes from coroutine storage");
      return CO_NOT_ENOUGH_SPACE;
    }
    size_t bytes_stored = co->bytes_stored - len;
    if(dest) {
      memcpy(dest, &co->storage[bytes_stored], len);
    }
    co->bytes_stored = bytes_stored;
#ifdef CO_ZERO_MEMORY
    /* Clear garbage in the discarded storage. */
    memset(&co->storage[bytes_stored], 0, len);
#endif
  }

  return CO_SUCCESS;
}

co_result co_peek(co_routine_t *co, void *dest, size_t len) {
  if(!co) {
    CO_LOG("attempt to use an invalid coroutine");
    return CO_INVALID_COROUTINE;
  } else if(len > 0) {
    if(len > co->bytes_stored) {
      CO_LOG("attempt to peek too many bytes from coroutine storage");
      return CO_NOT_ENOUGH_SPACE;
    }
    if(!dest) {
      CO_LOG("attempt peek into a null pointer");
      return CO_INVALID_POINTER;
    }
    memcpy(dest, &co->storage[co->bytes_stored - len], len);
  }
  return CO_SUCCESS;
}

size_t co_get_bytes_stored(co_routine_t *co) {
  if(co == NULL) {
    return 0;
  }
  return co->bytes_stored;
}

size_t co_get_storage_size(co_routine_t *co) {
  if(co == NULL) {
    return 0;
  }
  return co->storage_size;
}

void *co_get_user_data(co_routine_t *co)
{
  if(co != NULL) {
    return co->user_data;
  }
  return NULL;
}

co_state co_status(co_routine_t *co)
{
  if(co != NULL) return co->state;

  return CO_DEAD;
}

co_routine_t *co_running(void)
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
  size_t stack_size = CO_DEFAULT_STACK_SIZE;
  stack_size = _co_align_forward(stack_size, 16); /* Stack size should be aligned to 16 bytes. */
  void *memory = CO_MALLOC(stack_size);
  if (!memory) return (co_routine_t *)0;

  return co_derive(memory, stack_size, func, args);
}

void co_suspend()
{
  co_switch(co_running());
}

void co_yielding(co_routine_t *handle, void *data)
{
  if (handle->magic_number != CO_MAGIC_NUMBER) {
    /* Stack overflow. */
    CO_LOG("coroutine stack overflow, try increasing the stack size");
  } else {
    co_push(handle, &data, sizeof(data));
    co_switch(handle);
  }
}

void *co_resuming(co_routine_t *handle)
{
  if (co_terminated(handle)) return (void *)-1;
  co_switch(handle);
  co_pop(handle, &handle->yield_value, sizeof(handle->yield_value));
  return handle->yield_value;
}

value_t co_returning(co_routine_t *co)
{
  return co_value(co->results);
}

unsigned char co_terminated(co_routine_t *co)
{
  return co->halt;
}

void *co_results(co_routine_t *co)
{
  return co->results;
}

const char* co_result_description(co_result res)
{
  switch(res) {
    case CO_SUCCESS:
      return "No error";
    case CO_GENERIC_ERROR:
      return "Generic error";
    case CO_INVALID_POINTER:
      return "Invalid pointer";
    case CO_INVALID_COROUTINE:
      return "Invalid coroutine";
    case CO_NOT_SUSPENDED:
      return "Coroutine not suspended";
    case CO_NOT_RUNNING:
      return "Coroutine not running";
    case CO_MAKE_CONTEXT_ERROR:
      return "Make context error";
    case CO_SWITCH_CONTEXT_ERROR:
      return "Switch context error";
    case CO_NOT_ENOUGH_SPACE:
      return "Not enough space";
    case CO_OUT_OF_MEMORY:
      return "Out of memory";
    case CO_INVALID_ARGUMENTS:
      return "Invalid arguments";
    case CO_INVALID_OPERATION:
      return "Invalid operation";
    case CO_STACK_OVERFLOW:
      return "Stack overflow";
  }
  return "Unknown error";
}

#ifdef __cplusplus
}
#endif
