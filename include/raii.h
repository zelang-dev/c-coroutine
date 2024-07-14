#ifndef RAII_H
#define RAII_H

#include "rtypes.h"
#include "exception.h"
#include "cthread.h"
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
    extern "C" {
#endif

/* Smart memory pointer, the allocated memory requested in `arena` field,
all other fields private, this object binds any additional requests to it's lifetime. */
typedef struct memory_s memory_t;
typedef memory_t unique_t;
typedef void (*func_t)(void *);
typedef void (*func_args_t)(void *, ...);
typedef void *(*raii_func_t)(void *);
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
    RAII_QUEUE,
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
    signed long s_long;
    unsigned long u_long;
    long long long_long;
    size_t max_size;
    thrd_t thread;
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
    void *object;
    raii_func_t func;
    const char const_char[128];
} values_type;

typedef struct {
    values_type value;
    raii_type type;
} raii_values_t;

typedef struct {
    raii_type type;
    void *value;
} var_t;

typedef struct {
    raii_type type;
    void *value;
    func_t dtor;
} object_t;

typedef struct {
    raii_type type;
    void *base;
    size_t elements;
} raii_array_t;

typedef struct {
    raii_type type;
    raii_array_t base;
} defer_t;

typedef struct {
    raii_type type;
    void (*func)(void *);
    void *data;
    void *check;
} defer_func_t;

struct memory_s {
    void *arena;
    int status;
    bool is_recovered;
    void *volatile err;
    const char *volatile panic;
    bool is_protected;
    bool is_arena;
    bool is_emulated;
    ex_ptr_t *protector;
    defer_t defer;
    size_t mid;
};

typedef struct args_s {
    raii_type type;
    /* allocated array of arguments */
    raii_values_t *args;
    unique_t *context;

    /* total number of args in set */
    size_t n_args;
    bool defer_set;
} args_t;

/**
* `Release/free` allocated memory, must be called if not using `get_args()` function.
*
* @param params arbitrary arguments
*/
C_API void args_free(args_t *params);

/**
* Creates an scoped container for arbitrary arguments passing to an single `args` function.
* Use `get_args()` or `args_in()` for retrieval.
*
* @param scope callers context to bind `allocated` arguments to
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
C_API args_t *raii_args_for(memory_t *scope, const char *desc, ...);

/**
* Returns generic union `values_type` of argument, will auto `release/free`
* allocated memory when scoped return/exit.
*
* Must be called at least once to release `allocated` memory.
*
* @param params arbitrary arguments
* @param item index number
*/
C_API values_type get_args(void *params, int item);

/**
* Returns generic union `values_type` of argument.
*
* @param params arguments instance
* @param index item number
*/
C_API values_type args_in(args_t *params, int index);

C_API memory_t *raii_local(void);
/* Return current `thread` smart memory pointer. */
C_API memory_t *raii_init(void);
C_API void raii_unwind_set(ex_context_t *ctx, const char *ex, const char *message);
C_API int raii_deferred_init(defer_t *array);

C_API size_t raii_mid(void);
C_API size_t raii_last_mid(memory_t *scope);

/* Defer execution `LIFO` of given function with argument,
to current `thread` scope lifetime/destruction. */
C_API size_t raii_defer(func_t, void *);

C_API void raii_defer_cancel(size_t index);
C_API void raii_deferred_cancel(memory_t *scope, size_t index);

C_API void raii_defer_fire(size_t index);
C_API void raii_deferred_fire(memory_t *scope, size_t index);

/* Same as `raii_defer` but allows recover from an Error condition throw/panic,
you must call `raii_caught` inside function to mark Error condition handled. */
C_API void raii_recover(func_t, void *);

/* Same as `defer` but allows recover from an Error condition throw/panic,
you must call `raii_is_caught` inside function to mark Error condition handled. */
C_API void raii_recover_by(memory_t *, func_t, void *);

/* Compare `err` to current error condition, will mark exception handled, if `true`. */
C_API bool raii_caught(const char *err);

/* Compare `err` to scoped error condition, will mark exception handled, if `true`. */
C_API bool raii_is_caught(memory_t *scope, const char *err);

/* Get current error condition string. */
C_API const char *raii_message(void);

/* Get scoped error condition string. */
C_API const char *raii_message_by(memory_t *scope);

/* Defer execution `LIFO` of given function with argument,
to the given `scoped smart pointer` lifetime/destruction. */
C_API size_t raii_deferred(memory_t *, func_t, void *);
C_API size_t raii_deferred_count(memory_t *);

/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be freed with given `func`, when scope smart pointer panics/returns/exits. */
C_API void *malloc_full(memory_t *scope, size_t size, func_t func);

/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be freed when scope smart pointer panics/returns/exits. */
C_API void *malloc_by(memory_t *scope, size_t size);

/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be freed with given `func`, when scope smart pointer panics/returns/exits. */
C_API void *calloc_full(memory_t *scope, int count, size_t size, func_t func);

/* Request/return raw memory of given `size`, using smart memory pointer's lifetime scope handle.
DO NOT `free`, will be freed when scope smart pointer panics/returns/exits. */
C_API void *calloc_by(memory_t *scope, int count, size_t size);

/* Same as `raii_deferred_free`, but also destroy smart pointer. */
C_API void raii_delete(memory_t *ptr);

/* Same as `raii_deferred_clean`, but also
reset/clear current `thread` smart pointer. */
C_API void raii_destroy(void);

/* Begin `unwinding`, executing given scope smart pointer `raii_deferred` statements. */
C_API void raii_deferred_free(memory_t *);

