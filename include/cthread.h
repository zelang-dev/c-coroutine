#ifndef _CTHREAD_H_
#define _CTHREAD_H_

#if defined(_WIN32) || defined(_WIN64)
    #if !defined(__cplusplus)
        #define __STDC__ 1
    #endif
#endif

#if defined(__TINYC__) || defined(USE_EMULATED_TLS)
    #undef emulate_tls
    #define emulate_tls 1
    #ifndef thread_local
        #define thread_local
    #endif
#elif !defined(thread_local) /* User can override thread_local for obscure compilers */
     /* Running in multi-threaded environment */
    #if defined(__STDC__) /* Compiling as C Language */
      #if defined(_MSC_VER) /* Don't rely on MSVC's C11 support */
        #define thread_local __declspec(thread)
      #elif __STDC_VERSION__ < 201112L /* If we are on C90/99 */
        #if defined(__clang__) || defined(__GNUC__) /* Clang and GCC */
          #define thread_local __thread
        #else /* Otherwise, we ignore the directive (unless user provides their own) */
          #define thread_local
          #define emulate_tls 1
        #endif
      #elif __APPLE__ && __MACH__
        #define thread_local __thread
      #else /* C11 and newer define thread_local in threads.h */
        #define HAS_C11_THREADS 1
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
          #define emulate_tls 1
        #endif
      #else /* In C++ >= 11, thread_local in a builtin keyword */
        /* Don't do anything */
      #endif
      #define HAS_C11_THREADS 1
    #endif
#endif

#ifndef HAS_C11_THREADS
#ifdef __cplusplus
extern "C" {
#endif

/* Which platform are we on? */
#if !defined(_CTHREAD_PLATFORM_DEFINED_)
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
    #define _CTHREAD_WIN32_
#else
    #define _CTHREAD_POSIX_
#endif
    #define _CTHREAD_PLATFORM_DEFINED_
#endif

/* Activate some POSIX functionality (e.g. clock_gettime and recursive mutexes) */
#if defined(_CTHREAD_POSIX_)
#undef _FEATURES_H
    #if !defined(_GNU_SOURCE)
        #define _GNU_SOURCE
    #endif
    #if !defined(_POSIX_C_SOURCE) || ((_POSIX_C_SOURCE - 0) < 199309L)
        #undef _POSIX_C_SOURCE
        #define _POSIX_C_SOURCE 199309L
    #endif
    #if !defined(_XOPEN_SOURCE) || ((_XOPEN_SOURCE - 0) < 500)
        #undef _XOPEN_SOURCE
        #define _XOPEN_SOURCE 500
    #endif
#define _XPG6
#endif

/* Generic includes */
#include <pthread.h>

/* Platform specific includes */
#if defined(_CTHREAD_POSIX_)
    #include <signal.h>
    #include <sched.h>
    #include <unistd.h>
    #include <sys/time.h>
    #include <errno.h>
    #include <limits.h>
#elif defined(_CTHREAD_WIN32_)
    #include <windows.h>
    #include <sys/timeb.h>
#   ifndef usleep
#       define  usleep(n) Sleep(n)
#   endif
#endif
#include <time.h>
#include <stdbool.h>

/* Compiler-specific information */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define CTHREAD_NORETURN _Noreturn
#elif defined(__GNUC__)
    #define CTHREAD_NORETURN __attribute__((__noreturn__))
#else
    #define CTHREAD_NORETURN
#endif

/* Function return values */
enum
{
  thrd_success  = 0, /**< The requested operation succeeded */
  thrd_busy     = 1, /**< The requested operation failed because a resource requested by a test and return function is already in use */
  thrd_error    = 2, /**< The requested operation failed */
  thrd_nomem    = 3, /**< The requested operation failed because it was unable to allocate memory */
  thrd_timedout = 4  /**< The time specified in the call was reached without acquiring the requested resource */
};

/* Mutex types */
enum
{
  mtx_plain     = 0,
  mtx_recursive = 1,
  mtx_timed     = 2
};

/* Mutex */
typedef pthread_mutex_t mtx_t;

/** Create a mutex object.
* @param mtx A mutex object.
* @param type Bit-mask that must have one of the following six values:
*   @li @c mtx_plain for a simple non-recursive mutex
*   @li @c mtx_timed for a non-recursive mutex that supports timeout
*   @li @c mtx_plain | @c mtx_recursive (same as @c mtx_plain, but recursive)
*   @li @c mtx_timed | @c mtx_recursive (same as @c mtx_timed, but recursive)
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int mtx_init(mtx_t *mtx, int type);

/** Release any resources used by the given mutex.
* @param mtx A mutex object.
*/
void mtx_destroy(mtx_t *mtx);

/** Lock the given mutex.
* Blocks until the given mutex can be locked. If the mutex is non-recursive, and
* the calling thread already has a lock on the mutex, this call will block
* forever.
* @param mtx A mutex object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int mtx_lock(mtx_t *mtx);

/** Lock the given mutex, or block until a specific point in time.
* Blocks until either the given mutex can be locked, or the specified TIME_UTC
* based time.
* @param mtx A mutex object.
* @param ts A UTC based calendar time
* @return @ref The mtx_timedlock function returns thrd_success on success, or
* thrd_timedout if the time specified was reached without acquiring the
* requested resource, or thrd_error if the request could not be honored.
*/
int mtx_timedlock(mtx_t *mtx, const struct timespec *ts);

/** Try to lock the given mutex.
* The specified mutex shall support either test and return or timeout. If the
* mutex is already locked, the function returns without blocking.
* @param mtx A mutex object.
* @return @ref thrd_success on success, or @ref thrd_busy if the resource
* requested is already in use, or @ref thrd_error if the request could not be
* honored.
*/
int mtx_trylock(mtx_t *mtx);

/** Unlock the given mutex.
* @param mtx A mutex object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int mtx_unlock(mtx_t *mtx);

/* Condition variable */
typedef pthread_cond_t cnd_t;

/** Create a condition variable object.
* @param cond A condition variable object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int cnd_init(cnd_t *cond);

/** Release any resources used by the given condition variable.
* @param cond A condition variable object.
*/
void cnd_destroy(cnd_t *cond);

/** Signal a condition variable.
* Unblocks one of the threads that are blocked on the given condition variable
* at the time of the call. If no threads are blocked on the condition variable
* at the time of the call, the function does nothing and return success.
* @param cond A condition variable object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int cnd_signal(cnd_t *cond);

/** Broadcast a condition variable.
* Unblocks all of the threads that are blocked on the given condition variable
* at the time of the call. If no threads are blocked on the condition variable
* at the time of the call, the function does nothing and return success.
* @param cond A condition variable object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int cnd_broadcast(cnd_t *cond);

/** Wait for a condition variable to become signaled.
* The function atomically unlocks the given mutex and endeavors to block until
* the given condition variable is signaled by a call to cnd_signal or to
* cnd_broadcast. When the calling thread becomes unblocked it locks the mutex
* before it returns.
* @param cond A condition variable object.
* @param mtx A mutex object.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int cnd_wait(cnd_t *cond, mtx_t *mtx);

/** Wait for a condition variable to become signaled.
* The function atomically unlocks the given mutex and endeavors to block until
* the given condition variable is signaled by a call to cnd_signal or to
* cnd_broadcast, or until after the specified time. When the calling thread
* becomes unblocked it locks the mutex before it returns.
* @param cond A condition variable object.
* @param mtx A mutex object.
* @param ts A point in time at which the request will time out (absolute time).
* @return @ref thrd_success upon success, or @ref thrd_timeout if the time
* specified in the call was reached without acquiring the requested resource, or
* @ref thrd_error if the request could not be honored.
*/
int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts);

