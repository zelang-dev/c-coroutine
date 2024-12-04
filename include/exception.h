#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <inttypes.h>
#if !defined(_WIN32)
    #include <sys/time.h>
    #include <sys/resource.h> /* setrlimit() */
    #include <sys/mman.h>
    #include <fcntl.h>
#endif

/* exception config
 */
#if defined(__GNUC__) && (!defined(_WIN32) || !defined(_WIN64))
    #define _GNU_SOURCE
#endif

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #include <intrin.h>
    #include "compat/sys/mman.h"
    #include "compat/unistd.h"
    #include "compat/fcntl.h"
    #if !defined(__cplusplus)
        #define __STDC__ 1
    #endif
#endif

#define EX_MAX_NAME_LEN  256
#if !defined(RAII_MALLOC) || !defined(RAII_FREE) || !defined(RAII_REALLOC)|| !defined(RAII_CALLOC)
  #include "rpmalloc.h"
  #define RAII_MALLOC malloc
  #define RAII_FREE rpfree
  #define RAII_REALLOC realloc
  #define RAII_CALLOC calloc
  #define RAII_MEMALIGN memalign
#endif

#ifndef RAII_INLINE
  #define RAII_INLINE FORCEINLINE
#endif

#ifndef RAII_NO_INLINE
  #define RAII_NO_INLINE NO_INLINE
#endif

#ifndef __builtin_expect
    #define __builtin_expect(x, y) (x)
#endif

#ifndef LIKELY_IS
    #define LIKELY_IS(x, y) __builtin_expect((x), (y))
    #define LIKELY(x) LIKELY_IS(!!(x), 1)
    #define UNLIKELY(x) LIKELY_IS((x), 0)
    #ifndef INCREMENT
        #define INCREMENT 16
    #endif
#endif

#if defined(USE_DEBUG)
#   include <assert.h>
#   define RAII_ASSERT(c) assert(c)
#   define RAII_LOG(s) puts(s)
#   define RAII_INFO(s, ...) printf(s, __VA_ARGS__ )
#   define RAII_HERE() fprintf(stderr, "Here %s:%d\n", __FILE__, __LINE__)
#   ifdef _WIN32
#       include <DbgHelp.h>
#       pragma comment(lib,"Dbghelp.lib")
#   else
#       include <execinfo.h>
#   endif
#else
#   define RAII_ASSERT(c)
#   define RAII_LOG(s) (void)s
#   define RAII_INFO(s, ...)  (void)s
#   define RAII_HERE()  (void)0
#endif

 /* Public API qualifier. */
#ifndef C_API
    #define C_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ex_ptr_s ex_ptr_t;
typedef struct ex_context_s ex_context_t;
typedef struct ex_backtrace_s ex_backtrace_t;
typedef void (*ex_setup_func)(ex_context_t *, const char *, const char *);
typedef void (*ex_terminate_func)(void);
typedef void (*ex_unwind_func)(void *);

/* low-level api
 */
C_API void ex_throw(const char *ex, const char *file, int, const char *line, const char *message);
C_API int ex_uncaught_exception(void);
C_API void ex_terminate(void);
C_API ex_context_t *ex_init(void);
C_API ex_context_t *ex_local(void);
C_API void ex_update(ex_context_t *);
C_API void ex_unwind_set(ex_context_t *ctx, bool flag);
C_API void ex_data_set(ex_context_t *ctx, void *data);
C_API void ex_swap_set(ex_context_t *ctx, void *data);
C_API void ex_swap_reset(ex_context_t *ctx);
C_API void *ex_data_get(ex_context_t *ctx, void *data);
C_API void ex_flags_reset(void);
C_API void ex_signal(int sig, const char *ex);

/* Convert signals into exceptions */
C_API void ex_signal_setup(void);

/* Reset signal handler to default */
C_API void ex_signal_default(void);
C_API void ex_backtrace(ex_backtrace_t *ex);
C_API void ex_trace_set(ex_context_t *ex, void_t ctx);

#ifdef _WIN32
#define EXCEPTION_PANIC 0xE0000001
C_API void ex_signal_seh(DWORD sig, const char *ex);
C_API int catch_seh(const char *exception, DWORD code, struct _EXCEPTION_POINTERS *ep);
C_API int catch_filter_seh(DWORD code, struct _EXCEPTION_POINTERS *ep);
#endif

/***********************************************************
 * Implementation
 */

/* states
*/
enum {
    ex_try_st,
    ex_throw_st,
    ex_catch_st,
    ex_protected_st,
    ex_context_st
};

/* some useful macros
*/
#define EX_CAT(a, b) a ## b

#define EX_STR_(a) #a
#define EX_STR(a) EX_STR_(a)

#define EX_NAME(e) EX_CAT(ex_err_, e)
#define EX_PNAME(p) EX_CAT(ex_protected_, p)

