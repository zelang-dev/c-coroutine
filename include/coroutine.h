#ifndef C_COROUTINE_H
#define C_COROUTINE_H

#ifndef CERTIFICATE
    #define CERTIFICATE "localhost"
#endif

#include "uv_routine.h"
#include "raii.h"

#if defined(_MSC_VER)
    #define CO_MPROTECT 1
    #define S_IRUSR S_IREAD  /* read, user */
    #define S_IWUSR S_IWRITE /* write, user */
    #define S_IXUSR 0 /* execute, user */
    #define S_IRGRP 0 /* read, group */
    #define S_IWGRP 0 /* write, group */
    #define S_IXGRP 0 /* execute, group */
    #define S_IROTH 0 /* read, others */
    #define S_IWOTH 0 /* write, others */
    #define S_IXOTH 0 /* execute, others */
    #define S_IRWXU 0
    #define S_IRWXG 0
    #define S_IRWXO 0
    #if !defined(__cplusplus)
        #define __STDC__ 1
    #endif
#endif

#define CO_LOG(s) RAII_LOG(s)
#define CO_INFO(s, ...) RAII_INFO(s, __VA_ARGS__ )
#define CO_HERE() RAII_HERE()
#define CO_ASSERT RAII_ASSERT

/* Stack size when creating a coroutine. */
#ifndef CO_STACK_SIZE
    #define CO_STACK_SIZE (32 * 1024)
#endif

#ifndef CO_MAIN_STACK
    #define CO_MAIN_STACK (64 * 1024)
#endif

#define ERROR_SCRAPE_SIZE 256

#ifndef CO_SCRAPE_SIZE
    #define CO_SCRAPE_SIZE (2 * 64)
#endif

#if defined(UV_H)
    #define CO_EVENT_LOOP(h, m) uv_run(h, m)
#elif !defined(CO_EVENT_LOOP)
    #define CO_EVENT_LOOP(h, m)
#endif

/* Public API qualifier. */
#ifndef C_API
    #define C_API extern
#endif

/*[amd64, arm, ppc, x86]:
   by default, co_swap_function is marked as a text (code) section
   if not supported, uncomment the below line to use mprotect instead */
/* #define CO_MPROTECT */

/*[amd64]:
   Win64 only: provides a substantial speed-up, but will thrash XMM regs
   do not use this unless you are certain your application won't use SSE */
/* #define CO_NO_SSE */

/*[amd64, aarch64]:
   Win64 only: provides a small speed-up, but will break stack unwinding
   do not use this if your application uses exceptions or setjmp/longjmp */
/* #define CO_NO_TIB */

#if !defined(thread_local) /* User can override thread_local for obscure compilers */
     /* Running in multi-threaded environment */
    #if defined(__STDC__) /* Compiling as C Language */
      #if defined(_MSC_VER) /* Don't rely on MSVC's C11 support */
        #define thread_local __declspec(thread)
      #elif __STDC_VERSION__ < 201112L /* If we are on C90/99 */
        #if defined(__clang__) || defined(__GNUC__) /* Clang and GCC */
          #define thread_local __thread
        #else /* Otherwise, we ignore the directive (unless user provides their own) */
          #define thread_local
        #endif
      #elif __APPLE__ && __MACH__
        #define thread_local __thread
      #else /* C11 and newer define thread_local in threads.h */
        #include <threads.h>
      #endif
    #elif defined(__cplusplus) /* Compiling as C++ Language */
      #if __cplusplus < 201103L /* thread_local is a C++11 feature */
        #if defined(_MSC_VER)
          #define thread_local __declspec(thread)
        #elif defined(__clang__) || defined(__GNUC__)
          #define thread_local __thread
        #else /* Otherwise, we ignore the directive (unless user provides their own) */
          #define thread_local
        #endif
      #else /* In C++ >= 11, thread_local in a builtin keyword */
        /* Don't do anything */
      #endif
    #endif
#endif

#ifndef CO_FORCE_INLINE
    #define CO_FORCE_INLINE RAII_INLINE
#endif

#ifndef CO_NO_INLINE
    #define CO_NO_INLINE RAII_NO_INLINE
#endif

