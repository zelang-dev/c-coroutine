#include "include/coroutine.h"

static CO_FORCE_INLINE void co_deferred_array_sort(defer_t *array, int (*cmp)(const void *a, const void *b))
{
    co_array_sort((co_array_t *)array, sizeof(defer_func_t), cmp);
}

static CO_FORCE_INLINE defer_t *co_deferred_array_new(co_routine_t *coro)
{
    return (defer_t *)co_array_new(coro);
}

static CO_FORCE_INLINE defer_func_t *co_deferred_array_append(defer_t *array)
{
    return (defer_func_t *)co_array_append((co_array_t *)array, sizeof(defer_func_t));
}

static CO_FORCE_INLINE int co_deferred_array_reset(defer_t *array)
{
    return co_array_reset((co_array_t *)array);
}

static CO_FORCE_INLINE co_array_t *co_deferred_array_get_array(defer_t *array)
{
    return (co_array_t *)array->base.base;
}

static CO_FORCE_INLINE size_t co_deferred_array_get_index(const defer_t *array, co_array_t *elem)
{
    CO_ASSERT(elem >= (co_array_t *)array->base.base);
    CO_ASSERT(elem < (co_array_t *)array->base.base + array->base.elements);
    return (size_t)(elem - (co_array_t *)array->base.base);
}

static CO_FORCE_INLINE co_array_t *co_deferred_array_get_element(const defer_t *array, size_t index)
{
    CO_ASSERT(index <= array->base.elements);
    return &((co_array_t *)array->base.base)[index];
}

static CO_FORCE_INLINE size_t co_deferred_array_len(const defer_t *array)
{
    return array->base.elements;
}

int co_array_init(co_array_t *a)
{
    if (UNLIKELY(!a))
        return -EINVAL;

    a->base = NULL;
    a->elements = 0;

    return 0;
}

