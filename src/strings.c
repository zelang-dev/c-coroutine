#include "../include/coroutine.h"

string_t co_itoa(int64_t number) {
#ifdef _WIN32
    snprintf_(co_active()->scrape, CO_SCRAPE_SIZE, "%lld", number);
#else
    snprintf_(co_active()->scrape, CO_SCRAPE_SIZE, "%ld", number);
#endif
    return co_active()->scrape;
}

int co_strpos(string_t text, string pattern) {
    size_t c, d, e, text_length, pattern_length, position = -1;

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
            return (int)position;
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

string *co_split_ex(string_t s, string_t delim, int *count, bool use_defer) {
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
    if(use_defer)
        ptrs = data = co_new_by(1, ptrsSize + sLen + 1);
    else
        ptrs = data = try_calloc(1, ptrsSize + sLen + 1);

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
            *count = (int)nbWords;
    }

    return data;
}

string *co_str_split(string_t s, string_t delim, int *count) {
    return co_split_ex(s, delim, count, true);
}

string *str_split(string_t s, string_t delim, int *count) {
    return co_split_ex(s, delim, count, false);
}

string co_concat_ex(bool use_defer, int num_args, va_list ap_copy) {
    size_t strsize = 0;
    va_list ap;

    va_copy(ap, ap_copy);
    for (int i = 0; i < num_args; i++)
        strsize += strlen(va_arg(ap, string));

    string res = try_calloc(1, strsize + 1);
    strsize = 0;

    va_copy(ap, ap_copy);
    for (int i = 0; i < num_args; i++) {
        string s = va_arg(ap, string);
        strcpy(res + strsize, s);
        strsize += strlen(s);
    }
    va_end(ap);

    if (use_defer)
        co_defer(CO_FREE, res);

    return res;
}

string co_concat_by(int num_args, ...) {
    va_list ap;
    string text;

    va_start(ap, num_args);
    text = co_concat_ex(true, num_args, ap);
    va_end(ap);

    return text;
}

string str_concat_by(int num_args, ...) {
    va_list ap;
    string text;

    va_start(ap, num_args);
    text = co_concat_ex(false, num_args, ap);
    va_end(ap);

    return text;
}

string str_replace(string_t haystack, string_t needle, string_t replace) {
    if (!haystack || !needle || !replace)
        return NULL;

    string result;
    size_t i, cnt = 0;
    size_t newWlen = strlen(replace);
    size_t oldWlen = strlen(needle);

    for (i = 0; haystack[i] != '\0'; i++) {
        if (strstr(&haystack[i], needle) == &haystack[i]) {
            cnt++;

            i += oldWlen - 1;
        }
    }

    if (cnt == 0)
        return NULL;

    result = (string)co_new_by(1, i + cnt * (newWlen - oldWlen) + 1);
    i = 0;
    while (*haystack) {
        if (strstr(haystack, needle) == haystack) {
            strcpy(&result[i], replace);
            i += newWlen;
            haystack += oldWlen;
        } else {
            result[i++] = *haystack++;
        }
    }

    result[i] = '\0';
    return result;
}

#ifdef _WIN32
static bool _is_basename_start(string_t start, string_t pos) {
    if (pos - start >= 1
        && *(pos - 1) != '/'
        && *(pos - 1) != '\\') {
        if (pos - start == 1) {
            return true;
        } else if (*(pos - 2) == '/' || *(pos - 2) == '\\') {
            return true;
        } else if (*(pos - 2) == ':'
                   && _is_basename_start(start, pos - 2)) {
            return true;
        }
    }

    return false;
}

