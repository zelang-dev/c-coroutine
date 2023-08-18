#include "../include/coroutine.h"

#include "test_assert.h"

static char *_value_1 = "value1";
static char *_value_2 = "value2";
static char *_value_3 = "value3";

int test_map_create() {
    map_t *list = map_new(NULL);
    ASSERT_NOTNULL(list);
    map_free(list);
    return 0;
}

int test_map_free() {
    map_t *list = map_new(NULL);
    map_free(list);
    map_free(NULL);
    return 0;
}

int test_map_push_pop() {
    map_t *list;
    char *value;

    list = map_new(NULL);
    map_push(list, _value_1);
    ASSERT_XEQ(map_count(list), 1);
    value = (char *)map_pop(list);
    ASSERT_PTR(value, _value_1);
    ASSERT_XEQ(map_count(list), 0);

    map_push(list, _value_2);
    ASSERT_XEQ(map_count(list), 1);
    map_push(list, _value_3);
    ASSERT_XEQ(map_count(list), 2);
    value = (char *)map_pop(list);
    ASSERT_PTR(value, _value_3);
    ASSERT_XEQ(map_count(list), 1);
    value = (char *)map_pop(list);
    ASSERT_PTR(value, _value_2);
    ASSERT_XEQ(map_count(list), 0);

    map_free(list);
    return 0;
}

int test_map_get() {
    map_t *list;
    int index = 0;

    list = map_new(NULL);
    ASSERT_NULL(map_get(list, co_itoa(index)));
    ASSERT_NULL(map_get(list, "0"));

    index = map_push(list, _value_1);
    ASSERT_PTR(_value_1, map_get(list, co_itoa(index)));
    ASSERT_PTR(_value_1, map_get(list, "0"));

    index = map_push(list, _value_2);
    ASSERT_PTR(_value_2, map_get(list, co_itoa(index)));
    ASSERT_PTR(_value_1, map_get(list, "0"));

    index = map_push(list, _value_3);
    ASSERT_PTR(_value_3, map_get(list, co_itoa(index)));
    ASSERT_PTR(_value_1, map_get(list, "0"));

    map_free(list);
    return 0;
}

int test_map_shift_unshift() {
    map_t *list;
    char *value;

    list = map_new(NULL);
    map_shift(list, _value_1);
    ASSERT_XEQ(map_count(list), 1);
    value = (char *)map_unshift(list);
    ASSERT_PTR(value, _value_1);
    ASSERT_XEQ(map_count(list), 0);

    map_shift(list, _value_2);
    ASSERT_XEQ(map_count(list), 1);
    map_shift(list, _value_3);
    ASSERT_XEQ(map_count(list), 2);
    value = (char *)map_unshift(list);
    ASSERT_PTR(value, _value_3);
    ASSERT_XEQ(map_count(list), 1);
    value = (char *)map_unshift(list);
    ASSERT_PTR(value, _value_2);
    ASSERT_XEQ(map_count(list), 0);

    map_free(list);
    return 0;
}


int test_map_empty() {
    map_t *list;

    list = map_new(NULL);
    ASSERT_NULL(map_pop(list));
    ASSERT_NULL(map_unshift(list));
    map_free(list);
    return 0;
}


void _free_value(void *value) {
    return;
}

int test_map_iterator() {
    map_t *list;
    map_iter_t *iterator;

    list = map(&_free_value, 3, _value_1, _value_2, _value_3);
    iterator = iter_new(list, true);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iter_value(iterator), _value_1);
    iterator = iter_next(iterator);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iter_value(iterator), _value_2);
    iterator = iter_next(iterator);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iter_value(iterator), _value_3);
    iterator = iter_next(iterator);
    ASSERT_NULL(iterator);

    iterator = iter_new(list, false);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iter_value(iterator), _value_3);
    ASSERT_STR(iter_key(iterator), "2");
    iterator = iter_next(iterator);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iter_value(iterator), _value_2);
    ASSERT_STR(iter_key(iterator), "1");
    iterator = iter_next(iterator);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iter_value(iterator), _value_1);
    ASSERT_STR(iter_key(iterator), "0");
    iterator = iter_next(iterator);
    ASSERT_NULL(iterator);

    return 0;
}


int test_map_shift_free() {
    map_t *list;

    list = map_new(&_free_value);

    map_shift(list, _value_1);
    map_shift(list, _value_2);
    map_shift(list, _value_3);

    map_free(list);
    return 0;
}


int test_map_remove_one() {
    map_t *list;
    map_iter_t *iterator;

    list = map_new(NULL);
    map_push(list, _value_1);

    iterator = iter_new(list, true);
    iterator = iter_remove(iterator);
    ASSERT_NULL(iterator);
    ASSERT_XEQ(map_count(list), 0);

    map_free(list);
    return 0;
}


