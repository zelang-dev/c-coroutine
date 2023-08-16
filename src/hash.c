#include "../include/coroutine.h"
/*
MIT License

Copyright (c) 2021 Andrei Ciobanu

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

A open addressing hash table implemented in C.

Code associated with the following article:
https://www.andreinc.net/2021/10/02/implementing-hash-tables-in-c-part-1

*/

oa_hash *oa_hash_new(oa_key_ops key_ops, oa_val_ops val_ops, void (*probing_fct)(struct oa_hash_s *htable, size_t *from_idx));
oa_hash *oa_hash_new_lp(oa_key_ops key_ops, oa_val_ops val_ops);
void oa_hash_free(oa_hash *htable);
void *oa_hash_put(oa_hash *htable, const void *key, const void *value);
void *oa_hash_get(oa_hash *htable, const void *key);
void oa_hash_delete(oa_hash *htable, const void *key);
void oa_hash_print(oa_hash *htable, void (*print_key)(const void *k), void (*print_val)(const void *v));

// Pair related
oa_pair *oa_pair_new(uint32_t hash, const void *key, const void *value);

// String operations
uint32_t oa_string_hash(const void *data, void *arg);
void *oa_string_cp(const void *data, void *arg);
bool oa_string_eq(const void *data1, const void *data2, void *arg);
void oa_string_free(void *data, void *arg);
void oa_string_print(const void *data);

/* Probing functions */
static inline void oa_hash_lp_idx(oa_hash *htable, size_t *idx);

enum oa_ret_ops {
    DEL,
    PUT,
    GET
};

static size_t oa_hash_getidx(oa_hash *htable, size_t idx, uint32_t hash_val, const void *key, enum oa_ret_ops op);
static inline void oa_hash_grow(oa_hash *htable);
static inline bool oa_hash_should_grow(oa_hash *htable);
static inline bool oa_hash_is_tombstone(oa_hash *htable, size_t idx);
static inline void oa_hash_put_tombstone(oa_hash *htable, size_t idx);

oa_hash *oa_hash_new(
    oa_key_ops key_ops,
    oa_val_ops val_ops,
    void (*probing_fct)(struct oa_hash_s *htable, size_t *from_idx)) {
    oa_hash *htable;

    htable = CO_MALLOC(sizeof(*htable));
    if (NULL == htable)
        co_panic("malloc() failed");

    htable->size = 0;
    htable->capacity = OA_HASH_INIT_CAPACITY;
    htable->val_ops = val_ops;
    htable->key_ops = key_ops;
    htable->probing_fct = probing_fct;

    htable->buckets = CO_MALLOC(sizeof(*(htable->buckets)) * htable->capacity);
    if (NULL == htable->buckets)
        co_panic("malloc() failed");

    for (int i = 0; i < htable->capacity; i++) {
        htable->buckets[ i ] = NULL;
    }

    return htable;
}

oa_hash *oa_hash_new_lp(oa_key_ops key_ops, oa_val_ops val_ops) {
    return oa_hash_new(key_ops, val_ops, oa_hash_lp_idx);
}

void oa_hash_free(oa_hash *htable) {
    for (int i = 0; i < htable->capacity; i++) {
        if (NULL != htable->buckets[ i ]) {
            htable->key_ops.free(htable->buckets[ i ]->key, htable->key_ops.arg);
            if (htable->buckets[ i ]->value != NULL)
                htable->val_ops.free(htable->buckets[ i ]->value);

            htable->buckets[ i ]->value = NULL;
        }
        if (htable->buckets[ i ] != NULL)
            CO_FREE(htable->buckets[ i ]);

        htable->buckets[ i ] = NULL;
    }

    if (htable->buckets != NULL)
        CO_FREE(htable->buckets);

    htable->buckets = NULL;
    CO_FREE(htable);
}

inline static void oa_hash_grow(oa_hash *htable) {
    uint32_t old_capacity;
    oa_pair **old_buckets;
    oa_pair *crt_pair;

    uint64_t new_capacity_64 = (uint64_t)htable->capacity * OA_HASH_GROWTH_FACTOR;
    if (new_capacity_64 > SIZE_MAX)
        co_panic("re-size overflow");

    old_capacity = (uint32_t)htable->capacity;
    old_buckets = htable->buckets;

    htable->capacity = (uint32_t)new_capacity_64;
    htable->size = 0;
    htable->buckets = CO_CALLOC(1, htable->capacity * sizeof(*(htable->buckets)));

    if (NULL == htable->buckets)
        co_panic("calloc() failed");

    for (int i = 0; i < htable->capacity; i++) {
        htable->buckets[ i ] = NULL;
    };

    for (size_t i = 0; i < old_capacity; i++) {
        crt_pair = old_buckets[ i ];
        if (NULL != crt_pair && !oa_hash_is_tombstone(htable, i)) {
            oa_hash_put(htable, crt_pair->key, crt_pair->value);
            htable->key_ops.free(crt_pair->key, htable->key_ops.arg);
            htable->val_ops.free(crt_pair->value);
            CO_FREE(crt_pair);
        }
    }

    CO_FREE(old_buckets);
}