/* In alignas(a), 'a' should be a power of two that is at least the type's
   alignment and at most the implementation's alignment limit.  This limit is
   2**13 on MSVC. To be portable to MSVC through at least version 10.0,
   'a' should be an integer constant, as MSVC does not support expressions
   such as 1 << 3.

   The following C11 requirements are NOT supported on MSVC:

   - If 'a' is zero, alignas has no effect.
   - alignas can be used multiple times; the strictest one wins.
   - alignas (TYPE) is equivalent to alignas (alignof (TYPE)).
*/
#if !defined(alignas)
  #if defined(__STDC__) /* C Language */
    #if defined(_MSC_VER) /* Don't rely on MSVC's C11 support */
      #define alignas(bytes) __declspec(align(bytes))
    #elif __STDC_VERSION__ >= 201112L /* C11 and above */
      #include <stdalign.h>
    #elif defined(__clang__) || defined(__GNUC__) /* C90/99 on Clang/GCC */
      #define alignas(bytes) __attribute__ ((aligned (bytes)))
    #else /* Otherwise, we ignore the directive (user should provide their own) */
      #define alignas(bytes)
    #endif
  #elif defined(__cplusplus) /* C++ Language */
    #if __cplusplus < 201103L
      #if defined(_MSC_VER)
        #define alignas(bytes) __declspec(align(bytes))
      #elif defined(__clang__) || defined(__GNUC__) /* C++98/03 on Clang/GCC */
        #define alignas(bytes) __attribute__ ((aligned (bytes)))
      #else /* Otherwise, we ignore the directive (unless user provides their own) */
        #define alignas(bytes)
      #endif
    #else /* C++ >= 11 has alignas keyword */
      /* Do nothing */
    #endif
  #endif /* = !defined(__STDC_VERSION__) && !defined(__cplusplus) */
#endif

#if !defined(CO_MALLOC) || !defined(CO_FREE) || !defined(CO_REALLOC)|| !defined(CO_CALLOC)
    #define CO_MALLOC RAII_MALLOC
    #define CO_FREE RAII_FREE
    #define CO_REALLOC RAII_REALLOC
    #define CO_CALLOC RAII_CALLOC
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

#ifdef USE_VALGRIND
#include <valgrind/valgrind.h>
#endif

#ifndef MAX
# define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
# define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/* Function casting for single argument, no return. */
#define FUNC_VOID(fn)		((void (*)(void_t))(fn))
#define UNUSED(x) ((void)(x))

#if defined(_MSC_VER)
  #define section(name) __declspec(allocate("." #name))
#elif defined(__APPLE__)
  #define section(name) __attribute__((section("__TEXT,__" #name)))
#else
  #define section(name) __attribute__((section("." #name "#")))
#endif

/* Number used only to assist checking for stack overflows. */
#define CO_MAGIC_NUMBER 0x7E3CB1A9

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    CO_NULL = RAII_NULL,
    CO_INT,
    CO_ENUM,
    CO_INTEGER,
    CO_UINT,
    CO_SLONG,
    CO_ULONG,
    CO_LLONG,
    CO_MAXSIZE,
    CO_FLOAT,
    CO_DOUBLE,
    CO_BOOL,
    CO_SHORT,
    CO_USHORT,
    CO_CHAR,
    CO_UCHAR,
    CO_UCHAR_P,
    CO_CHAR_P,
    CO_CONST_CHAR,
    CO_STRING,
    CO_ARRAY,
    CO_HASH,
    CO_OBJ,
    CO_PTR,
    CO_FUNC,
    CO_NONE,
    CO_DEF_ARR,
    CO_DEF_FUNC,
    CO_ROUTINE,
    CO_OA_HASH,
    CO_REFLECT_TYPE,
    CO_REFLECT_INFO,
    CO_REFLECT_VALUE,
    CO_MAP_VALUE,
    CO_MAP_STRUCT,
    CO_MAP_ITER,
    CO_MAP_ARR,
    CO_ERR_PTR,
    CO_ERR_CONTEXT,
    CO_PROMISE,
    CO_FUTURE,
    CO_FUTURE_ARG,
    CO_EVENT_ARG,
    CO_ARGS,
    CO_PROCESS,
    CO_SCHED,
    CO_CHANNEL,
    CO_STRUCT,
    CO_UNION,
    CO_VALUE,
    CO_NO_INSTANCE
} value_types;

typedef void *void_t;
typedef const void *const_t;
typedef char *string;
typedef const char *string_t;
typedef unsigned char *u_string;
typedef const unsigned char *u_string_t;
typedef const unsigned char u_char_t;

typedef void_t(*callable_t)(void_t);
typedef int (*thrd_func_t)(void *);
typedef void (*any_func_t)(void_t, ...);

