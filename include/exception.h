#ifndef EXCEPTION_H
#define EXCEPTION_H
#include <setjmp.h>

#if !defined(__cplusplus) && !defined(__STDC__)
    #define __STDC__ 1
#endif

#ifndef C_API
    #define C_API extern
#endif

#if !defined(thread_local) /* User can override thread_local for obscure compilers */ /* Running in multi-threaded environment */
    #if defined(__STDC__) /* Compiling as C Language */
      #if defined(_MSC_VER) /* Don't rely on MSVC's C11 support */
        #define thread_local __declspec(thread)
      #elif __STDC_VERSION__ < 201112L /* If we are on C90/99 */
        #if defined(__clang__) || defined(__GNUC__) /* Clang and GCC */
          #define thread_local __thread
        #else /* Otherwise, we ignore the directive (unless user provides their own) */
          #define thread_local
        #endif
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
    #else
      #define thread_local
    #endif
#endif

#define EX_CAT_(a, b) a ## b
#define EX_CAT(a, b) EX_CAT_(a, b)

#define EX_STR_(a) #a
#define EX_STR(a) EX_STR_(a)

#define EX_NAME(e) EX_CAT(ex_err_, e)
#define EX_PNAME(p) EX_CAT(ex_protected_, p)

#define EX_MAKE()                                              \
    const char *const err = ((void)err, ex_err.ex);             \
    const char *const err_file = ((void)err_file, ex_err.file); \
    const int err_line = ((void)err_line, ex_err.line)

#ifdef sigsetjmp
    #define ex_jmp_buf         sigjmp_buf
    #define ex_setjmp(buf)    sigsetjmp(buf,1)
    #define ex_longjmp(buf,st) siglongjmp(buf,st)
#else
    #define ex_jmp_buf         jmp_buf
    #define ex_setjmp(buf)    setjmp(buf)
    #define ex_longjmp(buf,st) longjmp(buf,st)
#endif

#define EX_EXCEPTION(E) \
    static const char EX_NAME(E)[] = EX_STR(E)

#define try                                   \
    {                                         \
        /* local context */                   \
        ex_context_t ex_err;                  \
        if (!ex_context)                      \
            ex_init();                        \
        ex_err.next = ex_context;             \
        ex_err.stack = 0;                     \
        ex_err.ex = 0;                        \
        ex_err.unstack = 0;                   \
        /* global context updated */          \
        ex_context = &ex_err;                 \
        /* save jump location */              \
        ex_err.state = ex_setjmp(ex_err.buf); \
        if (ex_err.state == ex_try_st)        \
        {                                     \
            {

#define catch(E)                        \
    }                                   \
    }                                   \
    if (ex_err.state == ex_throw_st)    \
    {                                   \
        extern const char EX_NAME(E)[]; \
        if (ex_err.ex == EX_NAME(E))    \
        {                               \
            EX_MAKE();                  \
            ex_err.state = ex_catch_st;

#define catch_any                    \
    }                                \
    }                                \
    if (ex_err.state == ex_throw_st) \
    {                                \
        {                            \
            EX_MAKE();               \
            ex_err.state = ex_catch_st;

#define finally                          \
    }                                    \
    }                                    \
    {                                    \
        {                                \
            EX_MAKE();                   \
            /* global context updated */ \
            ex_context = ex_err.next;

#define rethrow() \
    ex_throw(ex_err.ex, ex_err.file, ex_err.line, ex_err.function)

#define ex_throw_loc(E, F, L, C)           \
    do                                  \
    {                                   \
        extern const char EX_NAME(E)[]; \
        ex_throw(EX_NAME(E), F, L, C);     \
    } while (0)

#define throw(E) \
    ex_throw_loc(E, __FILE__, __LINE__, __FUNCTION__)

#define end_try                            \
    }                                      \
    }                                      \
    if (ex_context == &ex_err)             \
        /* global context updated */       \
        ex_context = ex_err.next;          \
    if ((ex_err.state & ex_throw_st) != 0) \
        rethrow();                         \
    }

#ifdef __cplusplus
extern "C" {
#endif

/* Some common exception */
EX_EXCEPTION(invalid_type);
EX_EXCEPTION(range_error);
EX_EXCEPTION(division_by_zero);
EX_EXCEPTION(out_of_memory);

/* Some signal exception */
EX_EXCEPTION(sig_abrt);
EX_EXCEPTION(sig_alrm);
EX_EXCEPTION(sig_bus);
EX_EXCEPTION(sig_fpe);
EX_EXCEPTION(sig_ill);
EX_EXCEPTION(sig_int);
EX_EXCEPTION(sig_quit);
EX_EXCEPTION(sig_segv);
EX_EXCEPTION(sig_term);

C_API void ex_throw(const char *, const char *, int, const char *);
C_API int ex_uncaught_exception(void);
C_API void ex_terminate(void);
C_API void (*ex_signal(int, const char *))(int);
C_API void ex_init(void);

/* Convert signals into exceptions */
C_API void ex_signal_std(void);

enum
{
    ex_try_st,
    ex_throw_st,
    ex_catch_st
};

typedef struct ex_ptr_s ex_ptr_t;
typedef struct ex_context_s ex_context_t;

/* stack of protected pointer */
struct ex_ptr_s
{
    ex_ptr_t *next;
    void (*func)(void *);
    void **ptr;
};

/* stack of exception */
struct ex_context_s
{
    /* The handler in the stack (which is a FILO container). */
    ex_context_t *next;
    ex_ptr_t *stack;

    /** The function from which the exception was thrown */
    const char *volatile function;

    /** The name of this exception */
    const char *volatile ex;

    /* The file from whence this handler was made, in order to backtrace it later (if we want to). */
    const char *volatile file;

    /* The line from whence this handler was made, in order to backtrace it later (if we want to). */
    int volatile line;

    /** The value of errno at the time the exception was thrown */
    int error_number;

    /* Which is our active state? */
    int volatile state;

    int unstack;

    /* The program state in which the handler was created, and the one to which it shall return. */
    ex_jmp_buf buf;
};

C_API thread_local ex_context_t *ex_context;

static ex_ptr_t ex_ptr(ex_ptr_t *const_ptr, void *ptr, void (*func)(void *))
{
  if (!ex_context) ex_init();
  const_ptr->next = ex_context->stack;
  const_ptr->func = func;
  const_ptr->ptr = ptr;
  ex_context->stack = const_ptr;
  return *const_ptr;
  (void)ex_ptr;
}

/* Protects dynamically allocated memory against exceptions.
If the object pointed by `ptr` changes before `unprotected()`,
the new object will be automatically protected.

If `ptr` is not null, `func(ptr)` will be invoked during stack unwinding. */
#define protected(ptr, func) ex_ptr_t EX_PNAME(ptr) = ex_ptr(&EX_PNAME(ptr), &ptr, func)

/* Remove memory pointer protection, does not free the memory. */
#define unprotected(p) (ex_context->stack = EX_PNAME(p).next)

#ifdef __cplusplus
}
#endif

#endif /* EXCEPTION_H */
