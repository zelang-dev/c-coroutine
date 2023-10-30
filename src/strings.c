#include "../include/coroutine.h"

string_t co_itoa(int64_t number) {
#ifdef _WIN32
    snprintf(co_active()->scrape, CO_SCRAPE_SIZE, "%lld", number);
#else
    snprintf(co_active()->scrape, CO_SCRAPE_SIZE, "%ld", number);
#endif
    return co_active()->scrape;
}

int co_strpos(string_t text, string pattern) {
    int c, d, e, text_length, pattern_length, position = -1;

    text_length = strlen(text);
    pattern_length = strlen(pattern);

    if (pattern_length > text_length) {
        return -1;
    }

    for (c = 0; c <= text_length - pattern_length; c++) {
        position = e = c;
        for (d = 0; d < pattern_length; d++) {
            if (pattern[d] == text[e]) {
                e++;
            } else {
                break;
            }
        }

        if (d == pattern_length) {
            return position;
        }
    }

    return -1;
}

void co_strcpy(string dest, string_t src, size_t len) {
#if defined(_WIN32) || defined(_WIN64)
    strcpy_s(dest, len, src);
#else
    strcpy(dest, src);
#endif
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
    if (is_empty(*str_p)) {
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

string *co_str_split(string_t s, string_t delim, int *count) {
    if (is_str_eq(s, ""))
        return NULL;

    if (is_empty((void_t)delim))
        delim = " ";

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
        message = (string)CO_REALLOC(message, message_len);
        strncat(strncat(message, ";", message_len), words[i], message_len);
    }

    co_defer(CO_FREE, message);
    return message;
}

ht_string_t *co_parse_str(string lines, string sep) {
    ht_string_t *this = ht_string_init();
    defer(hash_free, this);
    int i = 0;

    string *token = co_str_split(lines, sep, &i);
    for (int x = 0; x < i; x++) {
        string *parts = co_str_split(token[x], "=", NULL);
        if (!is_empty(parts))
            hash_put(this, trim(parts[0]), trim(parts[1]));
    }

    return this;
}

/* Make a string uppercase. */
string str_toupper(string s, size_t len) {
    unsigned char *c;
    const unsigned char *e;

    c = (unsigned char *)s;
    e = (unsigned char *)c + len;

    while (c < e) {
        *c = toupper(*c);
        c++;
    }

    return s;
}

/* Make a string lowercase */
string str_tolower(string s, size_t len) {
    unsigned char *c;
    const unsigned char *e;

    c = (unsigned char *)s;
    e = c + len;

    while (c < e) {
        *c = tolower(*c);
        c++;
    }
    return s;
}

/* Make first character uppercase in string/word, remainder lowercase,
a word is represented by separator character provided. */
string word_toupper(string str, char sep) {
    size_t i, length = 0;

    length = strlen(str);
    for (i = 0;i < length;i++) {// capitalize the first most letter
        if (i == 0) {
            str[i] = toupper(str[i]);
        } else if (str[i] == sep) {//check if the charater is the separator
            str[i + 1] = toupper(str[i + 1]);
            i += 1;  // skip the next charater
        } else {
            str[i] = tolower(str[i]); // lowercase reset of the characters
        }
    }

    return str;
}

string ltrim(string s) {
    while (isspace(*s)) s++;
    return s;
}

string rtrim(string s) {
    string back = s + strlen(s);
    while (isspace(*--back));
    *(back + 1) = '\0';
    return s;
}

string trim(string s) {
    return rtrim(ltrim(s));
}