/* Coroutine states. */
typedef enum co_state
{
    CO_EVENT_DEAD = -1, /* The coroutine has ended it's Event Loop routine, is uninitialized or deleted. */
    CO_DEAD, /* The coroutine is uninitialized or deleted. */
    CO_NORMAL,   /* The coroutine is active but not running (that is, it has switch to another coroutine, suspended). */
    CO_RUNNING,  /* The coroutine is active and running. */
    CO_SUSPENDED, /* The coroutine is suspended (in a startup, or it has not started running yet). */
    CO_EVENT, /* The coroutine is in an Event Loop callback. */
    CO_ERRED, /* The coroutine has erred. */
} co_state;

typedef struct routine_s routine_t;
typedef struct oa_hash_s hash_t;
typedef struct json_value_t json_t;
typedef hash_t ht_string_t;
typedef hash_t wait_group_t;
typedef hash_t wait_result_t;
typedef hash_t gc_channel_t;
typedef hash_t gc_coroutine_t;

#if ((defined(__clang__) || defined(__GNUC__)) && defined(__i386__)) || (defined(_MSC_VER) && defined(_M_IX86))
    #define USE_NATIVE 1
#elif ((defined(__clang__) || defined(__GNUC__)) && defined(__amd64__)) || (defined(_MSC_VER) && defined(_M_AMD64))
    #define USE_NATIVE 1
#elif defined(__clang__) || defined(__GNUC__)
  #if defined(__arm__)
    #define USE_NATIVE 1
  #elif defined(__aarch64__)
    #define USE_NATIVE 1
  #elif defined(__powerpc64__) && defined(_CALL_ELF) && _CALL_ELF == 2
    #define USE_NATIVE 1
  #else
    #define USE_UCONTEXT 1
  #endif
#else
    #define USE_UCONTEXT 1
#endif

#if defined(USE_UCONTEXT)
    #define _BSD_SOURCE
    #if __APPLE__ && __MACH__
        #include <sys/ucontext.h>
    #elif defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
        #include <windows.h>
        #if defined(_X86_)
            #define DUMMYARGS
        #else
            #define DUMMYARGS long dummy0, long dummy1, long dummy2, long dummy3,
        #endif

        typedef struct __stack {
            void_t ss_sp;
            size_t ss_size;
            int ss_flags;
        } stack_t;

        typedef CONTEXT mcontext_t;
        typedef unsigned long __sigset_t;

        typedef struct __ucontext {
            unsigned long int	uc_flags;
            struct __ucontext	*uc_link;
            stack_t				uc_stack;
            mcontext_t			uc_mcontext;
            __sigset_t			uc_sigmask;
        } ucontext_t;

        C_API int getcontext(ucontext_t *ucp);
        C_API int setcontext(const ucontext_t *ucp);
        C_API int makecontext(ucontext_t *, void (*)(), int, ...);
        C_API int swapcontext(routine_t *, const routine_t *);
    #else
        #include <ucontext.h>
    #endif
#endif

/* Coroutine context structure. */
struct routine_s {
#if defined(_WIN32) && defined(_M_IX86)
    void_t rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15;
    void_t xmm[ 20 ]; /* xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15 */
    void_t fiber_storage;
    void_t dealloc_stack;
#elif defined(__x86_64__) || defined(_M_X64)
#ifdef _WIN32
    void_t rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15, *rdi, *rsi;
    void_t xmm[ 20 ]; /* xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15 */
    void_t fiber_storage;
    void_t dealloc_stack;
#else
    void_t rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15;
#endif
#elif defined(__i386) || defined(__i386__)
    void_t eip, *esp, *ebp, *ebx, *esi, *edi;
#elif defined(__ARM_EABI__)
#ifndef __SOFTFP__
    void_t f[ 16 ];
#endif
    void_t d[ 4 ]; /* d8-d15 */
    void_t r[ 4 ]; /* r4-r11 */
    void_t lr;
    void_t sp;
#elif defined(__aarch64__)
    void_t x[ 12 ]; /* x19-x30 */
    void_t sp;
    void_t lr;
    void_t d[ 8 ]; /* d8-d15 */
#elif defined(__powerpc64__) && defined(_CALL_ELF) && _CALL_ELF == 2
    uint64_t gprs[ 32 ];
    uint64_t lr;
    uint64_t ccr;

