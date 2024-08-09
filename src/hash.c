#include "coroutine.h"
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

oa_hash *oa_hash_new_ex(oa_key_ops key_ops, oa_val_ops val_ops, void (*probing_fct)(struct oa_hash_s *htable, size_t *from_idx), u32 cap);
oa_hash *oa_hash_new_lp(oa_key_ops key_ops, oa_val_ops val_ops);
void oa_hash_free(oa_hash *htable);
void_t oa_hash_put(oa_hash *htable, const_t key, const_t value);
void_t oa_hash_get(oa_hash *htable, const_t key);
bool oa_hash_has(oa_hash *htable, const_t key);
void oa_hash_delete(oa_hash *htable, const_t key);
void oa_hash_print(oa_hash *htable, void (*print_key)(const_t k), void (*print_val)(const_t v));

// Pair related
oa_pair *oa_pair_new(uint32_t hash, const_t key, const_t value);

// String operations
uint32_t oa_string_hash(const_t data, void_t arg);
void_t oa_string_cp(const_t data, void_t arg);
bool oa_string_eq(const_t data1, const_t data2, void_t arg);
void oa_string_free(void_t data, void_t arg);
void oa_string_print(const_t data);

/* Probing functions */
static inline void oa_hash_lp_idx(oa_hash *htable, size_t *idx);

enum oa_ret_ops {
    DEL,
    PUT,
    GET
};
static u32 hash_initial_capacity = HASH_INIT_CAPACITY;

static size_t oa_hash_getidx(oa_hash *htable, size_t idx, uint32_t hash_val, const_t key, enum oa_ret_ops op);
static CO_FORCE_INLINE void oa_hash_grow(oa_hash *htable);
static CO_FORCE_INLINE bool oa_hash_should_grow(oa_hash *htable);
static CO_FORCE_INLINE bool oa_hash_is_tombstone(oa_hash *htable, size_t idx);
static CO_FORCE_INLINE void oa_hash_put_tombstone(oa_hash *htable, size_t idx);

oa_hash *oa_hash_new_ex(
    oa_key_ops key_ops,
    oa_val_ops val_ops,
    void (*probing_fct)(struct oa_hash_s *htable, size_t *from_idx), u32 cap) {
    oa_hash *htable = try_calloc(1, sizeof(*htable));

    htable->size = 0;
    htable->capacity = is_zero(cap) ? hash_initial_capacity : cap;
    htable->overriden = !is_zero(cap);
    htable->val_ops = val_ops;
    htable->key_ops = key_ops;
    htable->probing_fct = probing_fct;
    htable->buckets = try_calloc(1, sizeof(oa_pair) * htable->capacity);
    htable->type = CO_OA_HASH;

    return htable;
}

CO_FORCE_INLINE oa_hash *oa_hash_new_lp(oa_key_ops key_ops, oa_val_ops val_ops) {
    return oa_hash_new_ex(key_ops, val_ops, oa_hash_lp_idx, 0);
}

void oa_hash_free(oa_hash *htable) {
    u32 i;
    if (is_type(htable, CO_OA_HASH)) {
        for (i = 0; i < htable->capacity; i++) {
            if (htable->buckets[i]) {
                if (htable->buckets[i]->key)
                    htable->key_ops.free(htable->buckets[i]->key, htable->key_ops.arg);
                if (!is_empty(htable->buckets[i]->value))
                    htable->val_ops.free(htable->buckets[i]->value);
            }

            CO_FREE(htable->buckets[i]);
        }

        if (htable->buckets)
            CO_FREE(htable->buckets);

        memset(htable, 0, sizeof(value_types));
        CO_FREE(htable);
    }
}