/* Returns the filename component of the path */
string basename(string_t s) {
    string c;
    string_t comp, cend;
    size_t inc_len, cnt;
    size_t len = strlen(s);
    int state;

    comp = cend = c = (string)s;
    cnt = len;
    state = 0;
    while (cnt > 0) {
        inc_len = (*c == '\0' ? 1 : mblen(c, cnt));
        switch (inc_len) {
            case -2:
            case -1:
                inc_len = 1;
                break;
            case 0:
                goto quit_loop;
            case 1:
                if (*c == '/' || *c == '\\') {
                    if (state == 1) {
                        state = 0;
                        cend = c;
                    }
                    /* Catch relative paths in c:file.txt style. They're not to confuse
                       with the NTFS streams. This part ensures also, that no drive
                       letter traversing happens. */
                } else if ((*c == ':' && (c - comp == 1))) {
                    if (state == 0) {
                        comp = c;
                        state = 1;
                    } else {
                        cend = c;
                        state = 0;
                    }
                } else {
                    if (state == 0) {
                        comp = c;
                        state = 1;
                    }
                }
                break;
            default:
                if (state == 0) {
                    comp = c;
                    state = 1;
                }
                break;
                }
        c += inc_len;
        cnt -= inc_len;
        }

quit_loop:
    if (state == 1) {
        cend = c;
    }

    len = cend - comp;
    return co_string(comp, len);
}
#endif

#ifdef _WIN32
/* Returns directory name component of path */
string dirname(string path) {
    size_t len = strlen(path);
    string end = path + len - 1;
    unsigned int len_adjust = 0;

    /* Note that on Win32 CWD is per drive (heritage from CP/M).
     * This means dirname("c:foo") maps to "c:." or "c:" - which means CWD on C: drive.
     */
    if ((2 <= len) && isalpha((int)((u_string)path)[0]) && (':' == path[1])) {
        /* Skip over the drive spec (if any) so as not to change */
        path += 2;
        len_adjust += 2;
        if (2 == len) {
            /* Return "c:" on Win32 for dirname("c:").
             * It would be more consistent to return "c:."
             * but that would require making the string *longer*.
             */
            return path;
        }
    }

    if (len == 0) {
        /* Illegal use of this function */
        return NULL;
    }

    /* Strip trailing slashes */
    while (end >= path && IS_SLASH_P(end)) {
        end--;
    }
    if (end < path) {
        /* The path only contained slashes */
        path[0] = SLASH;
        path[1] = '\0';
        return path;
    }

    /* Strip filename */
    while (end >= path && !IS_SLASH_P(end)) {
        end--;
    }
    if (end < path) {
        /* No slash found, therefore return '.' */
        path[0] = '.';
        path[1] = '\0';
        return path;
    }

    /* Strip slashes which came before the file name */
    while (end >= path && IS_SLASH_P(end)) {
        end--;
    }
    if (end < path) {
        path[0] = SLASH;
        path[1] = '\0';
        return path;
    }
    *(end + 1) = '\0';

    return path;
}
#endif

const_t str_memrchr(const_t s, int c, size_t n) {
    u_string_t e;
    if (0 == n) {
        return NULL;
    }

    for (e = (u_string_t)s + n - 1; e >= (u_string_t)s; e--) {
        if (*e == (u_char_t)c) {
            return (const_t)e;
        }
    }

    return NULL;
}

fileinfo_t *pathinfo(string filepath) {
    fileinfo_t *file = co_new_by(1, sizeof(fileinfo_t));
    size_t path_len = strlen(filepath);
    string dir_name;
    string_t p;
    ptrdiff_t idx;

    dir_name = co_strdup(filepath);
    dirname(dir_name);
    file->dirname = dir_name;
    file->base = basename((string)file->dirname);
    file->filename = basename((string)filepath);

    p = str_memrchr((const_t)file->filename, '.', strlen(file->filename));
    if (p) {
        idx = p - file->filename;
        file->extension = file->filename + idx + 1;
    }

    return file;
}