    /* FPRs */
    uint64_t fprs[ 32 ];

#ifdef __ALTIVEC__
    /* Altivec (VMX) */
    uint64_t vmx[ 12 * 2 ];
    uint32_t vrsave;
#endif
#else
    unsigned long int uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    __sigset_t uc_sigmask;
#endif
    /* Stack base address, can be used to scan memory in a garbage collector. */
    void_t stack_base;
#if defined(_WIN32) && (defined(_M_X64) || defined(_M_IX86))
    void_t stack_limit;
#endif
    /* Coroutine stack size. */
    size_t stack_size;
    co_state status;
    callable_t func;
    char name[ 64 ];
    char state[ 64 ];
    char scrape[ CO_SCRAPE_SIZE ];
    /* unique coroutine id */
    int cid;
    size_t alarm_time;
    size_t cycles;
    routine_t *next;
    routine_t *prev;
    bool channeled;
    bool ready;
    bool system;
    bool exiting;
    bool halt;
    bool synced;
    bool wait_active;
    int wait_counter;
    hash_t *wait_group;
    hash_t *event_group;
    int all_coroutine_slot;
    routine_t *context;
    bool loop_active;
    bool event_active;
    bool process_active;
    bool loop_erred;
    bool is_address;
    bool is_plain;
    signed int loop_code;
    void_t user_data;
#if defined(USE_VALGRIND)
    unsigned int vg_stack_id;
#endif
    void_t args;
    /* Coroutine result of function return/exit. */
    void_t results;
    /* Coroutine plain result of function return/exit. */
    size_t plain_results;
    memory_t scope[1];
    /* Used to check stack overflow. */
    size_t magic_number;
};

/* scheduler queue struct */
typedef struct co_scheduler_s
{
    value_types type;
    routine_t *head;
    routine_t *tail;
} co_scheduler_t;

/* Generic simple union storage types. */
typedef union
{
    int integer;
    unsigned int u_int;
    signed long s_long;
    unsigned long u_long;
    long long long_long;
    size_t max_size;
    float point;
    double precision;
    bool boolean;
    signed short s_short;
    unsigned short u_short;
    signed char schar;
    unsigned char uchar;
    unsigned char *uchar_ptr;
    char *char_ptr;
    char **array;
    void_t object;
    callable_t func;
    const char const_char[512];
} value_t;

typedef struct values_s
{
    value_t value;
    value_types type;
} values_t;

typedef struct
{
    value_types type;
    value_t value;
} generics_t;

typedef enum {
    ZE_OK = CO_NONE,
    ZE_ERR = CO_NULL,
} result_type;

typedef struct {
    result_type type;
    value_t *value;
} result_t;

typedef struct uv_args_s
{
    value_types type;
    /* allocated array of arguments */
    values_t *args;
    routine_t *context;
    string buffer;
    uv_buf_t bufs;
    uv_stat_t stat[1];
    uv_statfs_t statfs[1];
    evt_ctx_t ctx;

    struct sockaddr_in6 in6[1];
    struct sockaddr_in in4[1];

    bool is_path;
    bool is_request;
    uv_fs_type fs_type;
    uv_req_type req_type;
    uv_handle_type handle_type;
    uv_dirent_type_t dirent_type;
    uv_tty_mode_t tty_mode;
    uv_stdio_flags stdio_flag;
    uv_errno_t errno_code;
    int bind_type;

    /* total number of args in set */
    size_t n_args;
} uv_args_t;

/*
 * channel communication
 */
struct channel_co_s;
typedef struct channel_co_s channel_co_t;
typedef struct msg_queue_s
{
    channel_co_t **a;
    unsigned int n;
    unsigned int m;
} msg_queue_t;

typedef struct channel_s
{
    value_types type;
    unsigned int bufsize;
    unsigned int elem_size;
    unsigned char *buf;
    unsigned int nbuf;
    unsigned int off;
    unsigned int id;
    values_t *tmp;
    msg_queue_t a_send;
    msg_queue_t a_recv;
    char *name;
    bool select_ready;
} channel_t;

enum
{
    CHANNEL_END,
    CHANNEL_SEND,
    CHANNEL_RECV,
    CHANNEL_OP,
    CHANNEL_BLK,
};

struct channel_co_s
{
    channel_t *c;
    void_t v;
    unsigned int op;
    routine_t *co;
    channel_co_t *x_msg;
};

C_API uv_loop_t *co_loop(void);

/**
* Returns generic union `values_type` of argument, will auto `release/free`
* allocated memory when current coroutine return/exit.
*
* Must be called at least once to release `allocated` memory.
*
* @param params arbitrary arguments
* @param item index number
*/
C_API values_type args_get(void_t params, int item);
/**
* Creates an container for arbitrary arguments passing to an `coroutine` or `thread`.
* Use `args_get()` or `args_in()` for retrieval.
*
* @param desc format, similar to `printf()`:
* * `i` unsigned integer,
* * `d` signed integer,
* * `c` character,
* * `s` string,
* * `a` array,
* * `x` function,
* * `f` double/float,
* * `p` void pointer for any arbitrary object
* @param arguments indexed by `desc` format order
*/
#define args_for(desc, ...) raii_args_for(co_scope(), desc, __VA_ARGS__)