static int _verify_map_values(map_t *list, int count, ...) {
    va_list values_list;
    void **values;
    int i;
    map_iter_t *iterator;

    values = (void **)malloc(count * sizeof(void *));

    ASSERT_XEQ(count, map_count(list));

    va_start(values_list, count);
    for (i = 0; i < count; i++)
        values[ i ] = va_arg(values_list, void *);
    va_end(values_list);

    iterator = iter_new(list, true);
    for (i = 0; i < count; i++) {
        ASSERT_NOTNULL(iterator);
        ASSERT_PTR(values[ i ], iter_value(iterator));
        iterator = iter_next(iterator);
    }
    ASSERT_NULL(iterator);

    iterator = iter_new(list, false);
    for (i = count - 1; i >= 0; i--) {
        ASSERT_NOTNULL(iterator);
        ASSERT_PTR(values[ i ], iter_value(iterator));
        iterator = iter_next(iterator);
    }
    ASSERT_NULL(iterator);

    free(values);
    return 0;
}

int test_map_iter_remove_first() {
    map_t *list;
    map_iter_t *iterator;

    list = map_new(&_free_value);
    map_push(list, _value_1);
    map_push(list, _value_2);
    map_push(list, _value_3);

    iterator = iter_new(list, true);
    iterator = iter_remove(iterator);
    iter_free(iterator);

    _verify_map_values(list, 2, _value_2, _value_3);

    map_free(list);
    return 0;
}


int test_map_iter_remove_middle() {
    map_t *list;
    map_iter_t *iterator;

    list = map_new(&_free_value);
    map_push(list, _value_1);
    map_push(list, _value_2);
    map_push(list, _value_3);

    iterator = iter_new(list, true);
    iterator = iter_next(iterator);
    iterator = iter_remove(iterator);
    iter_free(iterator);

    _verify_map_values(list, 2, _value_1, _value_3);

    map_free(list);
    return 0;
}


int test_map_iter_remove_last() {
    map_t *list;
    map_iter_t *iterator;

    list = map_new(&_free_value);
    map_push(list, _value_1);
    map_push(list, _value_2);
    map_push(list, _value_3);

    iterator = iter_new(list, false);
    iterator = iter_remove(iterator);
    iter_free(iterator);

    _verify_map_values(list, 2, _value_1, _value_2);

    map_free(list);
    return 0;
}


int test_map_remove_first() {
    map_t *list;

    list = map_new(&_free_value);
    map_push(list, _value_1);
    map_push(list, _value_2);
    map_push(list, _value_3);

    ASSERT_PTR(map_remove(list, _value_1), _value_1);

    _verify_map_values(list, 2, _value_2, _value_3);

    map_free(list);
    return 0;
}

int test_map_remove_middle() {
    map_t *list;

    list = map_new(&_free_value);
    map_push(list, _value_1);
    map_push(list, _value_2);
    map_push(list, _value_3);

    ASSERT_PTR(map_remove(list, _value_2), _value_2);

    _verify_map_values(list, 2, _value_1, _value_3);

    map_free(list);
    return 0;
}

int test_map_remove_last() {
    map_t *list;

    list = map_new(&_free_value);
    map_push(list, _value_1);
    map_push(list, _value_2);
    map_push(list, _value_3);

    ASSERT_PTR(map_remove(list, _value_3), _value_3);

    _verify_map_values(list, 2, _value_1, _value_2);

    map_free(list);
    return 0;
}

int test_map_iter_free() {
    map_t *list;
    map_iter_t *iterator;

    list = map_new(_free_value);
    map_push(list, _value_1);
    iterator = iter_new(list, true);

    iter_free(iterator);
    iter_free(NULL);

    map_free(list);
    return 0;
}

int test_map_slice_foreach() {
    map_t *list;
    slice_t *part;
    int i = 0;

    list = map(CO_FREE, 3, "__1", "__2", "__3");
    range(item in list) {
        printf("item key is %s, and value is %s\n", indic(item), (char *)has(item));
        ASSERT_STR(indic(item), co_itoa(i));
        i++;
    }
    map_free(list);

    list = map(CO_FREE, 4, "John", "Paul", "George", "Ringo");
    part = slice(list, 0, 2);
    ASSERT_NOTNULL(part);
    i = 0;
    foreach(item in part) {
        printf("item key is %s, and value is %s\n", indic(item), (char *)has(item));
        ASSERT_STR(indic(item), co_itoa(i));
        i++;
    }
    map_free(list);

    return 0;
}

static int test_list(void) {
    test_map_create();
    test_map_free();
    test_map_push_pop();
    test_map_get();
    test_map_shift_unshift();
    test_map_empty();
    test_map_iterator();
    test_map_shift_free();
    test_map_remove_one();
    test_map_iter_remove_first();
    test_map_iter_remove_middle();
    test_map_iter_remove_last();
    test_map_remove_first();
    test_map_remove_middle();
    test_map_remove_last();
    test_map_iter_free();
    test_map_slice_foreach();

    return 1;
}

int co_main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    ASSERT_FUNC(test_list());

    return 0;
}
