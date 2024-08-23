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
static CO_FORCE_INLINE void oa_hash_lp_idx(oa_hash *htable, size_t *idx);

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
    u32 i, capacity = is_zero(cap) ? hash_initial_capacity : cap;
    atomic_init(&htable->size, 0);
    atomic_init(&htable->capacity, capacity);
    htable->overriden = !is_zero(cap);
    htable->val_ops = val_ops;
    htable->key_ops = key_ops;
    htable->probing_fct = probing_fct;
    atomic_init(&htable->buckets, try_calloc(1, sizeof(atomic_pair_t) * capacity));
    for (i = 0; i < capacity; i++) {
        atomic_init(&htable->buckets[i], NULL);
    }
    htable->type = CO_OA_HASH;

    return htable;
}

CO_FORCE_INLINE oa_hash *oa_hash_new_lp(oa_key_ops key_ops, oa_val_ops val_ops) {
    return oa_hash_new_ex(key_ops, val_ops, oa_hash_lp_idx, 0);
}

void oa_hash_free(oa_hash *htable) {
    if (is_type(htable, CO_OA_HASH)) {
        u32 i, capacity = atomic_load(&htable->capacity);
        oa_pair **buckets = (oa_pair **)atomic_load_explicit(&htable->buckets, memory_order_consume);
        for (i = 0; i < capacity; i++) {
            if (buckets[i]) {
                if (buckets[i]->key)
                    htable->key_ops.free(buckets[i]->key, htable->key_ops.arg);

                if (!is_empty(buckets[i]->value))
                    htable->val_ops.free(buckets[i]->value);
            }

            CO_FREE(buckets[i]);
        }

        if (buckets)
            CO_FREE(buckets);

        memset(htable, 0, sizeof(value_types));
        CO_FREE(htable);
    }
}

static CO_FORCE_INLINE void oa_hash_grow(oa_hash *htable) {
    u32 i, old_capacity;
    oa_pair **old_buckets;
    oa_pair *crt_pair;

    old_capacity = atomic_load_explicit(&htable->capacity, memory_order_consume);
    uint64_t new_capacity_64 = old_capacity * HASH_GROWTH_FACTOR;
    if (new_capacity_64 > SIZE_MAX)
        co_panic("re-size overflow");

    old_buckets = (oa_pair **)atomic_load_explicit(&htable->buckets, memory_order_consume);
    atomic_init(&htable->capacity, (size_t)new_capacity_64);
    atomic_init(&htable->size, 0);
    atomic_init(&htable->buckets, try_calloc(1, new_capacity_64 * sizeof(*(old_buckets))));
    for (i = 0; i < new_capacity_64; i++) {
        atomic_init(&htable->buckets[i], NULL);
    }

    for (i = 0; i < old_capacity; i++) {
        crt_pair = old_buckets[i];
        if (!is_empty(crt_pair) && !oa_hash_is_tombstone(htable, i)) {
            oa_hash_put(htable, crt_pair->key, crt_pair->value);
            htable->key_ops.free(crt_pair->key, htable->key_ops.arg);
            htable->val_ops.free(crt_pair->value);
            CO_FREE(crt_pair);
        }
    }

    CO_FREE(old_buckets);
}

static CO_FORCE_INLINE bool oa_hash_should_grow(oa_hash *htable) {
    return (atomic_load(&htable->size) / atomic_load(&htable->capacity)) > (htable->overriden ? .95 : HASH_LOAD_FACTOR);
}