ht_string_t *co_parse_str(string lines, string sep) {
    ht_string_t *this = ht_string_init();
    defer(hash_free, this);
    int i = 0;

    string *token = str_split(lines, sep, &i);
    for (int x = 0; x < i; x++) {
        string *parts = str_split(token[x], "=", NULL);
        if (!is_empty(parts)) {
            hash_put(this, trim(parts[0]), trim(parts[1]));
            CO_FREE(parts);
        }
    }

    if (!is_empty(token))
        CO_FREE(token);

    return this;
}

/* Make a string uppercase. */
string str_toupper(string s, size_t len) {
    u_string c;
    u_string_t e;

    c = (u_string)s;
    e = (u_string)c + len;

    while (c < e) {
        *c = toupper(*c);
        c++;
    }

    return s;
}

/* Make a string lowercase */
string str_tolower(string s, size_t len) {
    u_string c;
    u_string_t e;

    c = (u_string)s;
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

static u_char_t base64_table[65] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static u_char_t base64_decode_table[256] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x3e, 0x80, 0x80, 0x80, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x80, 0x80,
    0x80, 0x00, 0x80, 0x80, 0x80, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24,
    0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80};

static inline size_t base64_encoded_len(size_t decoded_len) {
    /* This counts the padding bytes (by rounding to the next multiple of 4). */
    return ((4u * decoded_len / 3u) + 3u) & ~3u;
}

bool is_base64(u_string_t src) {
    size_t len = strlen(src);
    for (size_t i = 0; i < len; i++) {
        if (base64_decode_table[src[i]] == 0x80)
            return false;
    }

    return true;
}

u_string base64_encode(u_string_t src) {
    u_string out, pos;
    u_string_t end, begin;
    size_t olen;
    size_t len = strlen(src);

    olen = base64_encoded_len(len) + 1 /* for NUL termination */;
    if (olen < len)
        return NULL; /* integer overflow */

    out = co_new_by(1, olen);
    end = src + len;
    begin = src;
    pos = out;
    while (end - begin >= 3) {
        *pos++ = base64_table[begin[0] >> 2];
        *pos++ = base64_table[((begin[0] & 0x03) << 4) | (begin[1] >> 4)];
        *pos++ = base64_table[((begin[1] & 0x0f) << 2) | (begin[2] >> 6)];
        *pos++ = base64_table[begin[2] & 0x3f];
        begin += 3;
    }

    if (end - begin) {
        *pos++ = base64_table[begin[0] >> 2];
        if (end - begin == 1) {
            *pos++ = base64_table[(begin[0] & 0x03) << 4];
            *pos++ = '=';
        } else {
            *pos++ = base64_table[((begin[0] & 0x03) << 4) | (begin[1] >> 4)];
            *pos++ = base64_table[(begin[1] & 0x0f) << 2];
        }
        *pos++ = '=';
    }

    *pos = '\0';

    return out;
}

u_string base64_decode(u_string_t src) {
    u_string out, pos;
    u_char block[4];
    size_t i, count, olen;
    int pad = 0;
    size_t len = strlen(src);

    count = 0;
    for (i = 0; i < len; i++) {
        if (base64_decode_table[src[i]] != 0x80)
            count++;
    }

    if (count == 0 || count % 4)
        return NULL;

    olen = (count / 4 * 3) + 1;
    pos = out = co_new_by(1, olen);

    count = 0;
    for (i = 0; i < len; i++) {
        u_char tmp = base64_decode_table[src[i]];
        if (tmp == 0x80)
            continue;

        if (src[i] == '=')
            pad++;
        block[count] = tmp;
        count++;
        if (count == 4) {
            *pos++ = (u_char)((block[0] << 2) | (block[1] >> 4));
            *pos++ = (u_char)((block[1] << 4) | (block[2] >> 2));
            *pos++ = (u_char)((block[2] << 6) | block[3]);
            count = 0;
            if (pad) {
                if (pad == 1)
                    pos--;
                else if (pad == 2)
                    pos -= 2;
                else {
                    /* Invalid padding */
                    return NULL;
                }
                break;
            }
        }
    }
    *pos = '\0';

    return out;
}
