#include "../include/coroutine.h"

int co_array_init(co_array_t *a) {
    if (UNLIKELY(!a))
        return -EINVAL;

    a->type = CO_DEF_ARR;
    a->base = NULL;
    a->elements = 0;

    return 0;
}

int co_array_reset(co_array_t *a) {
    if (UNLIKELY(!a))
        return -EINVAL;

    CO_FREE(a->base);
    a->base = NULL;
    a->elements = 0;
    memset(a, 0, sizeof(value_types));

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

void_t realloc_array(void_t optr, size_t nmemb, size_t size) {
    size_t total_size;
    if (UNLIKELY(umull_overflow(nmemb, size, &total_size))) {
        errno = ENOMEM;
        return NULL;
    }
    return CO_REALLOC(optr, total_size);
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

void_t co_array_append(co_array_t *a, size_t element_size) {
    if (!(a->elements % INCREMENT)) {
        void_t new_base;
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

static void co_array_free(void_t data) {
    co_array_t *array = data;

    co_array_reset(array);
    memset(array, 0, sizeof(array));
    //  CO_FREE(array);
    data = NULL;
}

co_array_t *co_array_new(routine_t *coro) {
    co_array_t *array;

    array = co_calloc_full(coro, 1, sizeof(*array), co_array_free);
    co_array_init(array);

    return array;
}

static CO_FORCE_INLINE defer_t *co_deferred_array_new(routine_t *coro) {
    return (defer_t *)co_array_new(coro);
}

static CO_FORCE_INLINE defer_func_t *co_deferred_array_append(defer_t *array) {
    return (defer_func_t *)co_array_append((co_array_t *)array, sizeof(defer_func_t));
}

static CO_FORCE_INLINE int co_deferred_array_reset(defer_t *array) {
    return co_array_reset((co_array_t *)array);
}

static CO_FORCE_INLINE void co_deferred_array_free(defer_t *array) {
    co_array_free((co_array_t *)array);
}

static CO_FORCE_INLINE size_t co_deferred_array_get_index(const defer_t *array, co_array_t *elem) {
    CO_ASSERT(elem >= (co_array_t *)array->base.base);
    return (size_t)(elem - (co_array_t *)array->base.base);
}

static CO_FORCE_INLINE co_array_t *co_deferred_array_get_element(const defer_t *array, size_t index) {
    CO_ASSERT(index <= array->base.elements);
    return &((co_array_t *)array->base.base)[ index ];
}

static CO_FORCE_INLINE size_t co_deferred_array_len(const defer_t *array) {
    return array->base.elements;
}

static void deferred_canceled(void_t data) {}

static void co_deferred_internal(routine_t *coro, defer_func_t *deferred) {
    const size_t num_defers = co_deferred_array_len(&coro->defer);

    CO_ASSERT(num_defers != 0 && deferred != NULL);

    if (deferred == (defer_func_t *)co_deferred_array_get_element(&coro->defer, num_defers - 1)) {
        /* If we're cancelling the last defer we armed, there's no need to waste
         * space of a deferred callback to an empty function like
         * disarmed_defer(). */
        co_array_t *defer_base = (co_array_t *)&coro->defer;
        defer_base->elements--;
    } else {
        deferred->func = deferred_canceled;
        deferred->check = NULL;
    }
}

void co_deferred_cancel(routine_t *coro, size_t index) {
    CO_ASSERT(index >= 0);

    return co_deferred_internal(coro, (defer_func_t *)co_deferred_array_get_element(&coro->defer, index));
}

void co_deferred_fire(routine_t *coro, size_t index) {
    CO_ASSERT(index >= 0);

    defer_func_t *deferred = (defer_func_t *)co_deferred_array_get_element(&coro->defer, index);
    CO_ASSERT(coro);

    deferred->func(deferred->data);

    return co_deferred_internal(coro, deferred);
}

void co_deferred_run(routine_t *coro, size_t generation) {
    co_array_t *array = (co_array_t *)&coro->defer;
    defer_func_t *defers = array->base;
    size_t i;

    coro->err_recovered = is_empty(coro->err);

    for (i = array->elements; i != generation; i--) {
        defer_func_t *defer = &defers[ i - 1 ];

        if (!is_empty(coro->err) && !is_empty(defer->check))
            coro->err_recovered = false;

        defer->func(defer->data);
        defer->data = NULL;
    }

    if (coro->err_protected && !is_empty(coro->err_allocated) && !is_empty(coro->err)) {
        if (!ex_context)
            ex_init();

        if (!is_empty(*coro->err_allocated->ptr)) {
            coro->err_allocated->func(*coro->err_allocated->ptr);
            *coro->err_allocated->ptr = NULL;
        }

        ex_context->stack = coro->err_allocated->next;
        coro->err_protected = false;
    }

    array->elements = generation;
    if (!is_empty(coro->err_allocated)) {
        CO_FREE(coro->err_allocated);
        coro->err_allocated = NULL;
    }
}

size_t co_deferred_count(const routine_t *coro) {
    const co_array_t *array = (co_array_t *)&coro->defer;

    return array->elements;
}

void co_deferred_free(routine_t *coro) {
    CO_ASSERT(coro);

    if (is_type(&coro->defer, CO_DEF_ARR)) {
        co_deferred_run(coro, 0);
        if (coro->loop_erred)
            co_deferred_array_free(&coro->defer);
        else
            co_deferred_array_reset(&coro->defer);
    }
}

static size_t co_deferred_any(routine_t *coro, func_t func, void_t data, void_t check) {
    defer_func_t *deferred;

    CO_ASSERT(func);

    deferred = co_deferred_array_append(&coro->defer);
    if (UNLIKELY(!deferred)) {
        CO_LOG("Could not add new deferred function.");
        return -1;
    } else {
        deferred->func = func;
        deferred->data = data;
        deferred->check = check;

        return co_deferred_array_get_index(&coro->defer, (co_array_t *)deferred);
    }
}

CO_FORCE_INLINE size_t co_defer(func_t func, void_t data) {
    return co_deferred(co_active(), func, data);
}

CO_FORCE_INLINE void co_defer_recover(func_t func, void_t data) {
    co_deferred_any(co_active(), func, data, (void_t)"err");
}

string_t co_recover() {
    routine_t *co = co_active();
    co->err_recovered = true;
    return !is_empty((void_t)co->panic) ? co->panic : co->err;
}

size_t co_deferred(routine_t *coro, func_t func, void_t data) {
    return co_deferred_any(coro, func, data, NULL);
}

CO_FORCE_INLINE void co_defer_cancel(size_t index) {
    co_deferred_cancel(co_active(), index);
}

CO_FORCE_INLINE void co_defer_fire(size_t index) {
    co_deferred_fire(co_active(), index);
}
