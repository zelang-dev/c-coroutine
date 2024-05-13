#include "raii.h"

static thread_local memory_t raii_context_buffer;
thread_local memory_t *raii_context = NULL;

int raii_array_init(raii_array_t *a) {
    if (UNLIKELY(!a))
        return -EINVAL;

    a->base = NULL;
    a->elements = 0;
    a->type = RAII_DEF_ARR;

    return 0;
}

int raii_array_reset(raii_array_t *a) {
    if (UNLIKELY(!a))
        return -EINVAL;

    RAII_FREE(a->base);
    a->base = NULL;
    a->elements = 0;
    memset(a, 0, sizeof(a));

    return 0;
}

#if !defined(HAS_REALLOC_ARRAY)
#if !defined(HAVE_BUILTIN_MUL_OVERFLOW)
/*
* This is sqrt(SIZE_MAX+1), as s1*s2 <= SIZE_MAX
* if both s1 < MUL_NO_OVERFLOW and s2 < MUL_NO_OVERFLOW
*/
#define MUL_NO_OVERFLOW ((size_t)1 << (sizeof(size_t) * 4))
static inline bool umull_overflow(size_t a, size_t b, size_t *out) {
    if ((a >= MUL_NO_OVERFLOW || b >= MUL_NO_OVERFLOW) && a > 0 && SIZE_MAX / a < b)
        return true;
    *out = a * b;
    return false;
}
#else
#define umull_overflow __builtin_mul_overflow
#endif
void *realloc_array(void *optr, size_t nmemb, size_t size) {
    size_t total_size;
    if (UNLIKELY(umull_overflow(nmemb, size, &total_size))) {
        errno = ENOMEM;
        return NULL;
    }

    return RAII_REALLOC(optr, total_size);
}
#endif /* HAS_REALLOC_ARRAY */

#if !defined(HAVE_BUILTIN_ADD_OVERFLOW)
static inline bool add_overflow(size_t a, size_t b, size_t *out) {
    if (UNLIKELY(a > 0 && b > SIZE_MAX - a))
        return true;

    *out = a + INCREMENT;
    return false;
}
#else
#define add_overflow __builtin_add_overflow
#endif

