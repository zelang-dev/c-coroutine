#include <stddef.h> /* for size_t */
#include <string.h>
#if defined(__clang__) || defined(__GNUC__)
    #include <stdint.h>
#endif

#if defined(_MSC_VER)
    #define CO_MPROTECT 1
    #if !defined(__cplusplus)
        #define __STDC__ 1
    #endif
#endif

#ifdef CO_DEBUG
    #include <stdio.h>
    #define CO_LOG(s) puts(s);
#else
    #define CO_LOG(s)
#endif

/* Minimum stack size when creating a coroutine. */
#ifndef CO_MIN_STACK_SIZE
    #define CO_MIN_STACK_SIZE 32768
#endif

/* Default stack size when creating a coroutine. */
#ifndef CO_DEFAULT_STACK_SIZE
    #define CO_DEFAULT_STACK_SIZE 65536
#endif

/* Size of coroutine storage buffer. */
#ifndef CO_DEFAULT_STORAGE_SIZE
    #define CO_DEFAULT_STORAGE_SIZE 1024
#endif
/* Number used only to assist checking for stack overflows. */
#define CO_MAGIC_NUMBER 0x7E3CB1A9

/* Public API qualifier. */
#ifndef C_API
    #define C_API extern
#endif

#ifndef CO_ASSERT
  #ifdef CO_DEBUG
    #include <assert.h>
    #define CO_ASSERT(c) assert(c)
  #else
    #define CO_ASSERT(c)
  #endif
#endif

/*[amd64, arm, ppc, x86]:
   by default, co_swap_function is marked as a text (code) section
   if not supported, uncomment the below line to use mprotect instead */
/* #define CO_MPROTECT */

/*[amd64]:
   Win64 only: provides a substantial speed-up, but will thrash XMM regs
   do not use this unless you are certain your application won't use SSE */
/* #define CO_NO_SSE */

#if !defined(thread_local) /* User can override thread_local for obscure compilers */
  #if !defined(CO_MP) /* Running in single-threaded environment */
    #define thread_local
  #else /* Running in multi-threaded environment */
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
    #endif
  #endif
#endif

#ifndef CO_FORCE_INLINE
  #ifdef _MSC_VER
    #define CO_FORCE_INLINE __forceinline
  #elif defined(__GNUC__)
    #if defined(__STRICT_ANSI__)
      #define CO_FORCE_INLINE __inline__ __attribute__((always_inline))
    #else
      #define CO_FORCE_INLINE inline __attribute__((always_inline))
    #endif
  #elif defined(__BORLANDC__) || defined(__DMC__) || defined(__SC__) || defined(__WATCOMC__) || defined(__LCC__) ||  defined(__DECC)
    #define CO_FORCE_INLINE __inline
  #else /* No inline support. */
    #define CO_FORCE_INLINE
  #endif
#endif

#ifndef CO_NO_INLINE
  #ifdef __GNUC__
    #define CO_NO_INLINE __attribute__((noinline))
  #elif defined(_MSC_VER)
    #define CO_NO_INLINE __declspec(noinline)
  #else
    #define CO_NO_INLINE
  #endif
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

#if !defined(CO_ASSERT)
  #include <assert.h>
  #define CO_ASSERT assert
#endif

#if defined (__OpenBSD__)
  #if !defined(CO_MALLOC) || !defined(CO_FREE)
    #include <unistd.h>
    #include <sys/mman.h>

    static void* malloc_obsd(size_t size) {
      long pagesize = sysconf(_SC_PAGESIZE);
      char* memory = (char*)mmap(NULL, size + pagesize, PROT_READ|PROT_WRITE, MAP_STACK|MAP_PRIVATE|MAP_ANON, -1, 0);
      if (memory == MAP_FAILED) return NULL;
      *(size_t*)memory = size + pagesize;
      memory += pagesize;
      return (void*)memory;
    }

    static void free_obsd(void *ptr) {
      char* memory = (char*)ptr - sysconf(_SC_PAGESIZE);
      munmap(memory, *(size_t*)memory);
    }

    #define CO_MALLOC malloc_obsd
    #define CO_FREE   free_obsd
  #endif
#endif

#if !defined(CO_MALLOC) || !defined(CO_FREE)
  #include <stdlib.h>
  #define CO_MALLOC malloc
  #define CO_FREE   free
#endif

#if defined(_MSC_VER)
  #define section(name) __declspec(allocate("." #name))
#elif defined(__APPLE__)
  #define section(name) __attribute__((section("__TEXT,__" #name)))
#else
  #define section(name) __attribute__((section("." #name "#")))
#endif

#define CO_ALIGN(value,alignment) (((value) + ((alignment) - 1)) & ~((alignment) - 1))

#if defined(__clang__) || defined(__GNUC__)
  #if defined(__i386__)
    #define CO_STACK_ALIGNMENT 64
    #define CO_STACK_MINIMAL_SIZE 128 * 1024
  #elif defined(__amd64__)
    #define CO_STACK_ALIGNMENT 64
    #define CO_STACK_MINIMAL_SIZE 128 * 1024
  #elif defined(__arm__)
    #define CO_STACK_ALIGNMENT 16
    #define CO_STACK_MINIMAL_SIZE 128
  #elif defined(__aarch64__)
    #define CO_STACK_ALIGNMENT 32
    #define CO_STACK_MINIMAL_SIZE 256
  #elif defined(__powerpc64__) && defined(_CALL_ELF) && _CALL_ELF == 2
    #define CO_STACK_ALIGNMENT 64
    #define CO_STACK_MINIMAL_SIZE 128 * 1024
  #elif defined(_ARCH_PPC) && !defined(__LITTLE_ENDIAN__)
    #define CO_STACK_ALIGNMENT 64
    #define CO_STACK_MINIMAL_SIZE 128 * 1024
  #elif defined(_WIN32)
    #define CO_STACK_ALIGNMENT 64
    #define CO_STACK_MINIMAL_SIZE 128 * 1024
  #else
    #define CO_STACK_ALIGNMENT 64
    #define CO_STACK_MINIMAL_SIZE 128 * 1024
  #endif
