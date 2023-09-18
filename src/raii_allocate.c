#include "../include/coroutine.h"

static thread_local gc_channel_t *channel_list = NULL;
static thread_local gc_coroutine_t *coroutine_list = NULL;

void gc_coroutine(co_routine_t *co) {
    if (!coroutine_list)
        coroutine_list = (gc_coroutine_t *)co_ht_group_init();
    co_hash_put(coroutine_list, co_itoa(co->cid), co);
}

void gc_channel(channel_t *ch) {
    if (!channel_list)
        channel_list = co_ht_channel_init();
    co_hash_put(channel_list, co_itoa(ch->id), ch);
}

CO_FORCE_INLINE gc_channel_t *gc_channel_list() {
    return channel_list;
}

CO_FORCE_INLINE gc_coroutine_t *gc_coroutine_list() {
    return coroutine_list;
}

void gc_channel_free() {
    if (channel_list)
        co_hash_free(channel_list);
}

void gc_coroutine_free() {
    if (coroutine_list)
        co_hash_free(coroutine_list);
}

void_t co_malloc_full(co_routine_t *coro, size_t size, func_t func) {
    void_t ptr = CO_MALLOC(size);

    if (LIKELY(ptr)) {
        if (coro->err_allocated == NULL)
            coro->err_allocated = CO_MALLOC(sizeof(ex_ptr_t));

        ex_protect_ptr(coro->err_allocated, ptr, func);
        coro->err_protected = true;
        co_deferred(coro, func, ptr);
    }

    return ptr;
}

void_t co_calloc_full(co_routine_t *coro, int count, size_t size, func_t func) {
    void_t ptr = CO_CALLOC(count, size);

    if (LIKELY(ptr)) {
        if (coro->err_allocated == NULL)
            coro->err_allocated = CO_CALLOC(1, sizeof(ex_ptr_t));

        ex_protect_ptr(coro->err_allocated, ptr, func);
        coro->err_protected = true;
        co_deferred(coro, func, ptr);
    }

    return ptr;
}

CO_FORCE_INLINE void_t co_new_by(int count, size_t size) {
    return co_calloc_full(co_active(), count, size, CO_FREE);
}

CO_FORCE_INLINE void_t co_new(size_t size) {
    return co_malloc_full(co_active(), size, CO_FREE);
}

void_t co_malloc(co_routine_t *coro, size_t size) {
    return co_malloc_full(coro, size, CO_FREE);
}

char *co_strndup(string_t str, size_t max_len) {
    const size_t len = strnlen(str, max_len) + 1;
    char *dup = co_memdup(co_active(), str, len);

    if (LIKELY(dup))
        dup[ len - 1 ] = '\0';

    return dup;
}

CO_FORCE_INLINE char *co_strdup(string_t str) {
    return co_memdup(co_active(), str, strlen(str) + 1);
}

#if defined(_WIN32) || defined(_WIN64)
int vasprintf(char **str_p, string_t fmt, va_list ap) {
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
    if (formattedLength < 0) {
        return -1;
    }

    // allocate buffer, with NULL terminator
    requiredSize = ((size_t)formattedLength) + 1;
    *str_p = (char *)CO_MALLOC(requiredSize);

    // bail out on failed memory allocation
    if (*str_p == NULL) {
        errno = ENOMEM;
        return -1;
    }

    // write formatted string to buffer, use security hardened _s function
    actualLength = vsnprintf_s(*str_p, requiredSize, requiredSize - 1, fmt, ap);

    // again, be paranoid
    if (actualLength != formattedLength) {
        CO_FREE(*str_p);
        *str_p = NULL;
        errno = EOTHER;
        return -1;
    }

    return formattedLength;
}

int asprintf(char **str_p, string_t fmt, ...) {
    int result;

    va_list ap;
    va_start(ap, fmt);
    result = vasprintf(str_p, fmt, ap);
    va_end(ap);

    return result;
}
#endif

char *co_sprintf(string_t fmt, ...) {
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

void_t co_memdup(co_routine_t *coro, const_t src, size_t len) {
    void_t ptr = co_malloc(coro, len);

    return LIKELY(ptr) ? memcpy(ptr, src, len) : NULL;
}