/* Thread */
typedef pthread_t thrd_t;

/** Thread start function.
* Any thread that is started with the @ref thrd_create() function must be
* started through a function of this type.
* @param arg The thread argument (the @c arg argument of the corresponding
*        @ref thrd_create() call).
* @return The thread return value, which can be obtained by another thread
* by using the @ref thrd_join() function.
*/
typedef int (*thrd_start_t)(void *arg);

/** Create a new thread.
* @param thr Identifier of the newly created thread.
* @param func A function pointer to the function that will be executed in
*        the new thread.
* @param arg An argument to the thread function.
* @return @ref thrd_success on success, or @ref thrd_nomem if no memory could
* be allocated for the thread requested, or @ref thrd_error if the request
* could not be honored.
* @note A thread’s identifier may be reused for a different thread once the
* original thread has exited and either been detached or joined to another
* thread.
*/
int thrd_create(thrd_t *thr, thrd_start_t func, void *arg);

/** Identify the calling thread.
* @return The identifier of the calling thread.
*/
thrd_t thrd_current(void);

/** Dispose of any resources allocated to the thread when that thread exits.
 * @return thrd_success, or thrd_error on error
*/
int thrd_detach(thrd_t thr);

/** Compare two thread identifiers.
* The function determines if two thread identifiers refer to the same thread.
* @return Zero if the two thread identifiers refer to different threads.
* Otherwise a nonzero value is returned.
*/
int thrd_equal(thrd_t thr0, thrd_t thr1);

/** Terminate execution of the calling thread.
* @param res Result code of the calling thread.
*/
CTHREAD_NORETURN void thrd_exit(int res);