#elif defined(_MSC_VER)
  #if defined(_M_IX86)
    #define CO_STACK_ALIGNMENT 32
    #define CO_STACK_MINIMAL_SIZE 128 * 1024
  #elif defined(_M_AMD64)
    #define CO_STACK_ALIGNMENT 64
    #define CO_STACK_MINIMAL_SIZE 128 * 1024
  #else
    #define CO_STACK_ALIGNMENT 64
    #define CO_STACK_MINIMAL_SIZE 128 * 1024
  #endif
#else
  #error "unsupported processor, compiler or operating system"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* Coroutine states. */
typedef enum co_state
{
    CO_DEAD = 0, /* The coroutine is uninitialized or deleted. */
    CO_NORMAL,   /* The coroutine is active but not running (that is, it has switch to another coroutine). */
    CO_RUNNING,  /* The coroutine is active and running. */
    CO_SUSPENDED /* The coroutine is suspended (in a call to yield/switch, or it has not started running yet). */
} co_state;

/* Coroutine result codes. */
typedef enum co_result
{
    CO_SUCCESS = 0,
    CO_GENERIC_ERROR,
    CO_INVALID_POINTER,
    CO_INVALID_COROUTINE,
    CO_NOT_SUSPENDED,
    CO_NOT_RUNNING,
    CO_MAKE_CONTEXT_ERROR,
    CO_SWITCH_CONTEXT_ERROR,
    CO_NOT_ENOUGH_SPACE,
    CO_OUT_OF_MEMORY,
    CO_INVALID_ARGUMENTS,
    CO_INVALID_OPERATION,
    CO_STACK_OVERFLOW,
} co_result;

typedef void *(*co_callable_t)(void *);

/* Coroutine structure. */
typedef struct co_routine_t co_routine_t;
typedef struct co_value co_value_t;

struct co_routine_t
{
    void *handle;
    void *allocated;
    /* Stack base address, can be used to scan memory in a garbage collector. */
    void *stack_base;
    co_callable_t func;
    /* Coroutine stack size. */
    size_t stack_size;
    /* Used to check stack overflow. */
    size_t magic_number;
    co_state state;
    unsigned char *storage;
    size_t bytes_stored;
    /* Coroutine storage size, to be used with the storage APIs. */
    size_t storage_size;
    unsigned char halt;
    void *args;
    /* Coroutine return result on function exit. */
    void *results;
};

enum value_args_t
{
    CO_FUNC,
    CO_OBJ,
    CO_INT,
    CO_FLOAT,
    CO_DOUBLE,
    CO_BOOL,
    CO_STRING,
    CO_ARRAY_CHR,
    CO_ARRAY_INT
};

typedef struct co_value co_value_t;
typedef union
{
    int integer;
    float point;
    double precision;
    unsigned char boolean;
    char *string;
    char **array_char;
    int *array_int;
    void *object;
    co_callable_t function;
} value_t;

struct co_value
{
    value_t value;
    int type;
    size_t size_args;
    size_t number_args;
};

/* Return handle to current coroutine. */
C_API co_routine_t *co_active(void);

/* Initializes new coroutine. */
C_API co_routine_t *co_derive(void *, unsigned int, co_callable_t func, void *);

/* Create new coroutine. */
C_API co_routine_t *co_create(unsigned int, co_callable_t func, void *);

/* Delete specified coroutine. */
C_API void co_delete(co_routine_t *);

/* Switch to specified coroutine. */
C_API void co_switch(co_routine_t *);

/* Check for coroutine completetion and return. */
C_API unsigned char co_terminated(co_routine_t *);

/* Results from an coroutine function completetion and return. */
C_API void *co_results(co_routine_t *) ;

C_API int co_serializable(void);

/* Return handle to previous coroutine. */
C_API co_routine_t *co_running(void);

/* Get the description of a result. */
C_API const char *co_result_description(co_result res);

/* Initialize and starts the coroutine passing any args. */
C_API co_routine_t *co_start(co_callable_t func, void *);

C_API value_t co_value(void *data);

/* Suspends the execution of current coroutine. */
C_API void co_suspend(void);

/* Resume specified coroutine. */
C_API void co_resume(co_routine_t *);

/* Returns the status of the coroutine. */
C_API co_state co_status(co_routine_t *);

/* Storage interface functions, used to pass values between suspend and resume/switch. */

/* Push bytes to the coroutine storage. Use to send values between suspend and resume/switch. */
C_API co_result co_push(co_routine_t *, const void *src, size_t len);

/* Pop bytes from the coroutine storage. Use to get values between suspend and resume/switch. */
C_API co_result co_pop(co_routine_t *, void *dest, size_t len);

/* Like `co_pop` but it does not consumes the storage. */
C_API co_result co_peek(co_routine_t *, void *dest, size_t len);

/* Get the available bytes that can be retrieved with a `co_pop`. */
C_API size_t co_get_bytes_stored(co_routine_t *);

 /* Get the total storage size. */
C_API size_t co_get_storage_size(co_routine_t *);
#ifdef __cplusplus
}
#endif