int co_array_reset(co_array_t *a)
{
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

static inline bool umull_overflow(size_t a, size_t b, size_t *out)
{
    if ((a >= MUL_NO_OVERFLOW || b >= MUL_NO_OVERFLOW) && a > 0 && SIZE_MAX / a < b)
        return true;
    *out = a * b;
    return false;
}
#else
#define umull_overflow __builtin_mul_overflow
#endif

void *realloc_array(void *optr, size_t nmemb, size_t size)
{
    size_t total_size;
    if (UNLIKELY(umull_overflow(nmemb, size, &total_size)))
    {
        errno = ENOMEM;
        return NULL;
    }
    return CO_REALLOC(optr, total_size);
}

#endif /* HAS_REALLOC_ARRAY */

#if !defined(HAVE_BUILTIN_ADD_OVERFLOW)
static inline bool add_overflow(size_t a, size_t b, size_t *out)
{
    if (UNLIKELY(a > 0 && b > SIZE_MAX - a))
        return true;

    *out = a + INCREMENT;
    return false;
}
#else
#define add_overflow __builtin_add_overflow
#endif

void *co_array_append(co_array_t *a, size_t element_size)
{
    if (!(a->elements % INCREMENT))
    {
        void *new_base;
        size_t new_cap;

        if (UNLIKELY(add_overflow(a->elements, INCREMENT, &new_cap)))
        {
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

void co_array_sort(co_array_t *a, size_t element_size, int (*cmp)(const void *a, const void *b))
{
    if (LIKELY(a->elements))
        qsort(a->base, a->elements - 1, element_size, cmp);
}

static void co_array_free(void *data)
{
    co_array_t *array = data;

    co_array_reset(array);
    CO_FREE(array);
}

co_array_t *co_array_new(co_routine_t *coro)
{
    co_array_t *array;

    array = co_malloc_full(coro, sizeof(*array), co_array_free);
    if (LIKELY(array))
        co_array_init(array);

    return array;
}

void co_deferred_run(co_routine_t *coro, size_t generation)
{
    co_array_t *array = (co_array_t *)&coro->defer;
    defer_func_t *defers = array->base;
    size_t i;

    for (i = array->elements; i != generation; i--)
    {
        defer_func_t *defer = &defers[i - 1];

        defer->func(defer->data1);
    }

    array->elements = generation;
}

size_t co_deferred_count(const co_routine_t *coro)
{
    const co_array_t *array = (co_array_t *)&coro->defer;

    return array->elements;
}

void co_deferred_free(co_routine_t *coro)
{
    CO_ASSERT(coro);
    co_deferred_run(coro, 0);
    co_deferred_array_reset(&coro->defer);
}

static void co_deferred_any(co_routine_t *coro, defer_func func, void *data1, void *data2)
{
    defer_func_t *defer;

    CO_ASSERT(func);

    defer = co_deferred_array_append(&coro->defer);
    if (UNLIKELY(!defer))
    {
        CO_LOG("Could not add new deferred function.");
    }
    else
    {
        defer->func = func;
        defer->data1 = data1;
        defer->data2 = data2;
    }
}

CO_FORCE_INLINE void co_defer(defer_func func, void *data)
{
    co_deferred(co_active(), func, data);
}

void co_deferred(co_routine_t *coro, defer_func func, void *data)
{
    co_deferred_any(coro, func, data, NULL);
}

void *co_malloc_full(co_routine_t *coro, size_t size, defer_func func)
{
    void *ptr = CO_MALLOC(size);

    if (LIKELY(ptr))
        co_deferred(coro, func, ptr);

    return ptr;
}

void *co_calloc_full(co_routine_t *coro, int count, size_t size, defer_func func)
{
    void *ptr = CO_CALLOC(count, size);

    if (LIKELY(ptr))
        co_deferred(coro, func, ptr);

    return ptr;
}

CO_FORCE_INLINE void *co_new_by(int count, size_t size)
{
    return co_calloc_full(co_active(), count, size, CO_FREE);
}

CO_FORCE_INLINE void *co_new(size_t size)
{
    return co_malloc_full(co_active(), size, CO_FREE);
}

void *co_malloc(co_routine_t *coro, size_t size)
{
    return co_malloc_full(coro, size, CO_FREE);
}

char *co_strndup(co_routine_t *coro, const char *str, size_t max_len)
{
    const size_t len = strnlen(str, max_len) + 1;
    char *dup = co_memdup(coro, str, len);

    if (LIKELY(dup))
        dup[len - 1] = '\0';

    return dup;
}

char *co_strdup(co_routine_t *coro, const char *str)
{
    return co_memdup(coro, str, strlen(str) + 1);
}

#if defined(_WIN32) || defined(_WIN64)
int vasprintf(char **str_p, const char *fmt, va_list ap)
{
    va_list ap_copy;
    int formattedLength, actualLength;
    size_t requiredSize;

    // be paranoid
    *str_p = NULL;

    // copy va_list, as it is used twice
    va_copy(ap_copy, ap);

    // compute length of formatted string, without NULL terminator
    formattedLength = _vscprintf(fmt, ap_copy);
    va_end(ap_copy);

    // bail out on error
    if (formattedLength < 0)
    {
        return -1;
    }

    // allocate buffer, with NULL terminator
    requiredSize = ((size_t)formattedLength) + 1;
    *str_p = (char *)CO_MALLOC(requiredSize);

    // bail out on failed memory allocation
    if (*str_p == NULL)
    {
        errno = ENOMEM;
        return -1;
    }

    // write formatted string to buffer, use security hardened _s function
    actualLength = vsnprintf_s(*str_p, requiredSize, requiredSize - 1, fmt, ap);

    // again, be paranoid
    if (actualLength != formattedLength)
    {
        CO_FREE(*str_p);
        *str_p = NULL;
        errno = EOTHER;
        return -1;
    }

    return formattedLength;
}

int asprintf(char **str_p, const char *fmt, ...)
{
    int result;

    va_list ap;
    va_start(ap, fmt);
    result = vasprintf(str_p, fmt, ap);
    va_end(ap);

    return result;
}
#endif

char *co_printf(const char *fmt, ...)
{
    va_list values;
    int len;
    char *tmp_str;

    va_start(values, fmt);
    len = vasprintf(&tmp_str, fmt, values);
    va_end(values);

    if (UNLIKELY(len < 0))
        return NULL;

    co_deferred(co_active(), CO_FREE, tmp_str);
    return tmp_str;
}

void *co_memdup(co_routine_t *coro, const void *src, size_t len)
{
    void *ptr = co_malloc(coro, len);

    return LIKELY(ptr) ? memcpy(ptr, src, len) : NULL;
}