/* Return handle to current coroutine. */
C_API routine_t *co_active(void);

C_API memory_t *co_scope(void);

/* Delete specified coroutine. */
C_API void co_delete(routine_t *);

/* Switch to specified coroutine. */
C_API void co_switch(routine_t *);

/* Check for coroutine completetion and return. */
C_API bool co_terminated(routine_t *);

/* Return handle to previous coroutine. */
C_API routine_t *co_current(void);

/* Return coroutine executing for scheduler */
C_API routine_t *co_coroutine(void);

/* Return the value in union storage type. */
C_API value_t co_value(void_t);

C_API values_t *co_var(var_t *);

/* Return the value in union storage type. */
C_API value_t co_data(values_t *);

/* Suspends the execution of current coroutine. */
C_API void co_suspend(void);

C_API void co_scheduler(void);

/* Yield to specified coroutine, passing data. */
C_API void co_yielding(routine_t *);

C_API void co_resuming(routine_t *);

/* Returns the status of the coroutine. */
C_API co_state co_status(routine_t *);

/* Get coroutine user data. */
C_API void_t co_user_data(routine_t *);

C_API void co_deferred_free(routine_t *);

/* Defer execution `LIFO` of given function with argument,
to when current coroutine exits/returns. */
C_API size_t co_defer(func_t, void_t);
C_API void co_defer_cancel(size_t index);
C_API void co_defer_fire(size_t index);
C_API size_t co_deferred(routine_t *, func_t, void_t);
C_API size_t co_deferred_count(routine_t *);

/* Same as `defer` but allows recover from an Error condition throw/panic,
you must call `co_catch` inside function to mark Error condition handled. */
C_API void co_recover(func_t, void_t);
/* Compare `err` to current error condition of coroutine, will mark exception handled, if `true`. */
C_API bool co_catch(string_t err);
/* Get current error condition string. */
C_API string_t co_message(void);

/* Call `CO_CALLOC` to allocate memory array of given count and size in current coroutine,
will auto free `LIFO` on function exit/return, do not free! */
C_API void_t co_new_by(int, size_t);
C_API void_t co_calloc_full(routine_t *, int, size_t, func_t);

/* Call `CO_MALLOC` to allocate memory of given size in current coroutine,
will auto free `LIFO` on function exit/return, do not free! */
C_API void_t co_new(size_t);
C_API void_t co_malloc(routine_t *, size_t);
C_API void_t co_malloc_full(routine_t *, size_t, func_t);
C_API void_t co_memdup(routine_t *, const_t, size_t);

C_API string co_strdup(string_t);
C_API string co_strndup(string_t, size_t);
C_API string co_string(string_t str, size_t length);
C_API string *co_str_split(string_t s, string_t delim, int *count);
C_API string co_concat_by(int num_args, ...);
C_API ht_string_t *co_parse_str(string lines, string sep);

/*
Returns information about a certain file string path

Modifed C code from PHP userland function
see https://www.php.net/manual/en/function.pathinfo.php */
C_API fileinfo_t *pathinfo(string filepath);
C_API const_t str_memrchr(const_t s, int c, size_t n);
C_API string *str_split(string_t s, string_t delim, int *count);
C_API string str_concat_by(int num_args, ...);
C_API string str_replace(string_t haystack, string_t needle, string_t replace);
C_API string str_toupper(string s, size_t len);
C_API string str_tolower(string s, size_t len);
C_API string word_toupper(string str, char sep);
C_API string ltrim(string s);
C_API string rtrim(string s);
C_API string trim(string s);

C_API u_string base64_encode(u_string_t src);
C_API u_string base64_decode(u_string_t src);


/* Creates an unbuffered channel, similar to golang channels. */
C_API channel_t *channel(void);

/* Creates an buffered channel of given element count,
similar to golang channels. */
C_API channel_t *channel_buf(int);

/* Send data to the channel. */
C_API int co_send(channel_t *, void_t);

/* Receive data from the channel. */
C_API value_t co_recv(channel_t *);

/* Creates an coroutine of given function with argument,
and add to schedular, same behavior as Go in golang. */
C_API int co_go(callable_t, void_t);

/* Creates an coroutine of given function with argument, and immediately execute. */
C_API void co_execute(func_t, void_t);

C_API uv_loop_t *co_loop(void);

C_API value_t co_event(callable_t, void_t arg);

C_API value_t co_await(callable_t fn, void_t arg);

C_API void co_handler(func_t fn, void_t handle, func_t dtor);
C_API void co_process(func_t fn, void_t args);