/** Wait for a thread to terminate.
* The function joins the given thread with the current thread by blocking
* until the other thread has terminated.
* @param thr The thread to join with.
* @param res If this pointer is not NULL, the function will store the result
*        code of the given thread in the integer pointed to by @c res.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int thrd_join(thrd_t thr, int *res);

/** Put the calling thread to sleep.
* Suspend execution of the calling thread.
* @param duration  Interval to sleep for
* @param remaining If non-NULL, this parameter will hold the remaining
*                  time until time_point upon return. This will
*                  typically be zero, but if the thread was woken up
*                  by a signal that is not ignored before duration was
*                  reached @c remaining will hold a positive time.
* @return 0 (zero) on successful sleep, -1 if an interrupt occurred,
*         or a negative value if the operation fails.
*/
int thrd_sleep(const struct timespec *duration, struct timespec *remaining);

/** Yield execution to another thread.
* Permit other threads to run, even if the current thread would ordinarily
* continue to run.
*/
void thrd_yield(void);

/* Thread local storage */
typedef pthread_key_t tss_t;

/** Destructor function for a thread-specific storage.
* @param val The value of the destructed thread-specific storage.
*/
typedef void (*tss_dtor_t)(void *val);

/** Create a thread-specific storage.
* @param key The unique key identifier that will be set if the function is
*        successful.
* @param dtor Destructor function. This can be NULL.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
* @note On Windows, the @c dtor will definitely be called when
* appropriate for threads created with @ref thrd_create.  It will be
* called for other threads in most cases, the possible exception being
* for DLLs loaded with LoadLibraryEx.  In order to be certain, you
* should use @ref thrd_create whenever possible.
*/
int tss_create(tss_t *key, tss_dtor_t dtor);

/** Delete a thread-specific storage.
* The function releases any resources used by the given thread-specific
* storage.
* @param key The key that shall be deleted.
*/
void tss_delete(tss_t key);

/** Get the value for a thread-specific storage.
* @param key The thread-specific storage identifier.
* @return The value for the current thread held in the given thread-specific
* storage.
*/
void *tss_get(tss_t key);

/** Set the value for a thread-specific storage.
* @param key The thread-specific storage identifier.
* @param val The value of the thread-specific storage to set for the current
*        thread.
* @return @ref thrd_success on success, or @ref thrd_error if the request could
* not be honored.
*/
int tss_set(tss_t key, void *val);

#define once_flag pthread_once_t
#define ONCE_FLAG_INIT PTHREAD_ONCE_INIT

/** Invoke a callback exactly once
 * @param flag Flag used to ensure the callback is invoked exactly
 *        once.
 * @param func Callback to invoke.
 */
#define call_once(flag,func) pthread_once(flag,func)

#ifdef __cplusplus
}
#endif
#endif /* HAS_C11_THREADS */
#endif /* _CTHREAD_H_ */

#if defined(__APPLE__) || defined(__MACH__)
#include <mach/clock.h>
#include <mach/mach.h>
int timespec_get(struct timespec *ts, int base);
#endif

#ifndef _CTHREAD_EXTRA_H_
#define _CTHREAD_EXTRA_H_

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(ZE_MALLOC) || !defined(ZE_FREE) || !defined(ZE_REALLOC)|| !defined(ZE_CALLOC)
#include "rpmalloc.h"
#define ZE_MALLOC malloc
#define ZE_FREE free
#define ZE_REALLOC realloc
#define ZE_CALLOC calloc
#define ZE_MEMALIGN memalign
#endif

#ifndef TIME_UTC
#define TIME_UTC 1
#endif

    /* Public API qualifier. */
#ifndef C_API
#define C_API extern
#endif