void_t oa_hash_put(oa_hash *htable, const_t key, const_t value) {
    if (oa_hash_should_grow(htable))
        oa_hash_grow(htable);

    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % (u32)atomic_load(&htable->capacity);

    oa_pair **buckets = (oa_pair **)atomic_load_explicit(&htable->buckets, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    if (is_empty(buckets[idx])) {
        // Key doesn't exist & we add it anew
        buckets[idx] = oa_pair_new(
            hash_val,
            htable->key_ops.cp(key, htable->key_ops.arg),
            htable->val_ops.cp(value, htable->val_ops.arg));
    } else {
        // // Probing for the next good index
        idx = oa_hash_getidx(htable, idx, hash_val, key, PUT);

        if (is_empty(buckets[idx])) {
            buckets[idx] = oa_pair_new(
                hash_val,
                htable->key_ops.cp(key, htable->key_ops.arg),
                htable->val_ops.cp(value, htable->val_ops.arg));
        } else {
            // Update the existing value
            // Free the old values
            htable->val_ops.free(buckets[idx]->value);
            htable->key_ops.free(buckets[idx]->key, htable->key_ops.arg);
            // Update the new values
            buckets[idx]->value = htable->val_ops.cp(value, htable->val_ops.arg);
            buckets[idx]->key = htable->val_ops.cp(key, htable->key_ops.arg);
            buckets[idx]->hash = hash_val;
            atomic_fetch_sub(&htable->size, 1);
        }
    }

    atomic_store_explicit(&htable->buckets, buckets, memory_order_release);
    atomic_fetch_add(&htable->size, 1);
    return buckets[idx];
}

void_t oa_hash_replace(oa_hash *htable, const_t key, const_t value) {
    if (oa_hash_should_grow(htable))
        oa_hash_grow(htable);

    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % (u32)atomic_load(&htable->capacity);

    // // Probing for the next good index
    idx = oa_hash_getidx(htable, idx, hash_val, key, PUT);

    oa_pair **buckets = (oa_pair **)atomic_load_explicit(&htable->buckets, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    if (is_empty(buckets[idx])) {
        buckets[idx] = oa_pair_new(
            hash_val, htable->key_ops.cp(key, htable->key_ops.arg),
            htable->val_ops.cp(value, htable->val_ops.arg));
    } else {
        // Update the new values
        memcpy(buckets[idx]->value, value, sizeof(buckets[idx]->value));
        memcpy(buckets[idx]->key, key, sizeof(buckets[idx]->key));
        buckets[idx]->hash = hash_val;
        atomic_fetch_sub(&htable->size, 1);
    }

    atomic_store_explicit(&htable->buckets, buckets, memory_order_release);
    atomic_fetch_add(&htable->size, 1);
    return buckets[idx];
}

static CO_FORCE_INLINE bool oa_hash_is_tombstone(oa_hash *htable, size_t idx) {
    oa_pair *buckets = (oa_pair *)atomic_load(&htable->buckets[idx]);
    if (is_empty(buckets))
        return false;

    if (is_empty(buckets->key) && is_empty(buckets->value) && 0 == buckets->hash)
        return true;

    return false;
}

static CO_FORCE_INLINE void oa_hash_put_tombstone(oa_hash *htable, size_t idx) {
    if (!is_empty(atomic_get(void_t, &htable->buckets[idx]))) {
        oa_pair **buckets = (oa_pair **)atomic_load_explicit(&htable->buckets, memory_order_acquire);
        atomic_thread_fence(memory_order_seq_cst);
        buckets[idx]->hash = 0;
        buckets[idx]->key = NULL;
        buckets[idx]->value = NULL;
        atomic_store_explicit(&htable->buckets, buckets, memory_order_release);
    }
}

void_t oa_hash_get(oa_hash *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % (u32)atomic_load(&htable->capacity);

    if (is_empty(atomic_get(void_t, htable->buckets[idx])))
        return NULL;

    idx = oa_hash_getidx(htable, idx, hash_val, key, GET);

    return is_empty(atomic_get(void_t, &htable->buckets[idx]))
        ? NULL
        : (atomic_get(oa_pair *, &htable->buckets[idx]))->value;
}

bool oa_hash_has(oa_hash *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % (u32)atomic_load(&htable->capacity);

    if (is_empty(atomic_get(void_t, &htable->buckets[idx])))
        return false;

    idx = oa_hash_getidx(htable, idx, hash_val, key, GET);

    return is_empty(atomic_get(void_t, &htable->buckets[idx])) ? false : true;
}

void oa_hash_delete(oa_hash *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % (u32)atomic_load(&htable->capacity);

    if (is_empty(atomic_get(void_t, &htable->buckets[idx])))
        return;

    idx = oa_hash_getidx(htable, idx, hash_val, key, DEL);
    if (is_empty(atomic_get(void_t, &htable->buckets[idx])))
        return;

    oa_pair **buckets = (oa_pair **)atomic_load(&htable->buckets);
    htable->val_ops.free(buckets[idx]->value);
    htable->key_ops.free(buckets[idx]->key, htable->key_ops.arg);
    atomic_fetch_sub(&htable->size, 1);

    oa_hash_put_tombstone(htable, idx);
}

void oa_hash_remove(oa_hash *htable, const_t key) {
    uint32_t hash_val = htable->key_ops.hash(key, htable->key_ops.arg);
    size_t idx = hash_val % (u32)atomic_load(&htable->capacity);

    if (is_empty(atomic_get(void_t, &htable->buckets[idx])))
        return;

    oa_pair **buckets = (oa_pair **)atomic_load_explicit(&htable->buckets, memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    if (buckets[idx]->value)
        buckets[idx]->value = NULL;

    atomic_store_explicit(&htable->buckets, buckets, memory_order_release);
    atomic_fetch_sub(&htable->size, 1);
}

void oa_hash_print(oa_hash *htable, void (*print_key)(const_t k), void (*print_val)(const_t v)) {
    oa_pair *pair;
    u32 i, capacity = (u32)atomic_load(&htable->capacity);

    printf("Hash Capacity: %zu\n", (size_t)capacity);
    printf("Hash Size: %zu\n", (size_t)atomic_load(&htable->size));

    printf("Hash Buckets:\n");
    oa_pair **buckets = (oa_pair **)atomic_load(&htable->buckets);
    for (i = 0; i < capacity; i++) {
        pair = buckets[i];
        if (!is_empty(pair)) {
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
    u32 i, capacity = (u32)atomic_load(&htable->capacity);
    oa_pair **buckets = (oa_pair **)atomic_load(&htable->buckets);
    for (i = 0; i < capacity; i++) {
        pair = buckets[i];
        if (!is_empty(pair))
            variable = func(variable, pair->key, pair->value);
    }

    return variable;
}

static size_t oa_hash_getidx(oa_hash *htable, size_t idx, uint32_t hash_val, const_t key, enum oa_ret_ops op) {
    do {
        if (op == PUT && oa_hash_is_tombstone(htable, idx))
            break;

        if ((atomic_get(oa_pair *, &htable->buckets[idx]))->hash == hash_val &&
            htable->key_ops.eq(key, (atomic_get(oa_pair *, &htable->buckets[idx]))->key, htable->key_ops.arg)) {
            break;
        }

        htable->probing_fct(htable, &idx);
    } while (!is_empty(atomic_get(void_t, &htable->buckets[idx])));
    return idx;
}

oa_pair *oa_pair_new(uint32_t hash, const_t key, const_t value) {
    oa_pair *p = try_calloc(1, sizeof(*p) + sizeof(key) + sizeof(value) + sizeof(hash));

    p->hash = hash;
    p->value = (void_t)value;
    p->key = (void_t)key;
    return p;
}

// Probing functions
static CO_FORCE_INLINE void oa_hash_lp_idx(oa_hash *htable, size_t *idx) {
    (*idx)++;
    if ((*idx) == (size_t)atomic_load(&htable->capacity))
        (*idx) = 0;
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

oa_key_ops oa_key_ops_string = {oa_string_hash, oa_string_cp, oa_string_free, oa_string_eq, NULL};
oa_val_ops oa_val_ops_struct = {oa_coroutine_cp, VOID_FUNC(co_delete), oa_value_eq, NULL};
oa_val_ops oa_val_ops_string = {oa_string_cp, CO_FREE, oa_string_eq, NULL};
oa_val_ops oa_val_ops_value = {oa_value_cp, CO_FREE, oa_value_eq, NULL};
oa_val_ops oa_val_ops_channel = {oa_channel_cp, VOID_FUNC(channel_free), oa_value_eq, NULL};

CO_FORCE_INLINE wait_group_t *ht_wait_init(u32 size) {
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
