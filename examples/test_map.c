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
    const char *value;

    list = map_new(NULL);
    map_push(list, _value_1);
    ASSERT_XEQ(map_count(list), 1);
    value = map_pop(list)->value.chars;
    ASSERT_PTR(value, _value_1);
    ASSERT_XEQ(map_count(list), 0);

    map_push(list, _value_2);
    ASSERT_XEQ(map_count(list), 1);
    map_push(list, _value_3);
    ASSERT_XEQ(map_count(list), 2);
    value = map_pop(list)->value.chars;
    ASSERT_PTR(value, _value_3);
    ASSERT_XEQ(map_count(list), 1);
    value = map_pop(list)->value.chars;
    ASSERT_PTR(value, _value_2);
    ASSERT_XEQ(map_count(list), 0);

    map_free(list);
    return 0;
}

int test_map_shift_unshift() {
    map_t *list;
    const char *value;

    list = map_new(NULL);
    map_shift(list, _value_1);
    ASSERT_XEQ(map_count(list), 1);
    value = map_unshift(list)->value.chars;
    ASSERT_PTR(value, _value_1);
    ASSERT_XEQ(map_count(list), 0);

    map_shift(list, _value_2);
    ASSERT_XEQ(map_count(list), 1);
    map_shift(list, _value_3);
    ASSERT_XEQ(map_count(list), 2);
    value = map_unshift(list)->value.chars;
    ASSERT_PTR(value, _value_3);
    ASSERT_XEQ(map_count(list), 1);
    value = map_unshift(list)->value.chars;
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
        ASSERT_STR((char *)values[ i ], iter_value(iterator).chars);
        iterator = iter_next(iterator);
    }
    ASSERT_NULL(iterator);

    iterator = iter_new(list, false);
    for (i = count - 1; i >= 0; i--) {
        ASSERT_NOTNULL(iterator);
        ASSERT_STR((char *)values[ i ], iter_value(iterator).chars);
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

int test_map_get_reverse() {
    map_t *list;
    int index1 = 1;
    int index2 = 2;
    int index3 = 3;
    int index4 = 21;

    list = map(CO_FREE, 3, kv("__1_", &index1), kv("__2_", &index2), kv("__3_", &index3));
    ASSERT_NOTNULL(list);

    ASSERT_PTR(&index1, map_get(list, "__1_"));
    ASSERT_PTR(&index2, map_get(list, "__2_"));
    ASSERT_PTR(&index3, map_get(list, "__3_"));

    map_put(list, "__2_", &index4);
    index4 = 99;
    reverse(item in list) {
        printf("item key is %s, and value is %d\n", indic(item), has(item).integer);
    }

    map_free(list);
    return 0;
}

int test_array_iterator() {
    array_t *list;
    map_iter_t *iterator;

    list = array(&_free_value, 3, _value_1, _value_2, _value_3);
    iterator = iter_new(list, true);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iter_value(iterator).chars, _value_1);
    iterator = iter_next(iterator);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iter_value(iterator).chars, _value_2);
    iterator = iter_next(iterator);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iter_value(iterator).chars, _value_3);
    iterator = iter_next(iterator);
    ASSERT_NULL(iterator);

    iterator = iter_new(list, false);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iter_value(iterator).chars, _value_3);
    ASSERT_STR(iter_key(iterator), "2");
    iterator = iter_next(iterator);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iter_value(iterator).chars, _value_2);
    ASSERT_STR(iter_key(iterator), "1");
    iterator = iter_next(iterator);
    ASSERT_NOTNULL(iterator);
    ASSERT_PTR(iter_value(iterator).chars, _value_1);
    ASSERT_STR(iter_key(iterator), "0");
    iterator = iter_next(iterator);
    ASSERT_NULL(iterator);

    map_free(list);

    list = map_by(3, kv("__1_", 1), kv("__2_", 2), kv("__3_", 3));
    foreach(item in list) {
        printf("key is %s, value is %d\n", indic(item), has(item).integer);
    }

    map_free(list);
    return 0;
}

int test_array_slice_foreach() {
    array_t *list;
    slice_t *part;
    int i = 5;

    list = array_by(6, 18, 6, 10, 1, 99, 8888888);
    reverse(item in list) {
        printf("key: %s, value: %d\n", indic(item), has(item).integer);
        ASSERT_STR(indic(item), co_itoa(i));
        i--;
    }
    map_free(list);

    list = array(CO_FREE, 4, "John", "Paul", "George", "Ringo");
    part = slice(list, 0, 2);
    ASSERT_NOTNULL(part);
    i = 0;
    foreach(item in part) {
        printf("item key is %s, and value is %s\n", indic(item), iter_value(item).chars);
        ASSERT_STR(indic(item), co_itoa(i));
        i++;
    }
    map_free(list);

    return 0;
}

int test_array_range() {
    array_t *list;
    int i = 9;
    reverse(item in list = range(0, 10)) {
        printf("range key: %s, range value: %d\n", indic(item), has(item).integer);
        ASSERT_STR(indic(item), co_itoa(i));
        i--;
    }
    map_free(list);

    return 0;
}

static int test_list(void) {
    test_map_create();
    test_map_free();
    test_map_push_pop();
    test_map_shift_unshift();
    test_map_empty();
    test_map_shift_free();
    test_map_remove_one();
    test_map_iter_remove_first();
    test_map_iter_remove_middle();
    test_map_iter_remove_last();
    test_map_remove_first();
    test_map_remove_middle();
    test_map_remove_last();
    test_map_iter_free();
    test_map_get_reverse();
    test_array_iterator();
    test_array_slice_foreach();
    test_array_range();

    return 1;
}

int co_main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    ASSERT_FUNC(test_list());

    return 0;
}
