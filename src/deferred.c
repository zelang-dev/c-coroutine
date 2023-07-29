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

    if (strcmp(coro->name, "co_main") != 0)
    {
        for (i = array->elements; i != generation; i--)
        {
            defer_func_t *defer = &defers[i - 1];

            if (defer->check != NULL)
                coro->err_recovered = false;

            defer->func(defer->data);
            defer->data = NULL;
        }
    }

    if (coro->err_protected && coro->err_allocated != NULL && coro->err != NULL)
    {
        if (!ex_context)
            ex_init();

        if (*coro->err_allocated->ptr != NULL)
            coro->err_allocated->func(coro->err_allocated->ptr);

        ex_context->stack = coro->err_allocated->next;
        coro->err_protected = false;
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

static void co_deferred_any(co_routine_t *coro, defer_func func, void *data, void *check)
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
        defer->data = data;
        defer->check = check;
    }
}

CO_FORCE_INLINE void co_defer(defer_func func, void *data)
{
    co_deferred(co_active(), func, data);
}

CO_FORCE_INLINE void co_defer_recover(defer_func func, void *data)
{
    co_deferred_any(co_active(), func, data, (void *)"err");
}

const char *co_recover()
{
    co_routine_t *co = co_active();
    co->err_recovered = true;
    return (co->panic != NULL) ? co->panic : co->err;
}

void co_deferred(co_routine_t *coro, defer_func func, void *data)
{
    co_deferred_any(coro, func, data, NULL);
}
