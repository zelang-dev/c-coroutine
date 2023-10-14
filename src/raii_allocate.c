#include "../include/coroutine.h"

static thread_local gc_channel_t *channel_list = NULL;
static thread_local gc_coroutine_t *coroutine_list = NULL;

void gc_coroutine(routine_t *co) {
    if (!coroutine_list)
        coroutine_list = (gc_coroutine_t *)ht_group_init();
    hash_put(coroutine_list, co_itoa(co->cid), co);
}

void gc_channel(channel_t *ch) {
    if (!channel_list)
        channel_list = ht_channel_init();
    hash_put(channel_list, co_itoa(ch->id), ch);
}

CO_FORCE_INLINE gc_channel_t *gc_channel_list() {
    return channel_list;
}

CO_FORCE_INLINE gc_coroutine_t *gc_coroutine_list() {
    return coroutine_list;
}

void gc_channel_free() {
    if (channel_list)
        hash_free(channel_list);
}

void gc_coroutine_free() {
    if (coroutine_list)
        hash_free(coroutine_list);
}

void_t try_calloc(int count, size_t size) {
    void_t ptr = CO_CALLOC(count, size);
    if (ptr == NULL)
        co_panic("Memory allocation failed!");

    return ptr;
}

void_t try_malloc(size_t size) {
    void_t ptr = CO_MALLOC(size);
    if (ptr == NULL)
        co_panic("Memory allocation failed!");

    return ptr;
}

void_t co_malloc_full(routine_t *coro, size_t size, func_t func) {
    void_t ptr = try_malloc(size);

    if (coro->err_allocated == NULL)
        coro->err_allocated = try_malloc(sizeof(ex_ptr_t));

    ex_protect_ptr(coro->err_allocated, ptr, func);
    coro->err_protected = true;
    co_deferred(coro, func, ptr);

    return ptr;
}

void_t co_calloc_full(routine_t *coro, int count, size_t size, func_t func) {
    void_t ptr = try_calloc(count, size);

    if (coro->err_allocated == NULL)
        coro->err_allocated = try_calloc(1, sizeof(ex_ptr_t));

    ex_protect_ptr(coro->err_allocated, ptr, func);
    coro->err_protected = true;
    co_deferred(coro, func, ptr);

    return ptr;
}

CO_FORCE_INLINE void_t co_new_by(int count, size_t size) {
    return co_calloc_full(co_active(), count, size, CO_FREE);
}

CO_FORCE_INLINE void_t co_new(size_t size) {
    return co_malloc_full(co_active(), size, CO_FREE);
}

void_t co_malloc(routine_t *coro, size_t size) {
    return co_malloc_full(coro, size, CO_FREE);
}

string co_strndup(string_t str, size_t max_len) {
    const size_t len = strnlen(str, max_len) + 1;
    string dup = co_memdup(co_active(), str, len);

    if (LIKELY(dup))
        dup[len - 1] = '\0';

    return dup;
}

CO_FORCE_INLINE string co_strdup(string_t str) {
    return co_memdup(co_active(), str, strlen(str));
}

CO_FORCE_INLINE string co_string(string_t str, size_t length) {
    return co_memdup(co_active(), str, length);
}

#if defined(_WIN32) || defined(_WIN64)
int vasprintf(string *str_p, string_t fmt, va_list ap) {
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
    *str_p = (string)CO_MALLOC(requiredSize);

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

int asprintf(string *str_p, string_t fmt, ...) {
    int result;

    va_list ap;
    va_start(ap, fmt);
    result = vasprintf(str_p, fmt, ap);
    va_end(ap);

    return result;
}
#endif

string co_sprintf(string_t fmt, ...) {
    va_list values;
    int len;
    string tmp_str;

    va_start(values, fmt);
    len = vasprintf(&tmp_str, fmt, values);
    va_end(values);

    if (UNLIKELY(len < 0))
        return NULL;

    co_deferred(co_active(), CO_FREE, tmp_str);
    return tmp_str;
}

void_t co_memdup(routine_t *coro, const_t src, size_t len) {
    void_t ptr = co_new_by(1, len + 1);

    return LIKELY(ptr) ? memcpy(ptr, src, len) : NULL;
}

string *co_str_split(string_t s, string_t delim, int *count) {
    void *data;
    string _s = (string)s;
    string_t *ptrs;
    size_t ptrsSize, nbWords = 1, sLen = strlen(s), delimLen = strlen(delim);

    while ((_s = strstr(_s, delim))) {
        _s += delimLen;
        ++nbWords;
    }

    ptrsSize = (nbWords + 1) * sizeof(string);
    ptrs = data = co_new_by(1, ptrsSize + sLen + 1);
    if (data) {
        *ptrs = _s = strcpy((string)data + ptrsSize, s);
        if (nbWords > 1) {
            while ((_s = strstr(_s, delim))) {
                *_s = '\0';
                _s += delimLen;
                *++ptrs = _s;
            }
        }

        *++ptrs = NULL;
        if (count)
            *count = nbWords;
    }

    return data;
}

string co_concat_by(int num_args, ...) {
    int strsize = 0;
    va_list ap;
    va_start(ap, num_args);
    for (int i = 0; i < num_args; i++)
        strsize += strlen(va_arg(ap, string));

    string res = try_calloc(1, strsize + 1);
    strsize = 0;
    va_start(ap, num_args);
    for (int i = 0; i < num_args; i++) {
        string s = va_arg(ap, string);
        strcpy(res + strsize, s);
        strsize += strlen(s);
    }
    va_end(ap);

    co_defer(CO_FREE, res);
    return res;
}

string co_str_concat(string_t header, string_t *words, size_t num_words) {
    size_t message_len = strlen(header) + 1; /* + 1 for terminating NULL */
    string message = (string)try_calloc(1, message_len);
    strncat(message, header, message_len);

    for (int i = 0; i < num_words; ++i) {
        message_len += 1 + strlen(words[i]); /* 1 + for separator ';' */
        message = (string)realloc(message, message_len);
        strncat(strncat(message, ";", message_len), words[i], message_len);
    }

    co_defer(CO_FREE, message);
    return message;
}
