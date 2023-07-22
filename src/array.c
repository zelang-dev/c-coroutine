#include "../include/coroutine.h"

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