/* Explicitly give up the CPU for at least ms milliseconds.
Other tasks continue to run during this time. */
C_API unsigned int co_sleep(unsigned int ms);

/* Return the unique id for the current coroutine. */
C_API unsigned int co_id(void);
C_API signed int co_err_code(void);

/* Yield execution to another coroutine and reschedule current. */
C_API void co_yield(void);

/* Returns the current coroutine's name. */
C_API string co_get_name(void);

/* Returns library version and OS system info from `uv_os_uname()`. */
C_API string co_system_uname(void);

/* The current coroutine will be scheduled again once all the
other currently-ready coroutines have a chance to run. Returns
the number of other tasks that ran while the current task was waiting. */
C_API int coroutine_yield(void);


/* Exit the current coroutine. If this is the last non-system coroutine,
exit the entire program using the given exit status. */
C_API void coroutine_exit(int);
C_API void coroutine_cleanup(void);
C_API void coroutine_update(routine_t *);

C_API void coroutine_schedule(routine_t *);
C_API bool coroutine_active(void);
C_API void coroutine_info(void);

C_API void channel_print(channel_t *);
C_API channel_t *channel_create(int, int);
C_API void channel_free(channel_t *);

#if defined(_WIN32) || defined(_WIN64)
C_API struct tm *gmtime_r(const time_t *timer, struct tm *buf);
#define gcvt _gcvt
#endif

#define HASH_LOAD_FACTOR (0.75)
#define HASH_GROWTH_FACTOR (1<<2)
#ifndef HASH_INIT_CAPACITY
    #define HASH_INIT_CAPACITY (1<<9)
#endif

typedef void_t(*hash_iter_func)(void_t instance, string_t key, const_t value);
typedef struct oa_key_ops_s {
    uint32_t (*hash)(const_t data, void_t arg);
    void* (*cp)(const_t data, void_t arg);
    void (*free)(void_t data, void_t arg);
    bool (*eq)(const_t data1, const_t data2, void_t arg);
    void_t arg;
} oa_key_ops;

typedef struct oa_val_ops_s {
    void* (*cp)(const_t data, void_t arg);
    void (*free)(void_t data);
    bool (*eq)(const_t data1, const_t data2, void_t arg);
    void_t arg;
} oa_val_ops;

typedef struct oa_pair_s {
    uint32_t hash;
    void_t key;
    void_t value;
} oa_pair;

typedef struct oa_hash_s oa_hash;
struct oa_hash_s {
    value_types type;
    int capacity;
    size_t size;
    oa_pair **buckets;
    void (*probing_fct)(struct oa_hash_s *htable, size_t *from_idx);
    oa_key_ops key_ops;
    oa_val_ops val_ops;
};

C_API void hash_free(hash_t *);
C_API void_t hash_put(hash_t *, const_t, const_t);
C_API void_t hash_replace(hash_t *, const_t, const_t);
C_API void_t hash_get(hash_t *, const_t);
C_API bool hash_has(hash_t *, const_t);
C_API size_t hash_count(hash_t *);
C_API void_t hash_iter(hash_t *, void_t, hash_iter_func);
C_API void hash_delete(hash_t *, const_t);
C_API void hash_remove(hash_t *, const_t);
C_API void hash_print(hash_t *);
C_API void hash_print_custom(hash_t *, void (*print_key)(const_t k), void (*print_val)(const_t v));

/* Creates a new wait group coroutine hash table. */
C_API wait_group_t *ht_group_init(void);
/* Creates a new wait group results hash table. */
C_API wait_result_t *ht_result_init(void);

C_API gc_channel_t *ht_channel_init(void);

C_API ht_string_t *ht_string_init(void);

/* Creates/initialize the next series/collection of coroutine's created to be part of wait group, same behavior of Go's waitGroups, but without passing struct or indicating when done.

All coroutines here behaves like regular functions, meaning they return values, and indicate a terminated/finish status.

The initialization ends when `co_wait()` is called, as such current coroutine will pause, and execution will begin for the group of coroutines, and wait for all to finished. */
C_API wait_group_t *co_wait_group(void);

/* Pauses current coroutine, and begin execution for given coroutine wait group object, will wait for all to finish.
Returns hast table of results, accessible by coroutine id. */
C_API wait_result_t *co_wait(wait_group_t *);

/* Returns results of the given completed coroutine id, value in union value_t storage format. */
C_API value_t co_group_get_result(wait_result_t *, int);

C_API void co_result_set(routine_t *, void_t);
C_API void co_plain_set(routine_t *, size_t);

C_API void delete(void_t ptr);

