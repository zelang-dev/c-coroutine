#include "../include/reflect.h"

#include "test_assert.h"

#include <inttypes.h>

typedef struct
{
  int p1;
  int p2;
} base_struct;

REFLECT_DEFINE_STRUCT(test_struct,
    (INTEGER, unsigned int, field1),
    (INTEGER, int64_t, field2, 20),
    (STRING, char, field3, 10),
    (STRUCT, base_struct, field4),
    (FLOAT, float, field5),
    (DOUBLE, double, field6),
    (POINTER, void*, field7, 3))

static int assert_field(reflect_info_t* expected, reflect_info_t* actual)
{
    ASSERT_STR(expected->field_type, actual->field_type);
    ASSERT_STR(expected->field_name, actual->field_name);
    ASSERT_UEQ(expected->size, actual->size);
    ASSERT_UEQ(expected->offset, actual->offset);
    ASSERT_EQ(expected->is_signed, actual->is_signed);
    ASSERT_EQ(expected->array_size, actual->array_size);
    ASSERT_EQ(expected->data_type, actual->data_type);

  return 0;
}

static int test_get_struct_info(void)
{
    reflect_type_t* info = reflect_get_test_struct_type_info();

    ASSERT_UEQ(7lu, info->fields_count);
    ASSERT_STR("test_struct", info->name);
    ASSERT_UEQ(sizeof(test_struct), info->size);
    size_t packed_size = sizeof(unsigned int) + sizeof(int64_t) + sizeof(char) +
            sizeof(base_struct) + sizeof(float) + sizeof(double) + sizeof(void*);
    ASSERT_UEQ(packed_size, info->packed_size);

    reflect_info_t fields_info[] =
    {
        {"unsigned int", "field1", sizeof(unsigned int), offsetof(test_struct, field1), 0, -1, REFLECT_INTEGER},
        {"int64_t", "field2", sizeof(int64_t[20]), offsetof(test_struct, field2), 1, 20, REFLECT_INTEGER},
        {"char", "field3", sizeof(char[10]), offsetof(test_struct, field3), 1, 10, REFLECT_STRING},
        {"base_struct", "field4", sizeof(base_struct), offsetof(test_struct, field4), 0, -1, REFLECT_STRUCT},
        {"float", "field5", sizeof(float), offsetof(test_struct, field5), 1, -1, REFLECT_FLOAT},
        {"double", "field6", sizeof(double), offsetof(test_struct, field6), 1, -1, REFLECT_DOUBLE},
        {"void*", "field7", sizeof(void*[3]), offsetof(test_struct, field7), 0, 3, REFLECT_POINTER}
    };

    for (size_t i = 0; i < info->fields_count; i++)
    {
        ASSERT_FUNC(assert_field(&fields_info[i], &info->fields[i]));
    }

    return 0;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    ASSERT_FUNC(test_get_struct_info());

    return 0;
}
