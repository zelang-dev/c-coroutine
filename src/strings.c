#include "coroutine.h"

CO_FORCE_INLINE string_t co_itoa(int64_t number) {
    simd_itoa(number, co_active()->scrape);
    return co_active()->scrape;
}

CO_FORCE_INLINE string co_strdup(string_t str) {
    return str_memdup_ex(co_scope(), str, simd_strlen(str));
}

CO_FORCE_INLINE string co_string(string_t str, size_t length) {
    return str_memdup_ex(co_scope(), str, length);
}

CO_FORCE_INLINE string *co_str_split(string_t s, string_t delim, int *count) {
    return str_split_ex(co_scope(), s, delim, count);
}

CO_FORCE_INLINE string *str_split(string_t s, string_t delim, int *count) {
    return str_split_ex(nullptr, s, delim, count);
}

string co_concat_by(int num_args, ...) {
    va_list args;

    va_start(args, num_args);
    string s = str_concat_ex(co_scope(), num_args, args);
    va_end(args);

    return s;
}

string str_concat_by(int num_args, ...) {
    va_list args;

    va_start(args, num_args);
    string s = str_concat_ex(nullptr, num_args, args);
    va_end(args);

    return s;
}

CO_FORCE_INLINE string str_replace(string_t haystack, string_t needle, string_t replace) {
    return str_replace_ex(co_scope(), haystack, needle, replace);
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
    int i = 0, x;

    string *token = str_split(lines, sep, &i);
    for (x = 0; x < i; x++) {
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

u_string base64_encode(u_string_t src) {
    return str_encode64_ex(co_scope(), src);
}

u_string base64_decode(u_string_t src) {
    return str_decode64_ex(co_scope(), src);
}