inline static bool oa_hash_should_grow(oa_hash *htable) {
    return (htable->size / htable->capacity) > OA_HASH_LOAD_FACTOR;
}

void *oa_hash_put(oa_hash *htable, const void *key, const void *value) {

    if (oa_hash_should_grow(htable)) {
        oa_hash_grow(htable);
    }

    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % htable->capacity;

    if (NULL == htable->buckets[ idx ]) {
        // Key doesn't exist & we add it anew
        htable->buckets[ idx ] = oa_pair_new(
            hash_val,
            htable->key_ops.cp(key, htable->key_ops.arg),
            htable->val_ops.cp(value, htable->val_ops.arg));
    } else {
        // // Probing for the next good index
        idx = oa_hash_getidx(htable, idx, hash_val, key, PUT);

        if (NULL == htable->buckets[ idx ]) {
            htable->buckets[ idx ] = oa_pair_new(
                hash_val,
                htable->key_ops.cp(key, htable->key_ops.arg),
                htable->val_ops.cp(value, htable->val_ops.arg));
        } else {
            // Update the existing value
            // Free the old values
            htable->val_ops.free(htable->buckets[ idx ]->value);
            htable->key_ops.free(htable->buckets[ idx ]->key, htable->key_ops.arg);
            // Update the new values
            htable->buckets[ idx ]->value = htable->val_ops.cp(value, htable->val_ops.arg);
            htable->buckets[ idx ]->key = htable->val_ops.cp(key, htable->key_ops.arg);
            htable->buckets[ idx ]->hash = hash_val;
            --htable->size;
        }
    }

    htable->size++;
    return htable->buckets[ idx ]->value;
}

inline static bool oa_hash_is_tombstone(oa_hash *htable, size_t idx) {
    if (NULL == htable->buckets[ idx ]) {
        return false;
    }
    if (NULL == htable->buckets[ idx ]->key &&
        NULL == htable->buckets[ idx ]->value &&
        0 == htable->buckets[ idx ]->key) {
        return true;
    }
    return false;
}

inline static void oa_hash_put_tombstone(oa_hash *htable, size_t idx) {
    if (NULL != htable->buckets[ idx ]) {
        htable->buckets[ idx ]->hash = 0;
        htable->buckets[ idx ]->key = NULL;
        htable->buckets[ idx ]->value = NULL;
    }
}

void *oa_hash_get(oa_hash *htable, const void *key) {
    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % htable->capacity;

    if (NULL == htable->buckets[ idx ]) {
        return NULL;
    }

    idx = oa_hash_getidx(htable, idx, hash_val, key, GET);

    return (NULL == htable->buckets[ idx ]) ? NULL : htable->buckets[ idx ]->value;
}

void oa_hash_delete(oa_hash *htable, const void *key) {
    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % htable->capacity;

    if (NULL == htable->buckets[ idx ]) {
        return;
    }

    idx = oa_hash_getidx(htable, idx, hash_val, key, DEL);
    if (NULL == htable->buckets[ idx ]) {
        return;
    }

    htable->val_ops.free(htable->buckets[ idx ]->value);
    htable->key_ops.free(htable->buckets[ idx ]->key, htable->key_ops.arg);
    --htable->size;

    oa_hash_put_tombstone(htable, idx);
}

void oa_hash_print(oa_hash *htable, void (*print_key)(const void *k), void (*print_val)(const void *v)) {

    oa_pair *pair;

    printf("Hash Capacity: %zu\n", (size_t)htable->capacity);
    printf("Hash Size: %zu\n", htable->size);

    printf("Hash Buckets:\n");
    for (int i = 0; i < htable->capacity; i++) {
        pair = htable->buckets[ i ];
        printf("\tbucket[%d]:\n", i);
        if (NULL != pair) {
            if (oa_hash_is_tombstone(htable, i)) {
                printf("\t\t TOMBSTONE");
            } else {
                printf("\t\thash=%" PRIu32 ", key=", pair->hash);
                print_key(pair->key);
                printf(", value=");
                print_val(pair->value);
            }
        }
        printf("\n");
    }
}

