#include "../include/coroutine.h"

#include "test_assert.h"

static char *_value_1 = "value1";
static char *_value_2 = "value2";
static char *_value_3 = "value3";

int test_queue_create() {
    co_queue_t *queue = queue_new();
    ASSERT_NOTNULL(queue);
    queue_free(queue, NULL);
    return 0;
}


int test_queue_free() {
    co_queue_t *queue = queue_new();
    queue_free(queue, NULL);
    queue_free(NULL, NULL);
    return 0;
}


int test_queue_push_pop() {
    co_queue_t *queue;
    char *value;

    queue = queue_new();
    queue_push(queue, _value_1);
    ASSERT_XEQ(queue_length(queue), 1);
    value = (char *)queue_pop(queue);
    ASSERT_PTR(value, _value_1);
    ASSERT_XEQ(queue_length(queue), 0);

    queue_push(queue, _value_2);
    ASSERT_XEQ(queue_length(queue), 1);
    queue_push(queue, _value_3);
    ASSERT_XEQ(queue_length(queue), 2);
    value = (char *)queue_pop(queue);
    ASSERT_PTR(value, _value_3);
    ASSERT_XEQ(queue_length(queue), 1);
    value = (char *)queue_pop(queue);
    ASSERT_PTR(value, _value_2);
    ASSERT_XEQ(queue_length(queue), 0);

    queue_free(queue, NULL);
    return 0;
}


int test_queue_peek() {
    co_queue_t *queue;

    queue = queue_new();
    ASSERT_NULL(queue_peek(queue));
    ASSERT_NULL(queue_peek_first(queue));

    queue_push(queue, _value_1);
    ASSERT_PTR(_value_1, queue_peek(queue));
    ASSERT_PTR(_value_1, queue_peek_first(queue));

    queue_push(queue, _value_2);
    ASSERT_PTR(_value_2, queue_peek(queue));
    ASSERT_PTR(_value_1, queue_peek_first(queue));

    queue_push(queue, _value_3);
    ASSERT_PTR(_value_3, queue_peek(queue));
    ASSERT_PTR(_value_1, queue_peek_first(queue));

    queue_free(queue, NULL);
    return 0;
}


int test_queue_shift_unshift() {
    co_queue_t *queue;
    char *value;

    queue = queue_new();
    queue_shift(queue, _value_1);
    ASSERT_XEQ(queue_length(queue), 1);
    value = (char *)queue_unshift(queue);
    ASSERT_PTR(value, _value_1);
    ASSERT_XEQ(queue_length(queue), 0);

    queue_shift(queue, _value_2);
    ASSERT_XEQ(queue_length(queue), 1);
    queue_shift(queue, _value_3);
    ASSERT_XEQ(queue_length(queue), 2);
    value = (char *)queue_unshift(queue);
    ASSERT_PTR(value, _value_3);
    ASSERT_XEQ(queue_length(queue), 1);
    value = (char *)queue_unshift(queue);
    ASSERT_PTR(value, _value_2);
    ASSERT_XEQ(queue_length(queue), 0);

    queue_free(queue, NULL);
    return 0;
}


int test_queue_empty() {
    co_queue_t *queue;

    queue = queue_new();
    ASSERT_NULL(queue_pop(queue));
    ASSERT_NULL(queue_unshift(queue));
    queue_free(queue, NULL);
    return 0;
}


void _free_value(void *value) {
    return;
}

int test_queue_iterator() {
    co_queue_t *queue;
    co_iterator_t *iterator;

    queue = queue_new();
    queue_push(queue, _value_1);
    queue_push(queue, _value_2);
    queue_push(queue, _value_3);

    iterator = iterator_new(queue, true);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iterator_value(iterator), _value_1);
    iterator = iterator_next(iterator);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iterator_value(iterator), _value_2);
    iterator = iterator_next(iterator);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iterator_value(iterator), _value_3);
    iterator = iterator_next(iterator);
    ASSERT_NULL(iterator);

    iterator = iterator_new(queue, false);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iterator_value(iterator), _value_3);
    iterator = iterator_next(iterator);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iterator_value(iterator), _value_2);
    iterator = iterator_next(iterator);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iterator_value(iterator), _value_1);
    iterator = iterator_next(iterator);
    ASSERT_NULL(iterator);

    foreach(item in queue) {
        printf("item value is %s\n", (char *)has(item));
    }

    queue_free(queue, &_free_value);
    return 0;
}


int test_queue_shift_free() {
    co_queue_t *queue;

    queue = queue_new();

    queue_shift(queue, _value_1);
    queue_shift(queue, _value_2);
    queue_shift(queue, _value_3);

    queue_free(queue, &_free_value);
    return 0;
}


int test_queue_remove_one() {
    co_queue_t *queue;
    co_iterator_t *iterator;

    queue = queue_new();
    queue_push(queue, _value_1);

    iterator = iterator_new(queue, true);
    iterator = iterator_remove(iterator);
    ASSERT_NULL(iterator);
    ASSERT_XEQ(queue_length(queue), 0);

    queue_free(queue, NULL);
    return 0;
}