/* Begin `unwinding`, executing current `thread` scope `raii_defer` statements. */
C_API void raii_deferred_clean(void);

/* Creates smart memory pointer, this object binds any additional requests to it's lifetime.
for use with `malloc_*` `calloc_*` wrapper functions to request/return raw memory. */
C_API unique_t *unique_init(void);

C_API unique_t *unique_init_arena(void);
C_API void *calloc_arena(memory_t *scope, int count, size_t size);
C_API void *malloc_arena(memory_t *scope, size_t size);
C_API void free_arena(memory_t *ptr);

/* Request/return raw memory of given `size`,
uses current `thread` smart pointer,
DO NOT `free`, will be `RAII_FREE`
when `raii_deferred_clean` is called. */
C_API void *malloc_default(size_t size);

/* Request/return raw memory of given `size`,
uses current `thread` smart pointer,
DO NOT `free`, will be `RAII_FREE`
when `raii_deferred_clean` is called. */
C_API void *calloc_default(int count, size_t size);

C_API values_type raii_value(void *);
C_API raii_type type_of(void *);
C_API bool is_type(void *, raii_type);
C_API bool is_instance_of(void *, void *);
C_API bool is_value(void *);
C_API bool is_instance(void *);
C_API bool is_valid(void *);
C_API bool is_zero(size_t);
C_API bool is_empty(void *);
C_API bool is_true(bool);
C_API bool is_false(bool);
C_API bool is_str_in(const char *text, char *pattern);
C_API bool is_str_eq(const char *str, const char *str2);
C_API bool is_str_empty(const char *str);
C_API bool is_guard(void *self);

C_API void *try_calloc(int, size_t);
C_API void *try_malloc(size_t);
C_API void *try_realloc(void *, size_t);

C_API void guard_set(ex_context_t *ctx, const char *ex, const char *message);
C_API void guard_reset(void *scope, ex_setup_func set, ex_unwind_func unwind);
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
#define _malloc(size)           malloc_full(_$##__FUNCTION__, size, RAII_FREE)

/* Returns protected raw memory pointer,
DO NOT FREE, will `throw/panic` if memory request fails. */
#define _calloc(count, size)    calloc_full(_$##__FUNCTION__, count, size, RAII_FREE)

/* Defer execution `LIFO` of given function with argument,
execution begins when current `guard` scope exits or panic/throw. */
#define _defer(func, ptr)       raii_recover_by(_$##__FUNCTION__, (func_t)func, ptr)

/* Compare `err` to scoped error condition, will mark exception handled, if `true`. */
#define _recover(err)   raii_is_caught(raii_init()->arena, err)

/* Compare `err` to scoped error condition,
will mark exception handled, if `true`.
DO NOT PUT `err` in quote's like "err". */
#define _is_caught(err)   raii_is_caught(raii_init()->arena, EX_STR(err))

/* Get scoped error condition string. */
#define _get_message()  raii_message_by(raii_init()->arena)

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
    void *s##__FUNCTION__ = raii_init()->arena;         \
    ex_setup_func sf##__FUNCTION__ = exception_setup_func;      \
    ex_unwind_func uf##__FUNCTION__ = exception_unwind_func;    \
    exception_unwind_func = (ex_unwind_func)raii_deferred_free; \
    exception_setup_func = guard_set;                   \
    unique_t *_$##__FUNCTION__ = unique_init();         \
    (_$##__FUNCTION__)->status = RAII_GUARDED_STATUS;   \
    raii_init()->arena = (void *)_$##__FUNCTION__;      \
    ex_try {                                            \
        do {

    /* This ends an scoped guard section, it replaces `}`.
    On exit will begin executing deferred functions,
    return given `result` when done, use `NONE` for no return. */
    #define unguarded(result)                                           \
            } while (false);                                            \
            raii_deferred_free(_$##__FUNCTION__);                       \
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
    } ex_catch_if {                                                \
        if ((is_type(&(_$##__FUNCTION__)->defer, RAII_DEF_ARR)))    \
            raii_deferred_free(_$##__FUNCTION__);                   \
    } ex_finally {                                                  \
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

typedef struct arena_s *arena_t;
struct arena_s {
    raii_type type;
    arena_t next;
    char *avail;
    char *limit;
    void *base;
    bool is_global;
    size_t threshold;
    size_t bytes;
    size_t total;
};

/* Allocates, initializes, a new arena, `size` is allocation threshold.
Default `0` = `10` = `10kb` additional added onto `arena_alloc` requests. */
C_API arena_t arena_init(size_t);

/* Deallocates all of the space in arena, deallocates the arena itself, and
clears arena. */
C_API void arena_free(arena_t arena);

/* Allocates nbytes bytes in arena and returns a pointer to the first byte.
The bytes are uninitialized. Will `Panic` if allocation fails. */
C_API void *arena_alloc(arena_t arena, long nbytes);

/* Allocates space in arena for an array of count elements, each occupying nbytes,
and returns a pointer to the first element.
The bytes are initialized to zero. Will `Panic` if allocation fails. */
C_API void *arena_calloc(arena_t arena, long count, long nbytes);

/* Deallocates all of the space in arena — all of the space allocated since
the last call to `arena_clear`. */
C_API void arena_clear(arena_t arena);

C_API size_t arena_capacity(const arena_t arena);
C_API size_t arena_total(const arena_t arena);
C_API void arena_print(const arena_t arena);


#ifdef __cplusplus
    }
#endif
#endif /* RAII_H */
