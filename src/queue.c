#include "../include/coroutine.h"

co_queue_t *queue_new(void) {
    return (co_queue_t *)CO_CALLOC(1, sizeof(co_queue_t));
}

void queue_free(co_queue_t *queue, void (*free_value)(void *value)) {
    co_item_t *next_item;

    if (!queue)
        return;

    while (queue->first_item) {
        if (free_value)
            free_value(queue->first_item->value);

        next_item = queue->first_item->next;
        CO_FREE(queue->first_item);
        queue->first_item = next_item;
    }

    CO_FREE(queue);
}

void queue_push(co_queue_t *queue, void *value) {
    co_item_t *item;

    item = (co_item_t *)CO_CALLOC(1, sizeof(co_item_t));
    item->value = value;
    item->previous = queue->last_item;
    item->next = NULL;

    queue->last_item = item;
    queue->length++;

    if (!queue->first_item)
        queue->first_item = item;
    else
        item->previous->next = item;
}

void *queue_pop(co_queue_t *queue) {
    void *value;
    co_item_t *item;

    if (!queue || !queue->last_item)
        return NULL;

    item = queue->last_item;
    queue->last_item = queue->last_item->previous;
    queue->length--;

    if (queue->length == 0)
        queue->first_item = NULL;

    value = item->value;
    CO_FREE(item);

    return value;
}

void *queue_peek(co_queue_t *queue) {
    if (!queue || !queue->last_item)
        return NULL;

    return queue->last_item->value;
}

void *queue_peek_first(co_queue_t *queue) {
    if (!queue || !queue->first_item)
        return NULL;

    return queue->first_item->value;
}

void queue_shift(co_queue_t *queue, void *value) {
    co_item_t *item;

    if (!queue)
        return;

    item = (co_item_t *)CO_CALLOC(1, sizeof(co_item_t));
    item->value = value;
    item->previous = NULL;
    item->next = queue->first_item;

    queue->first_item = item;
    queue->length++;

    if (!queue->last_item)
        queue->last_item = item;
    else
        item->next->previous = item;
}

void *queue_unshift(co_queue_t *queue) {
    void *value;
    co_item_t *item;

    if (!queue || !queue->first_item)
        return NULL;

    item = queue->first_item;
    queue->first_item = queue->first_item->next;
    queue->length--;

    if (queue->length == 0)
        queue->last_item = NULL;

    value = item->value;
    CO_FREE(item);

    return value;
}

size_t queue_length(co_queue_t *queue) {
    if (queue)
        return queue->length;

    return 0;
}

void *queue_remove(co_queue_t *queue, void *value) {
    co_item_t *item;

    if (!queue)
        return NULL;

    for (item = queue->first_item; item; item = item->next) {
        if (item->value == value) {
            if (item->previous)
                item->previous->next = item->next;
            else
                queue->first_item = item->next;

            if (item->next)
                item->next->previous = item->previous;
            else
                queue->last_item = item->previous;

            CO_FREE(item);
            queue->length--;

            return value;
        }
    }

    return NULL;
}

co_iterator_t *co_iterator(co_queue_t *queue, bool forward) {
    if (queue && queue->first_item) {
        co_iterator_t *iterator;

        iterator = (co_iterator_t *)CO_MALLOC(sizeof(co_iterator_t));
        iterator->queue = queue;
        iterator->item = forward ? queue->first_item : queue->last_item;
        iterator->forward = forward;

        return iterator;
    }

    return NULL;
}

co_iterator_t *co_iterator_next(co_iterator_t *iterator) {
    if (iterator) {
        co_item_t *item;

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

void *co_iterator_value(co_iterator_t *iterator) {
    if (iterator)
        return iterator->item->value;

    return NULL;
}

co_iterator_t *co_iterator_remove(co_iterator_t *iterator) {
    co_item_t *item;

    if (!iterator)
        return NULL;

    item = iterator->forward ? iterator->queue->first_item : iterator->queue->last_item;
    while (item) {
        if (iterator->item == item) {
            if (iterator->queue->first_item == item)
                iterator->queue->first_item = item->next;
            else
                item->previous->next = item->next;

            if (iterator->queue->last_item == item)
                iterator->queue->last_item = item->previous;
            else
                item->next->previous = item->previous;

            iterator->queue->length--;

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

void co_iterator_free(co_iterator_t *iterator) {
    if (iterator)
        CO_FREE(iterator);
}