#define EX_MAKE()                                               \
    const char *const err = ((void)err, ex_err.ex);             \
    const char *const err_file = ((void)err_file, ex_err.file); \
    ex_backtrace_t *const err_backtrace = ((void)err_backtrace, ex_err.backtrace); \
    const int err_line = ((void)err_line, ex_err.line)

#define EX_MAKE_IF()                                            \
    const char *const err = ((void)err, !is_empty((void *)ex_err.panic) ? ex_err.panic : ex_err.ex);    \
    const char *const err_file = ((void)err_file, ex_err.file); \
    ex_backtrace_t *const err_backtrace = ((void)err_backtrace, ex_err.backtrace); \
    const int err_line = ((void)err_line, ex_err.line)

/* macros
 */
#define EX_EXCEPTION(E) \
        const char EX_NAME(E)[] = EX_STR(E)

 /* context savings
*/
#ifdef sigsetjmp
    #define ex_jmp_buf  sigjmp_buf
    #define ex_setjmp(buf)  sigsetjmp(buf,1)
    #define ex_longjmp(buf,st)  siglongjmp(buf,st)
#else
    #define ex_jmp_buf  jmp_buf
    #define ex_setjmp(buf)  setjmp(buf)
    #define ex_longjmp(buf,st)  longjmp(buf,st)
#endif

#define ex_throw_loc(E, F, L, C)        \
    do {                                \
        C_API const char EX_NAME(E)[];  \
        ex_throw(EX_NAME(E), F, L, C, NULL);    \
    } while (0)

/* An macro that stops the ordinary flow of control and begins panicking,
throws an exception of given message. */
#define raii_panic(message)                                                     \
    do {                                                                        \
        C_API const char EX_NAME(panic)[];                                      \
        ex_throw(EX_NAME(panic), __FILE__, __LINE__, __FUNCTION__, (message));  \
    } while (0)

#ifdef _WIN32
#define throw(E) raii_panic(EX_STR(E))
#define rethrow()                                       \
        if (ex_err.caught > -1 || ex_err.is_rethrown) { \
            ex_err.is_rethrown = false;                         \
            ex_longjmp(ex_err.buf, ex_err.state | ex_throw_st); \
        } else if (true) {                                                \
            ex_throw(ex_err.ex, ex_err.file, ex_err.line, ex_err.function, ex_err.panic); \
        }