#ifndef thrd_local
#if defined(emulate_tls) || defined(__TINYC__)
#   define thrd_local_return(type, var)    return (type *)tss_get(thrd_##var##_tss);
#   define thrd_local_get(type, var, initial, prefix)   \
        prefix type* var(void) {                        \
            if (thrd_##var##_tls == 0) {	            \
                thrd_##var##_tls = sizeof(type);        \
                if (tss_create(&thrd_##var##_tss, ZE_FREE) == thrd_success)	\
                    atexit(var##_del);      \
                else                        \
                      goto err;			    \
            }								\
            void *ptr = tss_get(thrd_##var##_tss);  \
            if (ptr == NULL) {                      \
                ptr = ZE_MALLOC(thrd_##var##_tls); \
                if (ptr == NULL)		    \
                    goto err;			    \
                if ((tss_set(thrd_##var##_tss, ptr)) != thrd_success)	\
                    goto err;			    \
            }                               \
            return (type *)ptr;             \
        err:                                \
            return NULL;                    \
        }

#   define thrd_local_del(type, var, initial, prefix)   \
        prefix void var##_del(void) {       \
            if (thrd_##var##_tls != 0) {    \
                thrd_##var##_tls = 0;       \
                ZE_FREE(tss_get(thrd_##var##_tss));    \
                tss_delete(thrd_##var##_tss);           \
                thrd_##var##_tss = -1;      \
            }                               \
        }

#   define thrd_local_setup(type, var, initial, prefix)     \
        static type thrd_##var##_buffer;                    \
        prefix int thrd_##var##_tls = 0;                    \
        prefix tss_t thrd_##var##_tss = 0;                  \
        thrd_local_del(type, var, initial, prefix)          \
        prefix FORCEINLINE void var##_update(type *value) {  \
            *var() = *value;                                 \
        }                                                   \
        prefix FORCEINLINE bool is_##var##_empty(void) {    \
            return (type *)tss_get(thrd_##var##_tss) == (type *)initial;     \
        }

    /* Initialize and setup thread local storage `var name` as functions. */
#   define thrd_local(type, var, initial)       \
        thrd_local_setup(type, var, initial, )  \
        thrd_local_get(type, var, initial, )

#   define thrd_local_plain(type, var, initial) \
        thrd_local_setup(type, var, initial, )  \
        thrd_local_get(type, var, initial, )

#   define thrd_static(type, var, initial)      \
        static type *var(void);                 \
        static void var##_del(void);            \
        static bool is_##var##_empty(void);     \
        thrd_local_setup(type, var, initial, static)  \
        thrd_local_get(type, var, initial, static)

#   define thrd_static_plain(type, var, initial)    thrd_static(type, var, initial)

#   define thrd_local_proto(type, var, prefix)  \
        prefix int thrd_##var##_tls;            \
        prefix tss_t thrd_##var##_tss;          \
        prefix type var(void);                 \
        prefix void var##_del(void);            \
        prefix void var##_update(type value);  \
        prefix bool is_##var##_empty(void);

    /* Creates a `extern` compile time emulated/explict thread-local storage variable, pointer of type */
#   define thrd_local_extern(type, var) thrd_local_proto(type *, var, C_API)
    /* Creates a `extern` compile time emulated/explict thread-local storage variable, non-pointer of type */
#   define thrd_local_external(type, var) thrd_local_proto(type, var, C_API)
#else
#   define thrd_local_return(type, var)    return (type)thrd_##var##_tls;
#   define thrd_local_get(type, var, initial, prefix)       \
        prefix FORCEINLINE type var(void) {     \
            if (thrd_##var##_tls == initial) {  \
                thrd_##var##_tls = &thrd_##var##_buffer;    \
                atexit(var##_del);              \
            }                                   \
            thrd_local_return(type, var)        \
        }

#   define thrd_local_setup(type, var, initial, prefix)         \
        prefix thread_local type thrd_##var##_tls = initial;    \
        prefix FORCEINLINE void var##_del(void) {               \
            thrd_##var##_tls = NULL;                            \
        }                                                       \
        prefix FORCEINLINE void var##_update(type value) {      \
            thrd_##var##_tls = value;                           \
        }                                                       \
        prefix FORCEINLINE bool is_##var##_empty(void) {        \
            return thrd_##var##_tls == initial;                 \
        }

    /* Initialize and setup thread local storage `var name` as functions. */
#   define thrd_local(type, var, initial)               \
        static thread_local type thrd_##var##_buffer;   \
        thrd_local_setup(type *, var, initial, )        \
        thrd_local_get(type *, var, initial, )

#   define thrd_local_plain(type, var, initial)         \
        static thread_local type thrd_##var##_buffer;   \
        thrd_local_setup(type, var, initial, )          \
        thrd_local_get(type, var, initial, )

#   define thrd_static(type, var, initial)              \
        static thread_local type thrd_##var##_buffer;   \
        thrd_local_setup(type *, var, initial, static)  \
        thrd_local_get(type *, var, initial, static)

#   define thrd_static_plain(type, var, initial)        \
        static thread_local type thrd_##var##_buffer;   \
        thrd_local_setup(type, var, initial, static)    \
        thrd_local_get(type, var, initial, static)

#   define thrd_local_proto(type, var, prefix)          \
        prefix thread_local type thrd_##var##_tls;      \
        prefix void var##_del(void);                    \
        prefix void var##_update(type value);           \
        prefix bool is_##var##_empty(void);             \
        prefix type var(void);

    /* Creates a native `extern` compile time thread-local storage variable, pointer of type */
#   define thrd_local_extern(type, var) thrd_local_proto(type *, var, C_API)
    /* Creates a native `extern` compile time thread-local storage variable, non-pointer of type */
#   define thrd_local_external(type, var) thrd_local_proto(type, var, C_API)
#endif
#endif /* thrd_local */

#ifdef __cplusplus
}
#endif
#endif /* _CTHREAD_EXTRA_H_ */