C_API thread_local int coroutine_count;
C_API thread_local bool scheduler_info_log;
C_API thread_local uv_args_t *uv_server_args;

typedef struct _promise
{
    value_types type;
    values_t *result;
    mtx_t mutex;
    cnd_t cond;
    bool done;
    int id;
} promise;

typedef struct _future
{
    value_types type;
    thrd_t thread;
    thrd_func_t func;
    int id;
    promise *value;
} future;

typedef struct _future_arg
{
    value_types type;
    thrd_func_t func;
    void_t arg;
    promise *value;
} future_arg;

C_API future *future_create(callable_t);
C_API void future_start(future *, void_t);
C_API void future_close(future *);

C_API promise *promise_create();
C_API value_t promise_get(promise *);
C_API void promise_set(promise *, int);
C_API bool promise_done(promise *);
C_API void promise_close(promise *);

/* Calls fn (with args as arguments) in separate thread, returning without waiting
for the execution of fn to complete. The value `future` returned by fn can be accessed
by calling `co_async_get()`. */
C_API future *co_async(callable_t, void_t);

/* Returns the value of a promise, a future thread's shared object, If not ready this
function blocks the calling thread and waits until it is ready. */
C_API value_t co_async_get(future *);

/* Waits for the future thread's state to change. this function pauses current coroutine
and execute others until future is ready, thread execution has ended. */
C_API void co_async_wait(future *);

C_API unsigned long co_async_self(void);

/* Check for at least `n` bytes left on the stack. If not present, panic/abort. */
C_API void co_stack_check(int);

/* Generic "itoa" using current coroutine scrape buffer and `snprintf` */
C_API string_t co_itoa(int64_t number);

C_API int co_strpos(string_t text, string pattern);
C_API void co_strcpy(string dest, string_t src, size_t len);

C_API void gc_coroutine(routine_t *);
C_API void gc_channel(channel_t *);
C_API gc_channel_t *gc_channel_list(void);
C_API gc_coroutine_t *gc_coroutine_list(void);
C_API void gc_coroutine_free(void);
C_API void gc_channel_free(void);

/* Check if validated by json type */
C_API bool is_json(json_t *);

/**
* @param value Serialization of value to string.
* @param is_pretty Pretty serialization, if set `true`.
*/
C_API string json_serialize(json_t *, bool is_pretty);

/**
* @param text Parses first JSON value in a text, returns NULL in case of error.
* @param is_commented Ignores comments (/ * * / and //), if set `true`.
*/
C_API json_t *json_decode(string_t, bool is_commented);

C_API json_t *json_read(string_t filename, bool is_commented);
C_API int json_write(string_t filename, string_t text);

/**
* Creates json value `object` using a format like `printf` for each value to key.
*
* @param desc format string:
* * '`.`' indicate next format character will use dot function to record value for key name with dot,
* * '`a`' begin array encoding, every item `value` will be appended, until '`e`' is place in format desc,
* * '`e`' end array encoding,
* * '`n`' record `null` value for key, *DO NOT PLACE `NULL` IN ARGUMENTS*,
* * '`f`' record `float/double` number for key,
* * '`d`' record `signed` number for key,
* * '`i`' record `unsigned` number for key,
* * '`b`' record `boolean` value for key,
* * '`s`' record `string` value for key,
* * '`v`' record `JSON_Value` for key,
* @param arguments use `kv(key,value)` for pairs, *DO NOT PROVIDE FOR NULL, ONLY KEY*
*/
C_API json_t *json_encode(string_t desc, ...);

/**
* Creates json value string `array` using a format like `printf` for each value to index.
*
* @param desc format string:
* * '`n`' record `null` value for index, *DO NOT PLACE `NULL` IN ARGUMENTS*,
* * '`f`' record `float/double` number for index,
* * '`d`' record `signed` number for index,
* * '`i`' record `unsigned` number for index,
* * '`b`' record `boolean` value for index,
* * '`s`' record `string` value for index,
* * '`v`' record `JSON_Value` for index,
* @param arguments indexed by `desc` format order, *DO NOT PROVIDE FOR NULL*
*/
C_API string json_for(string_t desc, ...);

#define co_panic raii_panic

/* invalid address indicator */
#define CO_ERROR ((void_t)-1)

