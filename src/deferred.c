#include "../include/coroutine.h"

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