void *raii_array_append(raii_array_t *a, size_t element_size) {
    if (!(a->elements % INCREMENT)) {
        void *new_base;
        size_t new_cap;

        if (UNLIKELY(add_overflow(a->elements, INCREMENT, &new_cap))) {
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

RAII_INLINE int raii_deferred_init(defer_t *array) {
    return raii_array_init((raii_array_t *)array);
}

memory_t *raii_init(void) {
    if (raii_context == NULL) {
        raii_context = &raii_context_buffer;
        raii_context->arena = NULL;
        raii_context->protector = NULL;
        raii_context->panic = NULL;
        raii_context->err = NULL;
        raii_context->is_protected = false;
        raii_context->is_recovered = false;
        raii_context->mid = -1;
        if (UNLIKELY(raii_deferred_init(&raii_context->defer) < 0))
            raii_panic("Deferred initialization failed!");
    }

    return raii_context;
}

RAII_INLINE size_t raii_mid(void) {
    return raii_init()->mid;
}

RAII_INLINE size_t raii_last_mid(memory_t *scope) {
    return scope->mid;
}

void *try_calloc(int count, size_t size) {
    void *ptr = RAII_CALLOC(count, size);
    if (ptr == NULL) {
        errno = ENOMEM;
        raii_panic("Memory allocation failed!");
    }

    return ptr;
}

void *try_malloc(size_t size) {
    void *ptr = RAII_MALLOC(size);
    if (ptr == NULL) {
        errno = ENOMEM;
        raii_panic("Memory allocation failed!");
    }

    return ptr;
}

unique_t *unique_init(void) {
    unique_t *raii = try_calloc(1, sizeof(unique_t));
    if (UNLIKELY(raii_deferred_init(&raii->defer) < 0))
        raii_panic("Deferred initialization failed!");

    raii->arena = NULL;
    raii->protector = NULL;
    raii->is_protected = false;
    raii->mid = -1;
    return raii;
}

memory_t *raii_malloc_full(size_t size, func_t func) {
    memory_t *raii = try_malloc(sizeof(memory_t));
    if (UNLIKELY(raii_deferred_init(&raii->defer) < 0))
        raii_panic("Deferred initialization failed!");

    raii->arena = try_malloc(size);
    raii->protector = try_malloc(sizeof(ex_ptr_t));

    ex_protect_ptr(raii->protector, raii->arena, func);
    raii->is_protected = true;
    raii->mid = raii_deferred(raii, func, raii->arena);

    return raii;
}

void *malloc_full(memory_t *scope, size_t size, func_t func) {
    void *arena = try_malloc(size);
    if (is_empty(scope->protector))
        scope->protector = try_malloc(sizeof(ex_ptr_t));

    ex_protect_ptr(scope->protector, arena, func);
    scope->is_protected = true;
    scope->mid = raii_deferred(scope, func, arena);

    return arena;
}

RAII_INLINE void *malloc_arena(size_t size) {
    return malloc_full(raii_init(), size, RAII_FREE);
}

RAII_INLINE memory_t *raii_new(size_t size) {
    return raii_malloc_full(size, RAII_FREE);
}

RAII_INLINE void *new_arena(memory_t *scope, size_t size) {
    return malloc_full(scope, size, RAII_FREE);
}

memory_t *raii_calloc_full(int count, size_t size, func_t func) {
    memory_t *raii = try_calloc(1, sizeof(memory_t));
    if (UNLIKELY(raii_deferred_init(&raii->defer) < 0))
        raii_panic("Deferred initialization failed!");

    raii->arena = try_calloc(count, size);
    raii->protector = try_calloc(1, sizeof(ex_ptr_t));

    ex_protect_ptr(raii->protector, raii->arena, func);
    raii->is_protected = true;
    raii->mid = raii_deferred(raii, func, raii->arena);

    return raii;
}

void *calloc_full(memory_t *scope, int count, size_t size, func_t func) {
    void *arena = try_calloc(count, size);
    if (is_empty(scope->protector))
        scope->protector = try_calloc(1, sizeof(ex_ptr_t));

    ex_protect_ptr(scope->protector, arena, func);
    scope->is_protected = true;
    scope->mid = raii_deferred(scope, func, arena);

    return arena;
}

RAII_INLINE void *calloc_arena(int count, size_t size) {
    return calloc_full(raii_init(), count, size, RAII_FREE);
}

RAII_INLINE memory_t *raii_calloc(int count, size_t size) {
    return raii_calloc_full(count, size, RAII_FREE);
}

RAII_INLINE void *calloc_by(memory_t *scope, int count, size_t size) {
    return calloc_full(scope, count, size, RAII_FREE);
}

void raii_delete(memory_t *ptr) {
    if (ptr == NULL)
        return;

    raii_deferred_free(ptr);
    if (ptr != &raii_context_buffer) {
        memset(ptr, -1, sizeof(ptr));
        RAII_FREE(ptr);
    }

    ptr = NULL;
}

RAII_INLINE void raii_destroy(void) {
    raii_delete(raii_init());
}

static void raii_array_free(void *data) {
    raii_array_t *array = data;

    raii_array_reset(array);
    memset(array, 0, sizeof(array));
    data = NULL;
}

raii_array_t *raii_array_new(memory_t *scope) {
    raii_array_t *array = calloc_full(scope, 1, sizeof(raii_array_t), raii_array_free);
    raii_array_init(array);

    return array;
}

static RAII_INLINE defer_t *raii_deferred_array_new(memory_t *scope) {
    return (defer_t *)raii_array_new(scope);
}

static RAII_INLINE defer_func_t *raii_deferred_array_append(defer_t *array) {
    return (defer_func_t *)raii_array_append((raii_array_t *)array, sizeof(defer_func_t));
}

static RAII_INLINE int raii_deferred_array_reset(defer_t *array) {
    return raii_array_reset((raii_array_t *)array);
}

static RAII_INLINE void raii_deferred_array_free(defer_t *array) {
    raii_array_free((raii_array_t *)array);
}

static RAII_INLINE size_t raii_deferred_array_get_index(const defer_t *array, raii_array_t *elem) {
    RAII_ASSERT(elem >= (raii_array_t *)array->base.base);
    return (size_t)(elem - (raii_array_t *)array->base.base);
}

static RAII_INLINE raii_array_t *raii_deferred_array_get_element(const defer_t *array, size_t index) {
    RAII_ASSERT(index <= array->base.elements);
    return &((raii_array_t *)array->base.base)[index];
}

static RAII_INLINE size_t raii_deferred_array_len(const defer_t *array) {
    return array->base.elements;
}

static void deferred_canceled(void *data) {}

static void raii_deferred_internal(memory_t *scope, defer_func_t *deferred) {
    const size_t num_defers = raii_deferred_array_len(&scope->defer);

    RAII_ASSERT(num_defers != 0 && deferred != NULL);

    if (deferred == (defer_func_t *)raii_deferred_array_get_element(&scope->defer, num_defers - 1)) {
        /* If we're cancelling the last defer we armed, there's no need to waste
         * space of a deferred callback to an empty function like
         * disarmed_defer(). */
        raii_array_t *defer_base = (raii_array_t *)&scope->defer;
        defer_base->elements--;
    } else {
        deferred->func = deferred_canceled;
        deferred->check = NULL;
    }
}

void raii_deferred_cancel(memory_t *scope, size_t index) {
    RAII_ASSERT(index >= 0);

    raii_deferred_internal(scope, (defer_func_t *)raii_deferred_array_get_element(&scope->defer, index));
}

void raii_deferred_fire(memory_t *scope, size_t index) {
    RAII_ASSERT(index >= 0);

    defer_func_t *deferred = (defer_func_t *)raii_deferred_array_get_element(&scope->defer, index);
    RAII_ASSERT(scope);

    deferred->func(deferred->data);

    raii_deferred_internal(scope, deferred);
}

static void raii_deferred_run(memory_t *scope, size_t generation) {
    raii_array_t *array = (raii_array_t *)&scope->defer;
    defer_func_t *defers = array->base;
    bool defer_ran = false;
    size_t i;

    scope->is_recovered = is_empty(scope->err);

    for (i = array->elements; i != generation; i--) {
        defer_func_t *defer = &defers[i - 1];
        defer_ran = true;

        if (!is_empty(scope->err) && !is_empty(defer->check))
            scope->is_recovered = false;

        defer->func(defer->data);
        defer->data = NULL;
    }

    if (scope->is_protected && !is_empty(scope->protector) && !is_empty(scope->err)) {
        if (!ex_context)
            ex_init();

        scope->is_protected = false;
        if (!defer_ran) {
            scope->protector->func(*scope->protector->ptr);
            *scope->protector->ptr = NULL;
        }

        ex_unprotected_ptr(scope->protector);
    }

    array->elements = generation;
    if (!is_empty(scope->protector)) {
        RAII_FREE(scope->protector);
        scope->protector = NULL;
    }
}

size_t raii_deferred_count(memory_t *scope) {
    const raii_array_t *array = (raii_array_t *)&scope->defer;

    return array->elements;
}

void raii_deferred_free(memory_t *scope) {
    RAII_ASSERT(scope);

    if (is_type(&scope->defer, RAII_DEF_ARR)) {
        raii_deferred_run(scope, 0);
        raii_deferred_array_reset(&scope->defer);
    }
}

RAII_INLINE void raii_deferred_clean(void) {
    raii_deferred_free(raii_init());
}

static size_t raii_deferred_any(memory_t *scope, func_t func, void *data, void *check) {
    defer_func_t *deferred;

    RAII_ASSERT(func);

    deferred = raii_deferred_array_append(&scope->defer);
    if (UNLIKELY(!deferred)) {
        RAII_LOG("Could not add new deferred function.");
        return -1;
    } else {
        deferred->func = func;
        deferred->data = data;
        deferred->check = check;

        return raii_deferred_array_get_index(&scope->defer, (raii_array_t *)deferred);
    }
}

RAII_INLINE size_t raii_deferred(memory_t *scope, func_t func, void *data) {
    return raii_deferred_any(scope, func, data, NULL);
}

RAII_INLINE size_t raii_defer(func_t func, void *data) {
    return raii_deferred(raii_init(), func, data);
}

RAII_INLINE void raii_recover(func_t func, void *data) {
    raii_deferred_any(raii_init(), func, data, (void *)"err");
}

RAII_INLINE void raii_recover_by(memory_t *scope, func_t func, void *data) {
    raii_deferred_any(scope, func, data, (void *)"err");
}

bool raii_caught(const char *err) {
    memory_t *scope = raii_init();
    const char *exception = (const char *)(!is_empty((void *)scope->panic) ? scope->panic : scope->err);
    if (scope->is_recovered = is_str_eq(err, exception))
        ex_init()->state = ex_catch_st;

    return scope->is_recovered;
}

bool raii_is_caught(unique_t *scope, const char *err) {
    const char *exception = (const char *)(!is_empty((void *)scope->panic) ? scope->panic : scope->err);
    if (scope->is_recovered = is_str_eq(err, exception))
        ex_init()->state = ex_catch_st;

    return scope->is_recovered;
}

const char *raii_message(void) {
    memory_t *scope = raii_init();
    return !is_empty((void *)scope->panic) ? scope->panic : scope->err;
}

RAII_INLINE const char *raii_message_by(memory_t *scope) {
    return !is_empty((void *)scope->panic) ? scope->panic : scope->err;
}

RAII_INLINE void raii_defer_cancel(size_t index) {
    raii_deferred_cancel(raii_init(), index);
}

RAII_INLINE void raii_defer_fire(size_t index) {
    raii_deferred_fire(raii_init(), index);
}

void guard_set(ex_context_t *ctx, const char *ex, const char *message) {
    memory_t *scope = raii_init()->arena;
    scope->err = (void *)ex;
    scope->panic = message;
    ctx->is_guarded = true;
    ex_swap_set(ctx, (void *)scope);
    ex_unwind_set(ctx, scope->is_protected);
}

void guard_reset(void *scope, ex_setup_func set, ex_unwind_func unwind) {
    raii_init()->arena = scope;
    ex_swap_reset(ex_init());
    ex_init()->is_guarded = false;
    exception_setup_func = set;
    exception_unwind_func = unwind;
}

void guard_delete(memory_t *ptr) {
    if (is_guard(ptr)) {
        memset(ptr, -1, sizeof(ptr));
        RAII_FREE(ptr);
        ptr = NULL;
    }
}

values_type *raii_value(void *data) {
    if (data)
        return ((raii_values_t *)data)->value;

    RAII_LOG("attempt to get value on null");
    return;
}

static void raii_unwind_set(ex_context_t *ctx, const char *ex, const char *message) {
    memory_t *scope = raii_init();
    scope->err = (void *)ex;
    scope->panic = message;
    ex_swap_set(ctx, (void *)scope);
    ex_unwind_set(ctx, scope->is_protected);
}

void raii_setup(void) {
    exception_unwind_func = (ex_unwind_func)raii_deferred_free;
    exception_setup_func = raii_unwind_set;
    ex_signal_setup();
}

int strpos(const char *text, char *pattern) {
    size_t c, d, e, text_length, pattern_length, position = -1;

    text_length = strlen(text);
    pattern_length = strlen(pattern);

    if (pattern_length > text_length)
        return -1;

    for (c = 0; c <= text_length - pattern_length; c++) {
        position = e = c;
        for (d = 0; d < pattern_length; d++)
            if (pattern[d] == text[e])
                e++;
            else
                break;

        if (d == pattern_length)
            return (int)position;
    }

    return -1;
}

RAII_INLINE raii_type type_of(void *self) {
    return ((var_t *)self)->type;
}

RAII_INLINE bool is_guard(void *self) {
    return !is_empty(self) && ((unique_t *)self)->status == RAII_GUARDED_STATUS;
}

RAII_INLINE bool is_type(void *self, raii_type check) {
    return type_of(self) == check;
}

RAII_INLINE bool is_instance_of(void *self, void *check) {
    return type_of(self) == type_of(check);
}

RAII_INLINE bool is_value(void *self) {
    return (type_of(self) > RAII_NULL) && (type_of(self) < RAII_NAN);
}

RAII_INLINE bool is_instance(void *self) {
    return (type_of(self) > RAII_NAN) && (type_of(self) < RAII_NO_INSTANCE);
}

RAII_INLINE bool is_valid(void *self) {
    return is_value(self) || is_instance(self);
}

RAII_INLINE bool is_zero(size_t self) {
    return self == 0;
}

RAII_INLINE bool is_empty(void *self) {
    return self == NULL;
}

RAII_INLINE bool is_str_in(const char *text, char *pattern) {
    return strpos(text, pattern) >= 0;
}

RAII_INLINE bool is_str_eq(const char *str, const char *str2) {
    return (str != NULL && str2 != NULL) && (strcmp(str, str2) == 0);
}

RAII_INLINE bool is_str_empty(const char *str) {
    return is_str_eq(str, "");
}
