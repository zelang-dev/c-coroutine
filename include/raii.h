#ifndef RAII_H
#define RAII_H

#include "swar.h"
#include "cthread.h"
#include "catomic.h"
#include <stdio.h>
#include <time.h>

#ifndef O_BINARY
#   define O_BINARY 0
#endif

#ifdef __cplusplus
    extern "C" {
#endif

/* Smart memory pointer, the allocated memory requested in `arena` field,
all other fields private, this object binds any additional requests to it's lifetime. */
typedef struct memory_s memory_t;
typedef struct _future *future;
typedef struct future_pool *future_t;
typedef struct future_deque future_deque_t;
typedef memory_t unique_t;
typedef void (*func_t)(void_t);
typedef void (*func_args_t)(void_t, ...);
typedef void_t(*raii_func_args_t)(void_t, ...);
typedef void_t(*raii_func_t)(void_t);
typedef intptr_t(*raii_callable_t)(intptr_t);
typedef uintptr_t(*raii_callable_args_t)(uintptr_t, ...);
typedef uintptr_t *(*raii_callable_const_t)(const char *, ...);
typedef enum {
    RAII_NULL,
    RAII_INT,
    RAII_ENUM,
    RAII_INTEGER,
    RAII_UINT,
    RAII_SLONG,
    RAII_ULONG,
    RAII_LLONG,
    RAII_MAXSIZE,
    RAII_FLOAT,
    RAII_DOUBLE,
    RAII_BOOL,
    RAII_SHORT,
    RAII_USHORT,
    RAII_CHAR,
    RAII_UCHAR,
    RAII_UCHAR_P,
    RAII_CHAR_P,
    RAII_CONST_CHAR,
    RAII_STRING,
    RAII_ARRAY,
    RAII_HASH,
    RAII_OBJ,
    RAII_PTR,
    RAII_FUNC,
    RAII_NAN,
    RAII_DEF_ARR,
    RAII_DEF_FUNC,
    RAII_ROUTINE,
    RAII_OA_HASH,
    RAII_REFLECT_TYPE,
    RAII_REFLECT_INFO,
    RAII_REFLECT_VALUE,
    RAII_MAP_VALUE,
    RAII_MAP_STRUCT,
    RAII_MAP_ITER,
    RAII_MAP_ARR,
    RAII_ERR_PTR,
    RAII_ERR_CONTEXT,
    RAII_PROMISE,
    RAII_FUTURE,
    RAII_FUTURE_ARG,
    RAII_EVENT_ARG,
    RAII_ARGS,
    RAII_PROCESS,
    RAII_SCHED,
    RAII_CHANNEL,
    RAII_STRUCT,
    RAII_UNION,
    RAII_VALUE,
    RAII_NO_INSTANCE,
    RAII_ARENA,
    RAII_THREAD,
    RAII_GUARDED_STATUS,
    RAII_VECTOR,
    RAII_QUEUE,
    RAII_SPAWN,
    RAII_POOL,
    RAII_SYNC,
    RAII_COUNT
} raii_type;

enum {
    RAII_OK = 0,
    RAII_ERR = -1,
};

/* Generic simple union storage types. */
typedef union {
    int integer;
    unsigned int u_int;
    int *int_ptr;
    signed long s_long;
    unsigned long u_long;
    long long long_long;
    size_t max_size;
    uintptr_t ulong_long;
    thrd_t thread;
    float point;
    double precision;
    bool boolean;
    signed short s_short;
    unsigned short u_short;
    unsigned short *u_short_ptr;
    unsigned char *uchar_ptr;
    signed char schar;
    unsigned char uchar;
    char *char_ptr;
    void_t object;
    ptrdiff_t **array;
    char **array_char;
    intptr_t **array_int;
    uintptr_t **array_uint;
    raii_func_t func;
    char buffer[256];
} values_type, *vectors_t, *args_t;

typedef struct {
    values_type value;
    size_t size;
    void_t extended;
} raii_values_t;

typedef struct {
    raii_type type;
    void_t value;
} var_t;

typedef struct {
    raii_type type;
    void_t value;
    func_t dtor;
} object_t;

typedef struct {
    raii_type type;
    void_t base;
    size_t elements;
} raii_array_t;

typedef struct {
    raii_type type;
    raii_array_t base;
} defer_t;

typedef struct {
    raii_type type;
    void (*func)(void_t);
    void_t data;
    void_t check;
} defer_func_t;

struct memory_s {
    void_t arena;
    bool is_recovered;
    bool is_protected;
    bool is_emulated;
    int threading;
    int status;
    size_t mid;
    defer_t defer;
    future_t threaded;
    memory_t *local;
    ex_ptr_t *protector;
    ex_backtrace_t *backtrace;
    void_t volatile err;
    const char *volatile panic;
    future_deque_t *queued;
};

typedef void (*for_func_t)(i64, i64);
typedef void_t (*thrd_func_t)(args_t);
typedef void_t (*result_func_t)(void_t result, size_t id, values_type iter);
typedef void (*wait_func)(void);
typedef struct _promise {
    raii_type type;
    int id;
    atomic_flag done;
    atomic_spinlock mutex;
    memory_t *scope;
    raii_values_t *result;
} promise;

typedef struct _future_arg worker_t;
struct _future_arg {
    raii_type type;
    int id;
    unique_t *local;
    void_t arg;
    thrd_func_t func;
    promise *value;
    future_deque_t *queue;
};
make_atomic(worker_t, atomic_worker_t)

typedef struct {
    atomic_size_t size;
    atomic_worker_t buffer[];
} future_array_t;
make_atomic(future_array_t *, atomic_future_t)

typedef struct result_data {
    raii_type type;
    bool is_ready;
    size_t id;
    raii_values_t *result;
} *result_t;
make_atomic(result_t, atomic_result_t)

/* Calls fn (with args as arguments) in separate thread, returning without waiting
for the execution of fn to complete. The value returned by fn can be accessed
by calling `thrd_get()`. */
C_API future thrd_async(thrd_func_t fn, void_t args);

/* Returns the value of `future` ~promise~, a thread's shared object, If not ready, this
function blocks the calling thread and waits until it is ready. */
C_API values_type thrd_get(future);

/* This function blocks the calling thread and waits until `future` is ready,
will execute provided `yield` callback function continuously. */
C_API void thrd_wait(future, wait_func yield);

/* Check status of `future` object state, if `true` indicates thread execution has ended,
any call thereafter to `thrd_get` is guaranteed non-blocking. */
C_API bool thrd_is_done(future);
C_API void thrd_delete(future);
C_API uintptr_t thrd_self(void);
C_API size_t thrd_cpu_count(void);


/* Return `value` any heap allocated instance/struct,
only available in `thread` using `args_for`, DO NOT FREE! */
C_API raii_values_t *thrd_returning(args_t, void_t value, size_t size);
C_API raii_values_t *thrd_data(void_t value);
C_API raii_values_t *thrd_value(uintptr_t value);

C_API void thrd_init(size_t queue_size);
C_API future_t thrd_scope(void);
C_API future_t thrd_sync(future_t);
C_API result_t thrd_spawn(thrd_func_t fn, void_t args);
C_API values_type thrd_result(result_t value);

C_API future_t thrd_for(for_func_t loop, intptr_t initial, intptr_t times);

C_API void thrd_then(result_func_t callback, future_t iter, void_t result);
C_API void thrd_destroy(future_t);
C_API bool thrd_is_finish(future_t);

C_API memory_t *raii_local(void);
/* Return current `thread` smart memory pointer. */
C_API memory_t *raii_init(void);

/* Return current `context` ~scope~ smart pointer, in use! */
C_API memory_t *get_scope(void);

C_API void raii_unwind_set(ex_context_t *ctx, const char *ex, const char *message);
C_API int raii_deferred_init(defer_t *array);

C_API size_t raii_mid(void);
C_API size_t raii_last_mid(memory_t *scope);

/* Defer execution `LIFO` of given function with argument,
to current `thread` scope lifetime/destruction. */
C_API size_t raii_defer(func_t, void_t);

/* Defer execution `LIFO` of given function with argument,
execution begins when current `context` scope exits or panic/throw. */
C_API size_t deferring(func_t func, void_t data);

C_API void raii_defer_cancel(size_t index);
C_API void raii_deferred_cancel(memory_t *scope, size_t index);

C_API void raii_defer_fire(size_t index);
C_API void raii_deferred_fire(memory_t *scope, size_t index);

/* Same as `raii_defer` but allows recover from an Error condition throw/panic,
you must call `raii_caught` inside function to mark Error condition handled. */
C_API void raii_recover(func_t, void_t);

/* Same as `defer` but allows recover from an Error condition throw/panic,
you must call `raii_is_caught` inside function to mark Error condition handled. */
C_API void raii_recover_by(memory_t *, func_t, void_t);

/* Compare `err` to current error condition, will mark exception handled, if `true`. */
C_API bool raii_caught(const char *err);

/* Compare `err` to scoped error condition, will mark exception handled, if `true`. */
C_API bool raii_is_caught(memory_t *scope, const char *err);

/* Compare `err` to scoped error condition, will mark exception handled, if `true`.
Only valid between `guard` blocks or inside ~c++11~ like `thread/future` call. */
C_API bool is_recovered(const char *err);

/* Get current error condition string. */
C_API const char *raii_message(void);

/* Get scoped error condition string. */
C_API const char *raii_message_by(memory_t *scope);

/* Get scoped error condition string.
Only valid between `guard` blocks or inside ~c++11~ like `thread/future` call. */
C_API const char *err_message(void);

/* Defer execution `LIFO` of given function with argument,
to the given `scoped smart pointer` lifetime/destruction. */
C_API size_t raii_deferred(memory_t *, func_t, void_t);
C_API size_t raii_deferred_count(memory_t *);

/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be freed with given `func`, when scope smart pointer panics/returns/exits. */
C_API void_t malloc_full(memory_t *scope, size_t size, func_t func);

/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be freed with given `func`, when scope smart pointer panics/returns/exits. */
C_API void_t calloc_full(memory_t *scope, int count, size_t size, func_t func);

/* Same as `raii_deferred_free`, but also destroy smart pointer. */
C_API void raii_delete(memory_t *ptr);

/* Same as `raii_deferred_clean`, but also
reset/clear current `thread` smart pointer. */
C_API void raii_destroy(void);

/* Begin `unwinding`, executing given scope smart pointer `raii_deferred` statements. */
C_API void raii_deferred_free(memory_t *);

/* Begin `unwinding`, executing current `thread` scope `raii_defer` statements. */
C_API void raii_deferred_clean(void);

C_API void_t raii_memdup(const_t src, size_t len);
C_API string *raii_split(string_t s, string_t delim, int *count);
C_API string raii_concat(int num_args, ...);
C_API string raii_replace(string_t haystack, string_t needle, string_t replace);
C_API u_string raii_encode64(u_string_t src);
C_API u_string raii_decode64(u_string_t src);

/* Creates smart memory pointer, this object binds any additional requests to it's lifetime.
for use with `malloc_*` `calloc_*` wrapper functions to request/return raw memory. */
C_API unique_t *unique_init(void);

/* Returns protected raw memory pointer of given `size`,
DO NOT FREE, will `throw/panic` if memory request fails.
This uses current `thread` smart pointer, unless called
between `guard` blocks, or inside ~c++11~ like `thread/future` call. */
C_API void_t malloc_local(size_t size);

/* Returns protected raw memory pointer of given `size`,
DO NOT FREE, will `throw/panic` if memory request fails.
This uses current `thread` smart pointer, unless called
between `guard` blocks, or inside ~c++11~ like `thread/future` call. */
C_API void_t calloc_local(int count, size_t size);

C_API values_type raii_value(void_t);
C_API raii_type type_of(void_t);
C_API bool is_type(void_t, raii_type);
C_API bool is_void(void_t);
C_API bool is_instance_of(void_t, void_t);
C_API bool is_value(void_t);
C_API bool is_instance(void_t);
C_API bool is_valid(void_t);
C_API bool is_zero(size_t);
C_API bool is_empty(void_t);
C_API bool is_true(bool);
C_API bool is_false(bool);
C_API bool is_str_in(const char *text, char *pattern);
C_API bool is_str_eq(const char *str, const char *str2);
C_API bool is_str_empty(const char *str);
C_API bool is_guard(void_t self);

C_API void_t try_calloc(int, size_t);
C_API void_t try_malloc(size_t);
C_API void_t try_realloc(void_t, size_t);

C_API void guard_set(ex_context_t *ctx, const char *ex, const char *message);
C_API void guard_reset(void_t scope, ex_setup_func set, ex_unwind_func unwind);
C_API void guard_delete(memory_t *ptr);

#ifndef MAX
# define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
# define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define NONE

/* Returns protected raw memory pointer,
DO NOT FREE, will `throw/panic` if memory request fails. */
#define _malloc(size)           malloc_local(size)

/* Returns protected raw memory pointer,
DO NOT FREE, will `throw/panic` if memory request fails. */
#define _calloc(count, size)    calloc_local(count, size)

/* Defer execution `LIFO` of given function with argument,
execution begins when current `context` scope exits or panic/throw. */
#define _defer(func, ptr)       deferring((func_t)func, ptr)

/* Compare `err` to scoped error condition, will mark exception handled, if `true`. */
#define _recover(err)           is_recovered(err)

/* Compare `err` to scoped error condition,
will mark exception handled, if `true`.
DO NOT PUT `err` in quote's like "err". */
#define _is_caught(err)         is_recovered(EX_STR(err))

/* Get scoped error condition string. */
#define _get_message()  err_message()

/* Stops the ordinary flow of control and begins panicking,
throws an exception of given message. */
#define _panic          raii_panic

/* Makes a reference assignment of current scoped smart pointer. */
#define _assign_ptr(scope)      unique_t *scope = _$##__FUNCTION__

/* Exit `guarded` section, begin executing deferred functions,
return given `value` when done, use `NONE` for no return. */
#define _return(value)              \
    raii_delete(_$##__FUNCTION__);  \
    guard_reset(s##__FUNCTION__, sf##__FUNCTION__, uf##__FUNCTION__);   \
    ex_update(ex_err.next);         \
    if (ex_local() == &ex_err)      \
        ex_update(ex_err.next);     \
    return value;

/* Creates an scoped guard section, it replaces `{`.
Usage of: `_defer`, `_malloc`, `_calloc`, `_assign_ptr` macros
are only valid between these sections.
    - Use `_return(x);` macro, or `break;` to exit early.
    - Use `_assign_ptr(var)` macro, to make assignment of block scoped smart pointer. */
#define guard                                           \
{                                                       \
    if (!exception_signal_set)                          \
        ex_signal_setup();                              \
    void_t s##__FUNCTION__ = raii_init()->arena;         \
    ex_setup_func sf##__FUNCTION__ = exception_setup_func;      \
    ex_unwind_func uf##__FUNCTION__ = exception_unwind_func;    \
    exception_unwind_func = (ex_unwind_func)raii_deferred_free; \
    exception_setup_func = guard_set;                   \
    unique_t *_$##__FUNCTION__ = unique_init();         \
    (_$##__FUNCTION__)->status = RAII_GUARDED_STATUS;   \
    raii_local()->arena = (void_t)_$##__FUNCTION__;     \
    ex_try {                                            \
        do {

    /* This ends an scoped guard section, it replaces `}`.
    On exit will begin executing deferred functions,
    return given `result` when done, use `NONE` for no return. */
#define unguarded(result)                                           \
            } while (false);                                        \
            raii_deferred_free(_$##__FUNCTION__);                   \
    } ex_catch_if {                                                 \
        if ((is_type(&(_$##__FUNCTION__)->defer, RAII_DEF_ARR)))    \
            raii_deferred_free(_$##__FUNCTION__);                   \
    } ex_finally {                                                  \
        guard_reset(s##__FUNCTION__, sf##__FUNCTION__, uf##__FUNCTION__);   \
        guard_delete(_$##__FUNCTION__);                             \
    } ex_end_try;                                                   \
    return result;                                                  \
}

    /* This ends an scoped guard section, it replaces `}`.
    On exit will begin executing deferred functions. */
#define guarded                                                     \
        } while (false);                                            \
        raii_deferred_free(_$##__FUNCTION__);                       \
    } ex_catch_if {                                                 \
        raii_deferred_free(_$##__FUNCTION__);                       \
    } ex_finally {                                                  \
        guard_reset(s##__FUNCTION__, sf##__FUNCTION__, uf##__FUNCTION__);   \
        guard_delete(_$##__FUNCTION__);                             \
    } ex_end_try;                                                   \
}


    /* This ends an scoped guard section, it replaces `}`.
    On exit will begin executing deferred functions. Will catch and set `error`
    this is an internal macro for `thrd_spawn` and `thrd_async` usage. */
#define guarded_exception(error)                                    \
        } while (false);                                            \
        raii_deferred_free(_$##__FUNCTION__);                       \
    } ex_catch_if {                                                 \
        raii_deferred_free(_$##__FUNCTION__);                       \
        if (!_$##__FUNCTION__->is_recovered && raii_is_caught(_$##__FUNCTION__, raii_message_by(_$##__FUNCTION__))) { \
            ((memory_t *)error)->err = (void_t)ex_err.ex;           \
            ((memory_t *)error)->panic = ex_err.panic;              \
            ((memory_t *)error)->backtrace = ex_err.backtrace;      \
        }                                                           \
    } ex_finally{\
        guard_reset(s##__FUNCTION__, sf##__FUNCTION__, uf##__FUNCTION__);   \
        guard_delete(_$##__FUNCTION__);                             \
    } ex_end_try;                                                   \
}


#define block(type, name, ...)          \
    type name(__VA_ARGS__)              \
    guard

#define unblocked(result)               \
    unguarded(result)

#define blocked                         \
    guarded

#define try ex_try
#define catch_any ex_catch_any
#define catch_if ex_catch_if
#define catch(e) ex_catch(e)
#define end_try ex_end_try
#define finally ex_finally
#define caught(err) raii_caught(EX_STR(err))

#ifdef _WIN32
    #define finality ex_finality
    #define end_trying ex_end_trying
#else
    #define finality catch_any ex_finally
    #define end_trying ex_end_try
#endif

#define time_spec(sec, nsec) &(struct timespec){ .tv_sec = sec ,.tv_nsec = nsec }

thrd_local_extern(memory_t, raii)
thrd_local_extern(ex_context_t, except)
C_API const raii_values_t raii_values_empty[1];

C_API string str_memdup_ex(memory_t *defer, const_t src, size_t len);
C_API string str_copy(string dest, string_t src, size_t len);
C_API string *str_split_ex(memory_t *defer, string_t s, string_t delim, int *count);
C_API string str_concat_ex(memory_t *defer, int num_args, va_list ap_copy);
C_API string str_replace_ex(memory_t *defer, string_t haystack, string_t needle, string_t replace);
C_API u_string str_encode64_ex(memory_t *defer, u_string_t src);
C_API u_string str_decode64_ex(memory_t *defer, u_string_t src);
C_API bool is_base64(u_string_t src);
C_API int strpos(const char *text, char *pattern);

C_API vectors_t vector_variant(void);
C_API vectors_t vector_for(vectors_t, size_t, ...);
C_API void vector_insert(vectors_t vec, int pos, void_t val);
C_API void vector_clear(vectors_t);
C_API void vector_erase(vectors_t, int);
C_API size_t vector_size(vectors_t);
C_API size_t vector_capacity(vectors_t);
C_API memory_t *vector_scope(vectors_t);
C_API void vector_push_back(vectors_t, void_t);

/**
* Creates an scoped `vector/array/container` for arbitrary arguments passing into an single `paramater` function.
* - Use standard `array access` for retrieval of an `union` storage type.
*
* - MUST CALL `args_destructor_set()` to have memory auto released
*   within ~callers~ current scoped `context`, will happen either at return/exist or panics.
*
* - OTHERWISE `memory leak` will be shown in DEBUG build.
*
* NOTE: `vector_for` does auto memory cleanup.
*
* @param count numbers of parameters, `0` will create empty `vector/array`.
* @param arguments indexed in given order.
*/
C_API args_t args_for(size_t, ...);
C_API void args_destructor_set(args_t);
C_API void args_deferred_set(args_t, memory_t *);
C_API void args_returning_set(args_t);
C_API bool is_args(args_t);
C_API bool is_args_returning(args_t);
C_API values_type get_arg(void_t);

#define array(count, ...) args_for(count, __VA_ARGS__)
#define array_defer(arr) args_destructor_set(arr)
#define vectorize(vec) vectors_t vec = vector_variant()
#define vector(vec, count, ...) vectors_t vec = vector_for(nil, count, __VA_ARGS__)

#define $push_back(vec, value) vector_push_back(vec, (void_t)value)
#define $insert(vec, index, value) vector_insert(vec, index, (void_t)value)
#define $clear(vec) vector_clear(vec)
#define $size(vec) vector_size(vec)
#define $capacity(vec) vector_capacity(vec)
#define $erase(vec, index) vector_erase(vec, index)

#define in ,
#define foreach_xp(X, A) X A
#define foreach_in(X, S) values_type X; int i_##X;  \
    for (i_##X = 0; i_##X < $size(S); i_##X++)      \
        if ((X.object = S[i_##X].object) || X.object == nullptr)
#define foreach_in_back(X, S) values_type X; int i_##X, e_##X = $size(S) - 1;   \
    for (i_##X = e_##X; i_##X >= 0; i_##X--)        \
        if ((X.object = S[i_##X].object) || X.object == nullptr)
#define foreach(...) foreach_xp(foreach_in, (__VA_ARGS__))
#define foreach_back(...) foreach_xp(foreach_in_back, (__VA_ARGS__))

#ifdef __cplusplus
    }
#endif
#endif /* RAII_H */
