#include "../include/coroutine.h"

static void slice_free(slice_t *array) {
    array_item_t *next;

    if (!array)
        return;

    while (array->head) {
        CO_FREE((void *)array->head->key);
        next = array->head->next;
        CO_FREE(array->head);
        array->head = next;
    }

    CO_FREE(array);
}

static void slice_set(slice_t *array, const char *key, void *value) {
    array_item_t *item;

    item = (array_item_t *)CO_CALLOC(1, sizeof(array_item_t));
    item->key = CO_CALLOC(1, sizeof(key));

#if defined(_WIN32) || defined(_WIN64)
    strcpy_s((char *)item->key, sizeof(item->key), key);
#else
    strcpy((char *)item->key, key);
#endif
    item->value = value;
    item->prev = array->tail;
    item->next = NULL;

    array->tail = item;
    array->length++;

    if (!array->head)
        array->head = item;
    else
        item->prev->next = item;
}

slice_t *slice(array_t *array, int start, int end) {
    if (array->as != MAP_ARRAY)
        co_panic("slice() only accept `array` type!");

    slice_t *slice = (slice_t *)CO_CALLOC(1, sizeof(slice_t));

    for (int i = start; i < end; i++) {
        const char *key = co_itoa(i);
        slice_set(slice, key, map_get(array, key));
    }

    array->slice = slice;
    slice->dict = array->dict;

    return slice;
}

map_t *map_new(map_value_dtor dtor) {
    map_t *array = (map_t *)CO_CALLOC(1, sizeof(map_t));
    array->started = false;
    array->dtor = dtor;
    array->dict = co_ht_map_init();
    return array;
}

map_t *map_long_init() {
    map_t *array = (map_t *)CO_CALLOC(1, sizeof(map_t));
    array->started = false;
    array->dtor = NULL;
    array->dict = co_ht_map_long_init();
    return array;
}

map_t *map_string_init() {
    map_t *array = (map_t *)CO_CALLOC(1, sizeof(map_t));
    array->started = false;
    array->dtor = NULL;
    array->dict = co_ht_map_string_init();
    return array;
}

array_t *array(map_value_dtor dtor, int n_args, ...) {
    array_t *array = map_new(dtor);
    va_list argp;
    void *p;

    va_start(argp, n_args);
    for (int i = 0; i < n_args; i++) {
        p = va_arg(argp, void *);
        map_push(array, p);
    }
    va_end(argp);

    array->as = MAP_ARRAY;
    return array;
}

array_t *range(int start, int stop) {
    array_t *array = map_long_init();

    array->type = CO_LLONG;
    for (int i = start; i < stop; i++) {
        map_push(array, &i);
    }

    array->as = MAP_ARRAY;
    return array;
}

array_t *array_long(int n_args, ...) {
    array_t *array = map_long_init();
    va_list argp;
    long long p;

    array->type = CO_LLONG;
    va_start(argp, n_args);
    for (int i = 0; i < n_args; i++) {
        p = va_arg(argp, size_t);
        map_push(array, &p);
    }
    va_end(argp);

    array->as = MAP_ARRAY;
    return array;
}

array_t *array_str(int n_args, ...) {
    array_t *array = map_string_init();
    va_list argp;
    char *s;

    array->type = CO_STRING;
    va_start(argp, n_args);
    for (int i = 0; i < n_args; i++) {
        s = va_arg(argp, char *);
        map_push(array, s);
    }
    va_end(argp);

    array->as = MAP_ARRAY;
    return array;
}

map_t *map_long(int n_of_Pairs, ...) {
    map_t *array = map_long_init();
    va_list argp;
    const char *k;
    long long p;

    array->type = CO_LLONG;
    va_start(argp, n_of_Pairs);
    for (int i = 0; i < (n_of_Pairs * 2); i = i + 2) {
        k = va_arg(argp, char *);
        p = va_arg(argp, size_t);
        map_put(array, k, &p);
    }
    va_end(argp);

    array->as = MAP_HASH;
    return array;
}