static CO_FORCE_INLINE void oa_hash_grow(oa_hash *htable) {
    u32 i, old_capacity;
    oa_pair **old_buckets;
    oa_pair *crt_pair;

    uint64_t new_capacity_64 = (uint64_t)htable->capacity * HASH_GROWTH_FACTOR;
    if (new_capacity_64 > SIZE_MAX)
        co_panic("re-size overflow");

    old_capacity = htable->capacity;
    old_buckets = htable->buckets;

    htable->capacity = (u32)new_capacity_64;
    htable->size = 0;
    htable->buckets = try_calloc(1, htable->capacity * sizeof(*(htable->buckets)));
    for (i = 0; i < old_capacity; i++) {
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

static CO_FORCE_INLINE bool oa_hash_should_grow(oa_hash *htable) {
    return (htable->size / htable->capacity) > (htable->overriden ? .95 : HASH_LOAD_FACTOR);
}

void_t oa_hash_put(oa_hash *htable, const_t key, const_t value) {
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
    return htable->buckets[ idx ];
}

void_t oa_hash_replace(oa_hash *htable, const_t key, const_t value) {
    if (oa_hash_should_grow(htable)) {
        oa_hash_grow(htable);
    }

    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % htable->capacity;

    // // Probing for the next good index
    idx = oa_hash_getidx(htable, idx, hash_val, key, PUT);

    if (NULL == htable->buckets[ idx ]) {
        htable->buckets[ idx ] = oa_pair_new(
            hash_val,
            htable->key_ops.cp(key, htable->key_ops.arg),
            htable->val_ops.cp(value, htable->val_ops.arg));
    } else {
        // Update the new values
        memcpy(htable->buckets[ idx ]->value, value, sizeof(htable->buckets[ idx ]->value));
        memcpy(htable->buckets[ idx ]->key, key, sizeof(htable->buckets[ idx ]->key));
        htable->buckets[ idx ]->hash = hash_val;
        --htable->size;
    }

    htable->size++;
    return htable->buckets[ idx ];
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

void_t oa_hash_get(oa_hash *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % htable->capacity;

    if (NULL == htable->buckets[ idx ]) {
        return NULL;
    }

    idx = oa_hash_getidx(htable, idx, hash_val, key, GET);

    return (NULL == htable->buckets[ idx ]) ? NULL : htable->buckets[ idx ]->value;
}

bool oa_hash_has(oa_hash *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % htable->capacity;

    if (NULL == htable->buckets[ idx ]) {
        return false;
    }

    idx = oa_hash_getidx(htable, idx, hash_val, key, GET);

    return (NULL == htable->buckets[idx]) ? false : true;
}

void oa_hash_delete(oa_hash *htable, const_t key) {
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

void oa_hash_remove(oa_hash *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % htable->capacity;

    if (NULL == htable->buckets[ idx ]) {
        return;
    }

    if (htable->buckets[idx]->value)
        htable->buckets[idx]->value = NULL;
    --htable->size;
}

void oa_hash_print(oa_hash *htable, void (*print_key)(const_t k), void (*print_val)(const_t v)) {
    oa_pair *pair;
    u32 i;

    printf("Hash Capacity: %zu\n", (size_t)htable->capacity);
    printf("Hash Size: %zu\n", htable->size);

    printf("Hash Buckets:\n");
    for (i = 0; i < htable->capacity; i++) {
        pair = htable->buckets[ i ];
        if (NULL != pair) {
            printf("\tbucket[%d]:\n", i);
            if (oa_hash_is_tombstone(htable, i)) {
                printf("\t\t TOMBSTONE");
            } else {
                printf("\t\thash=%" PRIu32 ", key=", pair->hash);
                print_key(pair->key);
                printf(", value=");
                print_val(pair->value);
            }
            printf("\n");
        }
    }
}

void_t oa_hash_iter(oa_hash *htable, void_t variable, hash_iter_func func) {
    oa_pair *pair;
    u32 i;
    for (i = 0; i < htable->capacity; i++) {
        pair = htable->buckets[ i ];
        if (NULL != pair) {
            variable = func(variable, pair->key, pair->value);
        }
    }

    return variable;
}

static size_t oa_hash_getidx(oa_hash *htable, size_t idx, uint32_t hash_val, const_t key, enum oa_ret_ops op) {
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

oa_pair *oa_pair_new(uint32_t hash, const_t key, const_t value) {
    oa_pair *p = try_calloc(1, sizeof(*p) + sizeof(key) + sizeof(value) + 2);

    p->hash = hash;
    p->value = (void_t)value;
    p->key = (void_t)key;
    return p;
}

// Probing functions
static inline void oa_hash_lp_idx(oa_hash *htable, size_t *idx) {
    (*idx)++;
    if ((*idx) == htable->capacity) {
        (*idx) = 0;
    }
}

bool oa_string_eq(const_t data1, const_t data2, void_t arg) {
    string_t str1 = (string_t)data1;
    string_t str2 = (string_t)data2;
    return !(strcmp(str1, str2)) ? true : false;
}

// String operations
static uint32_t oa_hash_fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x3243f6a9U;
    h ^= h >> 16;
    return h;
}

uint32_t oa_string_hash(const_t data, void_t arg) {
    // djb2
    uint32_t hash = (const uint32_t)5381;
    string_t str = (string_t)data;
    char c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return oa_hash_fmix32(hash);
}

void_t oa_string_cp(const_t data, void_t arg) {
    string_t input = (string_t)data;
    size_t input_length = strlen(input) + 1;
    char *result = try_calloc(1, sizeof(*result) * input_length + sizeof(values_t) + 1);

    co_strcpy(result, input, input_length);
    return result;
}

CO_FORCE_INLINE bool oa_coroutine_eq(const_t data1, const_t data2, void_t arg) {
    return memcmp(data1, data2, sizeof(co_active())) == 0 ? true : false;
}

CO_FORCE_INLINE void_t oa_coroutine_cp(const_t data, void_t arg) {
    return (routine_t *)data;
}

CO_FORCE_INLINE void_t oa_channel_cp(const_t data, void_t arg) {
    return (channel_t *)data;
}

void_t oa_value_cp(const_t data, void_t arg) {
    values_t *result = try_calloc(1, sizeof(data) + sizeof(values_t) + 1);

    memcpy(result, data, sizeof(data));
    return result;
}

CO_FORCE_INLINE bool oa_value_eq(const_t data1, const_t data2, void_t arg) {
    return memcmp(data1, data2, sizeof(data2)) == 0 ? true : false;
}

CO_FORCE_INLINE void oa_string_free(void_t data, void_t arg) {
    CO_FREE(data);
}

CO_FORCE_INLINE void oa_string_print(const_t data) {
    printf("%s", (string_t)data);
}

oa_key_ops oa_key_ops_string = { oa_string_hash, oa_string_cp, oa_string_free, oa_string_eq, NULL };
oa_val_ops oa_val_ops_struct = { oa_coroutine_cp, FUNC_VOID(co_delete), oa_value_eq, NULL };
oa_val_ops oa_val_ops_string = { oa_string_cp, CO_FREE, oa_string_eq, NULL };
oa_val_ops oa_val_ops_value = { oa_value_cp, CO_FREE, oa_value_eq, NULL };
oa_val_ops oa_val_ops_channel = {oa_channel_cp, FUNC_VOID(channel_free), oa_value_eq, NULL};

CO_FORCE_INLINE wait_group_t *ht_event_init(u32 size) {
    return (wait_group_t *)oa_hash_new_ex(oa_key_ops_string, oa_val_ops_struct, oa_hash_lp_idx, size);
}

CO_FORCE_INLINE wait_group_t *ht_group_init(void) {
    return (wait_group_t *)oa_hash_new_ex(oa_key_ops_string, oa_val_ops_struct, oa_hash_lp_idx, 0);
}

CO_FORCE_INLINE ht_string_t *ht_string_init(void) {
    return (ht_string_t *)oa_hash_new_ex(oa_key_ops_string, oa_val_ops_string, oa_hash_lp_idx, 0);
}

CO_FORCE_INLINE wait_result_t *ht_result_init(void) {
    return (wait_result_t *)oa_hash_new_ex(oa_key_ops_string, oa_val_ops_value, oa_hash_lp_idx, 0);
}

CO_FORCE_INLINE chan_collector_t *ht_channel_init(void) {
    return (chan_collector_t *)oa_hash_new_ex(oa_key_ops_string, oa_val_ops_channel, oa_hash_lp_idx, 0);
}

CO_FORCE_INLINE void hash_capacity(u32 buckets) {
    hash_initial_capacity = buckets;
}

CO_FORCE_INLINE void hash_free(hash_t *htable) {
    oa_hash_free(htable);
}

CO_FORCE_INLINE void_t hash_put(hash_t *htable, const_t key, const_t value) {
    return oa_hash_put(htable, key, value);
}

CO_FORCE_INLINE void_t hash_replace(hash_t *htable, const_t key, const_t value) {
    return oa_hash_replace(htable, key, value);
}

CO_FORCE_INLINE void_t hash_get(hash_t *htable, const_t key) {
    return oa_hash_get(htable, key);
}

CO_FORCE_INLINE bool hash_has(hash_t *htable, const_t key) {
    return oa_hash_has(htable, key);
}

CO_FORCE_INLINE size_t hash_count(hash_t *htable) {
    return htable->size;
}

CO_FORCE_INLINE void_t hash_iter(oa_hash *htable, void_t variable, hash_iter_func func) {
    return oa_hash_iter(htable, variable, func);
}

CO_FORCE_INLINE void hash_delete(hash_t *htable, const_t key) {
    oa_hash_delete(htable, key);
}

CO_FORCE_INLINE void hash_remove(hash_t *htable, const_t key) {
    oa_hash_remove(htable, key);
}

CO_FORCE_INLINE void hash_print(hash_t *htable) {
    oa_hash_print(htable, oa_string_print, oa_string_print);
}

CO_FORCE_INLINE void hash_print_custom(hash_t *htable, void (*print_key)(const_t k), void (*print_val)(const_t v)) {
    oa_hash_print(htable, print_key, print_val);
}