/* The `for_select` macro sets up a coroutine to wait on multiple channel
operations. Must be closed out with `select_end`, and if no `select_case(channel)`, `select_case_if(channel)`, `select_break` provided, an infinite loop is created.

This behaves same as GoLang `select {}` statement. */
#define for_select       \
  bool ___##__FUNCTION__; \
  while (true)            \
  {                       \
      ___##__FUNCTION__ = false; \
    if (true)

#define select_end              \
  if (___##__FUNCTION__ == false) \
      coroutine_yield();          \
  }

#define select_case(ch)                                 \
  if ((ch)->select_ready && ___##__FUNCTION__ == false) \
  {                                                     \
      (ch)->select_ready = false; if (true)

#define select_break      \
  ___##__FUNCTION__ = true; \
  }

#define select_case_if(ch) \
  select_break else select_case(ch)

/* The `select_default` is run if no other case is ready.
Must also closed out with `select_break()`. */
#define select_default()                         \
  select_break if (___##__FUNCTION__ == false) {

#define match(variable_type) switch (type_of(variable_type))
#define and(ENUM) case ENUM:
#define or(ENUM) break; case ENUM:
#define otherwise break; default:

#ifndef kv
#define kv(key, value) (key), (value)
#endif

#define c_int(data) co_value((data)).integer
#define c_long(data) co_value((data)).s_long
#define c_int64(data) co_value((data)).long_long
#define c_unsigned_int(data) co_value((data)).u_int
#define c_unsigned_long(data) co_value((data)).u_long
#define c_size_t(data) co_value((data)).max_size
#define c_const_char(data) co_value((data)).const_char
#define c_char(data) co_value((data)).schar
#define c_char_ptr(data) co_value((data)).char_ptr
#define c_bool(data) co_value((data)).boolean
#define c_float(data) co_value((data)).point
#define c_double(data) co_value((data)).precision
#define c_unsigned_char(data) co_value((data)).uchar
#define c_char_ptr_ptr(data) co_value((data)).array
#define c_unsigned_char_ptr(data) co_value((data)).uchar_ptr
#define c_short(data) co_value((data)).s_short
#define c_unsigned_short(data) co_value((data)).u_short
#define c_void_ptr(data) co_value((data)).object
#define c_callable(data) co_value((data)).func
#define c_cast(type, data) (type *)co_value((data)).object

#define c_integer(value) co_data((value)).integer
#define c_signed_long(value) co_data((value)).s_long
#define c_long_long(value) co_data((value)).long_long
#define c_unsigned_integer(value) co_data((value)).u_int
#define c_unsigned_long_int(value) co_data((value)).u_long
#define c_unsigned_long_long(value) co_data((value)).max_size
#define c_string(value) co_data((value)).const_char
#define c_signed_chars(value) co_data((value)).schar
#define c_const_chars(value) co_data((value)).char_ptr
#define c_boolean(value) co_data((value)).boolean
#define c_point(value) co_data((value)).point
#define c_precision(value) co_data((value)).precision
#define c_unsigned_chars(value) co_data((value)).uchar
#define c_chars_array(value) co_data((value)).array
#define c_unsigned_chars_ptr(value) co_data((value)).uchar_ptr
#define c_signed_shorts(value) co_data((value)).s_short
#define c_unsigned_shorts(value) co_data((value)).u_short
#define c_object(value) co_data((value)).object
#define c_func(value) co_data((value)).func
#define c_cast_of(type, value) (type *)co_data((value)).object

#define defer(func, arg) co_defer(FUNC_VOID(func), arg)

/* Cast argument to generic union values_t storage type */
#define args_cast(x) (values_t *)((x))

#define var(data) co_var((data))->value
#define as_var(variable, variable_type, data, enum_type) var_t *variable = (var_t *)co_new_by(1, sizeof(var_t)); \
    variable->type = enum_type; \
    variable->value = (variable_type *)co_new_by(1, sizeof(variable_type) + sizeof(data)); \
    memcpy(variable->value, &data, sizeof(data))

#define as_char(variable, data) as_var(variable, char, data, CO_CHAR_P)
#define as_string(variable, data) as_var(variable, char, data, CO_STRING)
#define as_long(variable, data) as_var(variable, long, data, CO_LONG)
#define as_int(variable, data) as_var(variable, int, data, CO_INTEGER)
#define as_uchar(variable, data) as_var(variable, unsigned char, data, CO_UCHAR)

#define as_instance(variable, variable_type) variable_type *variable = (variable_type *)co_new_by(1, sizeof(variable_type)); \
    variable->type = CO_STRUCT;

C_API bool is_base64(u_string_t src);
C_API bool is_tls(uv_handle_t *);
C_API bool is_status_invalid(routine_t *);

/* Write this function instead of `main`, this library provides its own main, the scheduler,
which call this function as an coroutine! */
int co_main(int, char **);

#ifdef __cplusplus
}
#endif

#endif