static size_t oa_hash_getidx(oa_hash *htable, size_t idx, uint32_t hash_val, const void *key, enum oa_ret_ops op) {
    do {
        if (op == PUT && oa_hash_is_tombstone(htable, idx)) {
            break;
        }
        if (htable->buckets[ idx ]->hash == hash_val &&
            htable->key_ops.eq(key, htable->buckets[ idx ]->key, htable->key_ops.arg)) {
            break;
        }
        htable->probing_fct(htable, &idx);
    } while (NULL != htable->buckets[ idx ]);
    return idx;
}

// Pair related

oa_pair *oa_pair_new(uint32_t hash, const void *key, const void *value) {
    oa_pair *p;
    p = CO_CALLOC(1, sizeof(p));
    if (NULL == p)
        co_panic("calloc() failed");

    p->hash = hash;
    p->value = (void *)value;
    p->key = (void *)key;
    return p;
}

// Probing functions
static inline void oa_hash_lp_idx(oa_hash *htable, size_t *idx) {
    (*idx)++;
    if ((*idx) == htable->capacity) {
        (*idx) = 0;
    }
}

bool oa_string_eq(const void *data1, const void *data2, void *arg) {
    const char *str1 = (const char *)data1;
    const char *str2 = (const char *)data2;
    return !(strcmp(str1, str2)) ? true : false;
}

// String operations
static uint32_t oa_hash_fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x3243f6a9U;
    h ^= h >> 16;
    return h;
}

uint32_t oa_string_hash(const void *data, void *arg) {

    // djb2
    uint32_t hash = (const uint32_t)5381;
    const char *str = (const char *)data;
    char c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return oa_hash_fmix32(hash);
}

void *oa_string_cp(const void *data, void *arg) {
    const char *input = (const char *)data;
    size_t input_length = strlen(input) + 1;
    char *result;
    size_t copy_size = sizeof(result) * input_length;
    result = CO_CALLOC(1, copy_size);
    if (NULL == result)
        co_panic("calloc() failed");

#if defined(_WIN32) || defined(_WIN64)
    strcpy_s(result, copy_size, input);
#else
    strcpy(result, input);
#endif
    return result;
}

bool oa_coroutine_eq(const void *data1, const void *data2, void *arg) {
    return memcmp(data1, data2, sizeof(co_active())) == 0 ? true : false;
}

void *oa_coroutine_cp(const void *data, void *arg) {
    co_routine_t *input = (co_routine_t *)data;
    return input;
}

void *oa_value_cp(const void *data, void *arg) {
    co_value_t *result = CO_CALLOC(1, sizeof(data));
    if (NULL == result)
        co_panic("calloc() failed");

    memcpy(result, data, sizeof(result));
    return result;
}

bool oa_value_eq(const void *data1, const void *data2, void *arg) {
    return memcmp(data1, data2, sizeof(data2)) == 0 ? true : false;
}

void oa_string_free(void *data, void *arg) {
    CO_FREE(data);
}

void oa_string_print(const void *data) {
    printf("%s", (const char *)data);
}

void oa_map_free(void *data) {}
oa_key_ops oa_key_ops_string = { oa_string_hash, oa_string_cp, oa_string_free, oa_string_eq, NULL };
oa_val_ops oa_val_ops_struct = { oa_coroutine_cp, CO_DEFER(co_delete), oa_coroutine_eq, NULL };
oa_val_ops oa_val_ops_value = { oa_value_cp, CO_FREE, oa_value_eq, NULL };
oa_val_ops oa_val_ops_map = { oa_value_cp, oa_map_free, oa_value_eq, NULL };

CO_FORCE_INLINE co_ht_group_t *co_ht_group_init() {
    return (co_ht_group_t *)oa_hash_new(oa_key_ops_string, oa_val_ops_struct, oa_hash_lp_idx);
}

CO_FORCE_INLINE co_ht_result_t *co_ht_result_init() {
    return (co_ht_result_t *)oa_hash_new(oa_key_ops_string, oa_val_ops_value, oa_hash_lp_idx);
}

CO_FORCE_INLINE co_ht_map_t *co_ht_map_init() {
    return (co_ht_map_t *)oa_hash_new(oa_key_ops_string, oa_val_ops_map, oa_hash_lp_idx);
}

CO_FORCE_INLINE void co_hash_free(co_hast_t *htable) {
    oa_hash_free(htable);
}

CO_FORCE_INLINE void *co_hash_put(co_hast_t *htable, const void *key, const void *value) {
    return oa_hash_put(htable, key, value);
}

CO_FORCE_INLINE void *co_hash_get(co_hast_t *htable, const void *key) {
    return oa_hash_get(htable, key);
}

CO_FORCE_INLINE void co_hash_delete(co_hast_t *htable, const void *key) {
    oa_hash_delete(htable, key);
}

CO_FORCE_INLINE void co_hash_print(co_hast_t *htable, void (*print_key)(const void *k), void (*print_val)(const void *v)) {
    oa_hash_print(htable, print_key, print_val);
}