map_t *map_str(int n_of_Pairs, ...) {
    map_t *array = map_string_init();
    va_list argp;
    const char *k;
    char *s;

    array->type = CO_STRING;
    va_start(argp, n_of_Pairs);
    for (int i = 0; i < (n_of_Pairs * 2); i = i + 2) {
        k = va_arg(argp, char *);
        s = va_arg(argp, char *);
        map_put(array, k, s);
    }
    va_end(argp);

    array->as = MAP_HASH;
    return array;
}

map_t *map_for(map_value_dtor dtor, char *desc, ...) {
    map_t *array = map_new(dtor);
    va_list argp;
    const char *k;
    long long i;
    char c, *s;
    void *p;

    array->type = CO_NULL;
    va_start(argp, desc);
    while (*desc) {
        k = va_arg(argp, char *);
        switch (*desc++) {
            case 'i':
                // integer argument
                i = va_arg(argp, size_t);
                array_put_long(array, k, i);
                break;
            case 'c':
                // character argument
                c = (char)va_arg(argp, int);
                array_put_long(array, k, c);
                break;
            case 's':
                // string argument
                s = va_arg(argp, char *);
                map_put(array, k, s);
                break;
            case 'p':
                // void pointer (any arbitrary pointer) argument
                p = va_arg(argp, void *);
                map_put(array, k, p);
                break;
            default:
                break;
        }
    }
    va_end(argp);

    array->as = MAP_HASH;
    return array;
}

map_t *map(map_value_dtor dtor, int n_of_Pairs, ...) {
    map_t *array = map_new(dtor);
    va_list argp;
    map_value_t *p;
    const char *k;

    array->type = CO_OBJ;
    va_start(argp, n_of_Pairs);
    for (int i = 0; i < (n_of_Pairs * 2); i = i + 2) {
        k = va_arg(argp, char *);
        p = va_arg(argp, void *);
        map_put(array, k, (map_value_t *)p);
    }
    va_end(argp);

    array->as = MAP_HASH;
    return array;
}

void map_free(map_t *array) {
    array_item_t *next;

    if (!array)
        return;

    while (array->head) {
        if (array->dtor)
            array->dtor(array->head->value);

        co_hash_delete(array->dict, array->head->key);
        next = array->head->next;
        CO_FREE(array->head);
        array->head = next;
    }

    co_hash_free(array->dict);
    if (array->slice != NULL)
        slice_free(array->slice);

    CO_FREE(array);
}

int map_push(map_t *array, void *value) {
    array_item_t *item;
    oa_pair *kv;

    if (!array->started) {
        array->started = true;
        array->indices = 0;
    } else {
        array->indices++;
    }

    item = (array_item_t *)CO_CALLOC(1, sizeof(array_item_t));
    item->indic = array->indices;
    kv = (oa_pair *)co_hash_put(array->dict, co_itoa(item->indic), value);
    item->key = kv->key;
    item->value = kv->value;
    item->prev = array->tail;
    item->next = NULL;

    array->tail = item;
    array->length++;

    if (!array->head)
        array->head = item;
    else
        item->prev->next = item;

    return item->indic;
}

map_value_t *map_pop(map_t *array) {
    map_value_t *value;
    array_item_t *item;

    if (!array || !array->tail)
        return NULL;

    item = array->tail;
    array->tail = array->tail->prev;
    array->length--;

    if (array->length == 0)
        array->head = NULL;

    value = item->value;
    co_hash_delete(array->dict, co_itoa(item->indic));
    CO_FREE(item);

    return value;
}

void map_shift(map_t *array, void *value) {
    array_item_t *item;
    oa_pair *kv;

    if (!array)
        return;

    if (!array->started) {
        array->started = true;
        array->indices = 0;
    } else {
        array->indices++;
    }

    item = (array_item_t *)CO_CALLOC(1, sizeof(array_item_t));
    item->prev = NULL;
    item->next = array->head;
    if (array->head == NULL)
        item->indic = 0;
    else
        item->indic = --item->next->indic;

    array->head = item;
    array->length++;

    if (!array->tail)
        array->tail = item;
    else
        item->next->prev = item;

    kv = (oa_pair *)co_hash_put(array->dict, co_itoa(item->indic), value);
    item->key = kv->key;
    item->value = kv->value;
}

