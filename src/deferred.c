#include "../include/coroutine.h"

int co_array_init(co_array_t *a) {
    if (UNLIKELY(!a))
        return -EINVAL;

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

void *co_array_append(co_array_t *a, size_t element_size) {
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

void co_array_sort(co_array_t *a, size_t element_size, int (*cmp)(const void *a, const void *b)) {
    if (LIKELY(a->elements))
        qsort(a->base, a->elements - 1, element_size, cmp);
}

static void co_array_free(void *data) {
    co_array_t *array = data;

    co_array_reset(array);
    CO_FREE(array);
}

co_array_t *co_array_new(co_routine_t *coro) {
    co_array_t *array;

    array = co_malloc_full(coro, sizeof(*array), co_array_free);
    if (LIKELY(array))
        co_array_init(array);

    return array;
}

static CO_FORCE_INLINE void co_deferred_array_sort(defer_t *array, int (*cmp)(const void *a, const void *b)) {
    co_array_sort((co_array_t *)array, sizeof(defer_func_t), cmp);
}

static CO_FORCE_INLINE defer_t *co_deferred_array_new(co_routine_t *coro) {
    return (defer_t *)co_array_new(coro);
}

static CO_FORCE_INLINE defer_func_t *co_deferred_array_append(defer_t *array) {
    return (defer_func_t *)co_array_append((co_array_t *)array, sizeof(defer_func_t));
}

static CO_FORCE_INLINE int co_deferred_array_reset(defer_t *array) {
    return co_array_reset((co_array_t *)array);
}

static CO_FORCE_INLINE co_array_t *co_deferred_array_get_array(defer_t *array) {
    return (co_array_t *)array->base.base;
}

static CO_FORCE_INLINE size_t co_deferred_array_get_index(const defer_t *array, co_array_t *elem) {
    CO_ASSERT(elem >= (co_array_t *)array->base.base);
    CO_ASSERT(elem < (co_array_t *)array->base.base + array->base.elements);
    return (size_t)(elem - (co_array_t *)array->base.base);
}

static CO_FORCE_INLINE co_array_t *co_deferred_array_get_element(const defer_t *array, size_t index) {
    CO_ASSERT(index <= array->base.elements);
    return &((co_array_t *)array->base.base)[ index ];
}

static CO_FORCE_INLINE size_t co_deferred_array_len(const defer_t *array) {
    return array->base.elements;
}

void co_deferred_run(co_routine_t *coro, size_t generation) {
    co_array_t *array = (co_array_t *)&coro->defer;
    defer_func_t *defers = array->base;
    size_t i;

    coro->err_recovered = coro->err == NULL;

    for (i = array->elements; i != generation; i--) {
        defer_func_t *defer = &defers[ i - 1 ];

        if (coro->err != NULL && defer->check != NULL)
            coro->err_recovered = false;

        defer->func(defer->data);
        defer->data = NULL;
    }

    if (coro->err_protected && coro->err_allocated != NULL && coro->err != NULL) {
        if (!ex_context)
            ex_init();

        if (*coro->err_allocated->ptr != NULL)
            coro->err_allocated->func(coro->err_allocated->ptr);

        ex_context->stack = coro->err_allocated->next;
        coro->err_protected = false;
    }

    array->elements = generation;
    if (coro->err_allocated != NULL){
        CO_FREE(coro->err_allocated);
        coro->err_allocated = NULL;
    }
}

size_t co_deferred_count(const co_routine_t *coro) {
    const co_array_t *array = (co_array_t *)&coro->defer;

    return array->elements;
}

void co_deferred_free(co_routine_t *coro) {
    CO_ASSERT(coro);
    co_deferred_run(coro, 0);
    co_deferred_array_reset(&coro->defer);
}

static void co_deferred_any(co_routine_t *coro, defer_func func, void *data, void *check) {
    defer_func_t *defer;

    CO_ASSERT(func);

    defer = co_deferred_array_append(&coro->defer);
    if (UNLIKELY(!defer)) {
        CO_LOG("Could not add new deferred function.");
    } else {
        defer->func = func;
        defer->data = data;
        defer->check = check;
    }
}

CO_FORCE_INLINE void co_defer(defer_func func, void *data) {
    co_deferred(co_active(), func, data);
}

CO_FORCE_INLINE void co_defer_recover(defer_func func, void *data) {
    co_deferred_any(co_active(), func, data, (void *)"err");
}

const char *co_recover() {
    co_routine_t *co = co_active();
    co->err_recovered = true;
    return (co->panic != NULL) ? co->panic : co->err;
}

void co_deferred(co_routine_t *coro, defer_func func, void *data) {
    co_deferred_any(coro, func, data, NULL);
}