#define ex_signal_block(ctrl)               \
    CRITICAL_SECTION ctrl##__FUNCTION__;    \
    InitializeCriticalSection(&ctrl##__FUNCTION__); \
    EnterCriticalSection(&ctrl##__FUNCTION__);

#define ex_signal_unblock(ctrl)                 \
    LeaveCriticalSection(&ctrl##__FUNCTION__);  \
    DeleteCriticalSection(&ctrl##__FUNCTION__);

#define ex_try                              \
{                                           \
    if (!exception_signal_set)              \
        ex_signal_setup();                  \
    /* local context */                     \
    ex_context_t ex_err;                    \
    ex_err.next = ex_init();               \
    ex_err.stack = 0;                       \
    ex_err.ex = 0;                          \
    ex_err.unstack = 0;                     \
    ex_err.is_rethrown = false;             \
    /* global context updated */            \
    ex_update(&ex_err);                     \
    /* save jump location */                \
    ex_err.state = ex_setjmp(ex_err.buf);   \
    if (ex_err.state == ex_try_st) {		\
		__try {

#define ex_catch(E)                             \
		} __except(catch_seh(EX_STR(E), GetExceptionCode(), GetExceptionInformation())) {   \
			if (ex_err.state == ex_throw_st) {  \
				EX_MAKE_IF();                   \
				ex_err.state = ex_catch_st;

#define ex_finally						\
			}							\
		}                               \
    }                                   \
        {                               \
            EX_MAKE_IF();               \
            /* global context updated */\
            ex_update(ex_err.next);

#define ex_end_try                          \
    }										\
    if (ex_init() == &ex_err)  \
        /* global context updated */        \
        ex_update(ex_err.next);             \
    ex_err.caught = -1;                     \
    ex_err.is_rethrown = false;             \
    if ((ex_err.state & ex_throw_st) != 0)  \
        rethrow();                          \
}

#define ex_catch_any                    	    \
        } __except(catch_filter_seh(GetExceptionCode(), GetExceptionInformation())) {   \
            if (ex_err.state == ex_throw_st) {  \
                EX_MAKE_IF();                   \
                ex_err.state = ex_catch_st;

#define ex_catch_if                             \
        } __except(catch_filter_seh(GetExceptionCode(), GetExceptionInformation())) {   \
            if (ex_err.state == ex_throw_st) {  \
                EX_MAKE_IF();

#define ex_finality							\
        } __finally {						\
			EX_MAKE_IF();					\
			/* global context updated */	\
			ex_update(ex_err.next);

#define ex_end_trying	 ex_finally ex_end_try

#else
#define ex_signal_block(ctrl)                  \
    sigset_t ctrl##__FUNCTION__, ctrl_all##__FUNCTION__; \
    sigfillset(&ctrl##__FUNCTION__); \
    pthread_sigmask(SIG_SETMASK, &ctrl##__FUNCTION__, &ctrl_all##__FUNCTION__);

#define ex_signal_unblock(ctrl)                  \
    pthread_sigmask(SIG_SETMASK, &ctrl_all##__FUNCTION__, NULL);

#define rethrow()  \
    ex_throw(ex_err.ex, ex_err.file, ex_err.line, ex_err.function, ex_err.panic)

#define throw(E) \
    ex_throw_loc(E, __FILE__, __LINE__, __FUNCTION__)
#define ex_try                              \
{                                           \
    if (!exception_signal_set)              \
        ex_signal_setup();                  \
    /* local context */                     \
    ex_context_t ex_err;                    \
    ex_err.next = ex_init();                \
    ex_err.stack = 0;                       \
    ex_err.ex = 0;                          \
    ex_err.unstack = 0;                     \
    /* global context updated */            \
    ex_update(&ex_err);                     \
    /* save jump location */                \
    ex_err.state = ex_setjmp(ex_err.buf);   \
    if (ex_err.state == ex_try_st)          \
        {                                   \
        {

#define ex_catch_any                    \
    }                                \
    }                                \
    if (ex_err.state == ex_throw_st) \
    {                                \
        {                            \
            EX_MAKE();               \
            ex_err.state = ex_catch_st;

#define ex_catch_if                    \
    }                                \
    }                                \
    if (ex_err.state == ex_throw_st) \
    {                                \
        {                            \
            EX_MAKE();               \

#define ex_finally                          \
    }                                    \
    }                                    \
    {                                    \
        {                                \
            EX_MAKE();                   \
            /* global context updated */ \
            ex_update(ex_err.next);

#define ex_end_try                            \
    }                                      \
    }                                      \
    if (ex_init() == &ex_err)  \
        /* global context updated */       \
        ex_update(ex_err.next);            \
    if ((ex_err.state & ex_throw_st) != 0) \
        rethrow();                         \
    }

#define ex_catch(E)                        \
    }                                   \
    }                                   \
    if (ex_err.state == ex_throw_st)    \
    {                                   \
        C_API const char EX_NAME(E)[];  \
        if (ex_err.ex == EX_NAME(E))    \
        {                               \
            EX_MAKE();                  \
            ex_err.state = ex_catch_st;
#endif

/* types
*/

struct ex_backtrace_s {
#ifdef _WIN32
    CONTEXT ctx[1];
    HANDLE process;
    HANDLE thread;
#else
    void *ctx[EX_MAX_NAME_LEN];
    size_t size;
    char **dump;
#endif
};

/* stack of exception */
struct ex_context_s {
    int type;
    bool is_unwind;
    bool is_rethrown;
    bool is_guarded;
    bool is_raii;
    bool is_emulated;
    int unstack;
    int volatile caught;

    /* The line from whence this handler was made, in order to backtrace it later (if we want to). */
    int volatile line;

    /* Which is our active state? */
    int volatile state;
    void *data;
    void *prev;

    /* The handler in the stack (which is a FILO container). */
    ex_context_t *next;
    ex_ptr_t *stack;

    /** The panic error message thrown */
    const char *volatile panic;

    /** The function from which the exception was thrown */
    const char *volatile function;

    /** The name of this exception */
    const char *volatile ex;

    /* The file from whence this handler was made, in order to backtrace it later (if we want to). */
    const char *volatile file;

    ex_backtrace_t backtrace[1];

    /* The program state in which the handler was created, and the one to which it shall return. */
    ex_jmp_buf buf;
};

/* stack of protected pointer */
struct ex_ptr_s {
    int type;
    bool is_emulated;
    ex_ptr_t *next;
    ex_unwind_func func;
    void **ptr;
};

/* extern declaration
*/

C_API ex_setup_func exception_setup_func;
C_API ex_unwind_func exception_unwind_func;
C_API ex_terminate_func exception_terminate_func;
C_API ex_terminate_func exception_ctrl_c_func;
C_API bool exception_signal_set;

/* pointer protection
*/
C_API ex_ptr_t ex_protect_ptr(ex_ptr_t *const_ptr, void *ptr, void (*func)(void *));
C_API void ex_unprotected_ptr(ex_ptr_t *const_ptr);

/* Protects dynamically allocated memory against exceptions.
If the object pointed by `ptr` changes before `unprotected()`,
the new object will be automatically protected.

If `ptr` is not null, `func(ptr)` will be invoked during stack unwinding. */
#define protected(ptr, func) ex_ptr_t EX_PNAME(ptr) = ex_protect_ptr(&EX_PNAME(ptr), &ptr, func)

/* Remove memory pointer protection, does not free the memory. */
#define unprotected(p) (ex_local()->stack = EX_PNAME(p).next)

#ifdef __cplusplus
}
#endif

#endif /* EXCEPTION_H */