static int _verify_queue_values(co_queue_t *queue, int count, ...) {
    va_list values_list;
    void **values;
    int i;
    co_iterator_t *iterator;

    values = (void **)malloc(count * sizeof(void *));

    ASSERT_XEQ(count, queue_length(queue));

    va_start(values_list, count);
    for (i = 0; i < count; i++)
        values[ i ] = va_arg(values_list, void *);
    va_end(values_list);

    iterator = iterator_new(queue, true);
    for (i = 0; i < count; i++) {
        ASSERT_NOTNULL(iterator);
        ASSERT_PTR(values[ i ], iterator_value(iterator));
        iterator = iterator_next(iterator);
    }
    ASSERT_NULL(iterator);

    iterator = iterator_new(queue, false);
    for (i = count - 1; i >= 0; i--) {
        ASSERT_NOTNULL(iterator);
        ASSERT_PTR(values[ i ], iterator_value(iterator));
        iterator = iterator_next(iterator);
    }
    ASSERT_NULL(iterator);

    free(values);
    return 0;
}

int test_queue_iterator_remove_first() {
    co_queue_t *queue;
    co_iterator_t *iterator;

    queue = queue_new();
    queue_push(queue, _value_1);
    queue_push(queue, _value_2);
    queue_push(queue, _value_3);

    iterator = iterator_new(queue, true);
    iterator = iterator_remove(iterator);
    iterator_free(iterator);

    _verify_queue_values(queue, 2, _value_2, _value_3);

    queue_free(queue, &_free_value);
    return 0;
}


int test_queue_iterator_remove_middle() {
    co_queue_t *queue;
    co_iterator_t *iterator;

    queue = queue_new();
    queue_push(queue, _value_1);
    queue_push(queue, _value_2);
    queue_push(queue, _value_3);

    iterator = iterator_new(queue, true);
    iterator = iterator_next(iterator);
    iterator = iterator_remove(iterator);
    iterator_free(iterator);

    _verify_queue_values(queue, 2, _value_1, _value_3);

    queue_free(queue, &_free_value);
    return 0;
}


int test_queue_iterator_remove_last() {
    co_queue_t *queue;
    co_iterator_t *iterator;

    queue = queue_new();
    queue_push(queue, _value_1);
    queue_push(queue, _value_2);
    queue_push(queue, _value_3);

    iterator = iterator_new(queue, false);
    iterator = iterator_remove(iterator);
    iterator_free(iterator);

    _verify_queue_values(queue, 2, _value_1, _value_2);

    queue_free(queue, &_free_value);
    return 0;
}


int test_queue_remove_first() {
    co_queue_t *queue;

    queue = queue_new();
    queue_push(queue, _value_1);
    queue_push(queue, _value_2);
    queue_push(queue, _value_3);

    ASSERT_PTR(queue_remove(queue, _value_1), _value_1);

    _verify_queue_values(queue, 2, _value_2, _value_3);

    queue_free(queue, &_free_value);
    return 0;
}

int test_queue_remove_middle() {
    co_queue_t *queue;

    queue = queue_new();
    queue_push(queue, _value_1);
    queue_push(queue, _value_2);
    queue_push(queue, _value_3);

    ASSERT_PTR(queue_remove(queue, _value_2), _value_2);

    _verify_queue_values(queue, 2, _value_1, _value_3);

    queue_free(queue, &_free_value);
    return 0;
}

int test_queue_remove_last() {
    co_queue_t *queue;

    queue = queue_new();
    queue_push(queue, _value_1);
    queue_push(queue, _value_2);
    queue_push(queue, _value_3);

    ASSERT_PTR(queue_remove(queue, _value_3), _value_3);

    _verify_queue_values(queue, 2, _value_1, _value_2);

    queue_free(queue, &_free_value);
    return 0;
}

int test_queue_iterator_free() {
    co_queue_t *queue;
    co_iterator_t *iterator;

    queue = queue_new();
    queue_push(queue, _value_1);
    iterator = iterator_new(queue, true);

    iterator_free(iterator);
    iterator_free(NULL);

    queue_free(queue, _free_value);
    return 0;
}

static int test_queue(void) {
    test_queue_create();
    test_queue_free();
    test_queue_push_pop();
    test_queue_peek();
    test_queue_shift_unshift();
    test_queue_empty();
    test_queue_iterator();
    test_queue_shift_free();
    test_queue_remove_one();
    test_queue_iterator_remove_first();
    test_queue_iterator_remove_middle();
    test_queue_iterator_remove_last();
    test_queue_remove_first();
    test_queue_remove_middle();
    test_queue_remove_last();
    test_queue_iterator_free();

    return 1;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    map_queue(items, _value_1, _value_2, _value_3);

    foreach(item in items) {
        printf("item value is %s\n", (char *)has(item));
    }
    queue_free(items, NULL);

    ASSERT_FUNC(test_queue());

    return 0;
}