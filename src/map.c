#include "../include/coroutine.h"

map_t *map_new(map_value_dtor dtor) {
    map_t *array = (map_t *)CO_CALLOC(1, sizeof(map_t));
    array->indices = -9999999;
    array->dtor = dtor;
    array->dict = co_ht_map_init();
    return array;
}

map_t *map(map_value_dtor dtor, int n_args, ...) {
    map_t *array = map_new(dtor);
    va_list argp;
    void *p;

    va_start(argp, n_args);
    for (int i = 0; i < n_args; i++) {
        p = va_arg(argp, void *);
        map_push(array, p);
    }
    va_end(argp);

    return array;
}

void map_free(map_t *array) {
    array_item_t *next;

    if (!array)
        return;

    while (array->first) {
        if (array->dtor)
            array->dtor(array->first->value);

        next = array->first->next;
        CO_FREE(array->first);
        array->first = next;
    }

    CO_FREE(array->dict);
    CO_FREE(array);
}

int map_push(map_t *array, void *value) {
    array_item_t *item;

    if (array->indices == -9999999)
        array->indices = 0;
    else
        array->indices++;

    item = (array_item_t *)CO_CALLOC(1, sizeof(array_item_t));
    item->indic = array->indices;
    item->value = co_hash_put(array->dict, co_itoa(item->indic), value);
    item->previous = array->last;
    item->next = NULL;

    array->last = item;
    array->length++;

    if (!array->first)
        array->first = item;
    else
        item->previous->next = item;

    return item->indic;
}

void *map_pop(map_t *array) {
    void *value;
    array_item_t *item;

    if (!array || !array->last)
        return NULL;

    item = array->last;
    array->last = array->last->previous;
    array->length--;

    if (array->length == 0)
        array->first = NULL;

    value = item->value;
    co_hash_delete(array->dict, co_itoa(item->indic));
    CO_FREE(item);

    return value;
}

void map_shift(map_t *array, void *value) {
    array_item_t *item;

    if (!array)
        return;

    if (array->indices == -9999999)
        array->indices = 0;
    else
        array->indices++;

    item = (array_item_t *)CO_CALLOC(1, sizeof(array_item_t));
    item->previous = NULL;
    item->next = array->first;
    item->value = value;
    if (array->first == NULL)
        item->indic = 0;
    else
        item->indic = --item->next->indic;

    array->first = item;
    array->length++;

    if (!array->last)
        array->last = item;
    else
        item->next->previous = item;

    co_hash_put(array->dict, co_itoa(item->indic), value);
}

void *map_unshift(map_t *array) {
    void *value;
    array_item_t *item;

    if (!array || !array->first)
        return NULL;

    item = array->first;
    array->first = array->first->next;
    array->length--;

    if (array->length == 0)
        array->last = NULL;

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

    for (item = array->first; item != NULL; item = item->next) {
        if (memcmp(item->value, value, sizeof(item->value)) == 0) {
            if (item->previous)
                item->previous->next = item->next;
            else
                array->first = item->next;

            if (item->next)
                item->next->previous = item->previous;
            else
                array->last = item->previous;

            co_hash_delete(array->dict, co_itoa(item->indic));
            array->length--;

            return value;
        }
    }

    return NULL;
}

void *map_get(map_t *array, const char *key) {
    return co_hash_get(array->dict, key);
}

void map_put(map_t *array, const char *key, void *value) {
    array_item_t *item;
    void *has = co_hash_get(array->dict, key);
    if (has == NULL) {
        if (array->indices == -9999999)
            array->indices = 0;
        else
            array->indices++;

        item = (array_item_t *)CO_CALLOC(1, sizeof(array_item_t));
        item->indic = array->indices;
        item->value = co_hash_put(array->dict, key, value);
        item->previous = array->last;
        item->next = NULL;

        array->last = item;
        array->length++;

        if (!array->first)
            array->first = item;
        else
            item->previous->next = item;
    } else {
        for (item = array->first; item; item = item->next) {
            if (memcmp(item->value, has, sizeof(item->value)) == 0) {
                item->value = co_hash_put(array->dict, key, value);
                break;
            }
        }
    }
}

map_iter_t *iter_new(map_t *array, bool forward) {

    if (array && array->first) {
        map_iter_t *iterator;

        iterator = (map_iter_t *)CO_MALLOC(sizeof(map_iter_t));
        iterator->array = array;
        iterator->item = forward ? array->first : array->last;
        iterator->forward = forward;

        return iterator;
    }

    return NULL;
}

map_iter_t *iter_next(map_iter_t *iterator) {
    if (iterator) {
        array_item_t *item;

        item = iterator->forward ? iterator->item->next : iterator->item->previous;
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

void *iter_value(map_iter_t *iterator) {
    if (iterator)
        return iterator->item->value;

    return NULL;
}

map_iter_t *iter_remove(map_iter_t *iterator) {
    array_item_t *item;

    if (!iterator)
        return NULL;

    item = iterator->forward ? iterator->array->first : iterator->array->last;
    while (item) {
        if (iterator->item == item) {
            if (iterator->array->first == item)
                iterator->array->first = item->next;
            else
                item->previous->next = item->next;

            if (iterator->array->last == item)
                iterator->array->last = item->previous;
            else
                item->next->previous = item->previous;

            iterator->array->length--;

            iterator->item = iterator->forward ? item->next : item->previous;
            CO_FREE(item);
            if (iterator->item) {
                return iterator;
            } else {
                CO_FREE(iterator);
                return NULL;
            }
        }

        item = iterator->forward ? item->next : item->previous;
    }

    return iterator;
}

void iter_free(map_iter_t *iterator) {
    if (iterator)
        CO_FREE(iterator);
}