map_value_t *map_unshift(map_t *array) {
    map_value_t *value;
    array_item_t *item;

    if (!array || !array->head)
        return NULL;

    item = array->head;
    array->head = array->head->next;
    array->length--;

    if (array->length == 0)
        array->tail = NULL;

    value = item->value;
    co_hash_delete(array->dict, co_itoa(item->indic));
    CO_FREE(item);

    return value;
}

size_t map_count(map_t *array) {
    if (array)
        return array->length;

    return 0;
}

void *map_remove(map_t *array, void *value) {
    array_item_t *item;

    if (!array)
        return NULL;

    for (item = array->head; item != NULL; item = item->next) {
        if (memcmp(item->value, value, sizeof(item->value)) == 0) {
            if (item->prev)
                item->prev->next = item->next;
            else
                array->head = item->next;

            if (item->next)
                item->next->prev = item->prev;
            else
                array->tail = item->prev;

            co_hash_delete(array->dict, co_itoa(item->indic));
            array->length--;

            return value;
        }
    }

    return NULL;
}

map_value_t *map_get(map_t *array, const char *key) {
    return (map_value_t *)co_hash_get(array->dict, key);
}

void array_put_long(map_t *array, const char *key, long long value) {
    map_put(array, key, &value);
}

void array_put_str(map_t *array, const char *key, const char *value) {
    map_put(array, key, (char *)value);
}

void map_put(map_t *array, const char *key, void *value) {
    array_item_t *item;
    oa_pair *kv;
    void *has = co_hash_get(array->dict, key);
    if (has == NULL) {
        if (!array->started) {
            array->started = true;
            array->indices = 0;
        } else {
            array->indices++;
        }

        item = (array_item_t *)CO_CALLOC(1, sizeof(array_item_t));
        item->indic = array->indices;
        kv = (oa_pair *)co_hash_put(array->dict, key, value);
        item->key = kv->key;
        item->value = kv->value;
        item->prev = array->tail;
        item->next = NULL;

        array->tail = item;
        array->length++;

        if (!array->head)
            array->head = item;
        else
            item->prev->next = item;
    } else {
        for (item = array->head; item; item = item->next) {
            if (memcmp(item->value, has, sizeof(item->value)) == 0) {
                kv = (oa_pair *)co_hash_put(array->dict, key, value);
                item->key = kv->key;
                item->value = kv->value;
                break;
            }
        }
    }
}

map_iter_t *iter_new(map_t *array, bool forward) {

    if (array && array->head) {
        map_iter_t *iterator;

        iterator = (map_iter_t *)CO_MALLOC(sizeof(map_iter_t));
        iterator->array = array;
        iterator->item = forward ? array->head : array->tail;
        iterator->forward = forward;

        return iterator;
    }

    return NULL;
}

map_iter_t *iter_next(map_iter_t *iterator) {
    if (iterator) {
        array_item_t *item;

        item = iterator->forward ? iterator->item->next : iterator->item->prev;
        if (item) {
            iterator->item = item;
            return iterator;
        } else {
            CO_FREE(iterator);
            return NULL;
        }
    }

    return NULL;
}

map_value iter_value(map_iter_t *iterator) {
    if (iterator)
        return iterator->item->value->value;

    return ((map_value_t *)NULL)->value;
}

const char *iter_key(map_iter_t *iterator) {
    if (iterator)
        return iterator->item->key;

    return NULL;
}

map_iter_t *iter_remove(map_iter_t *iterator) {
    array_item_t *item;

    if (!iterator)
        return NULL;

    item = iterator->forward ? iterator->array->head : iterator->array->tail;
    while (item) {
        if (iterator->item == item) {
            if (iterator->array->head == item)
                iterator->array->head = item->next;
            else
                item->prev->next = item->next;

            if (iterator->array->tail == item)
                iterator->array->tail = item->prev;
            else
                item->next->prev = item->prev;

            iterator->array->length--;

            iterator->item = iterator->forward ? item->next : item->prev;
            CO_FREE(item);
            if (iterator->item) {
                return iterator;
            } else {
                CO_FREE(iterator);
                return NULL;
            }
        }

        item = iterator->forward ? item->next : item->prev;
    }

    return iterator;
}

void iter_free(map_iter_t *iterator) {
    if (iterator)
        CO_FREE(iterator);
}