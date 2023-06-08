#define MKCREFLECT_IMPL
#include <mkcreflect.h>

#include "mkcreflect_test.h"

#include <inttypes.h>

typedef struct
{
  int p1;
  int p2;
} BaseStruct;

MKCREFLECT_DEFINE_STRUCT(TestStruct,
    (INTEGER, unsigned int, field1),
    (INTEGER, int64_t, field2, 20),
    (STRING, char, field3, 10),
    (STRUCT, BaseStruct, field4),
    (FLOAT, float, field5),
    (DOUBLE, double, field6),
    (POINTER, void*, field7, 3))

static int assert_field(MKCREFLECT_FieldInfo* expected, MKCREFLECT_FieldInfo* actual)
{
    ASSERT_STREQ(expected->field_type, actual->field_type);
    ASSERT_STREQ(expected->field_name, actual->field_name);
    ASSERT_UEQ(expected->size, actual->size);
    ASSERT_UEQ(expected->offset, actual->offset);
    ASSERT_EQ(expected->is_signed, actual->is_signed);
    ASSERT_EQ(expected->array_size, actual->array_size);
    ASSERT_EQ(expected->data_type, actual->data_type);

  return 0;
}

static int test_get_struct_info(void)
{
    MKCREFLECT_TypeInfo* info = mkcreflect_get_TestStruct_type_info();

    ASSERT_UEQ(7lu, info->fields_count);
    ASSERT_STREQ("TestStruct", info->name);
    ASSERT_UEQ(sizeof(TestStruct), info->size);
    size_t packed_size = sizeof(unsigned int) + sizeof(int64_t) + sizeof(char) +
            sizeof(BaseStruct) + sizeof(float) + sizeof(double) + sizeof(void*);
    ASSERT_UEQ(packed_size, info->packed_size);

    MKCREFLECT_FieldInfo fields_info[] =
    {
        {"unsigned int", "field1", sizeof(unsigned int), offsetof(TestStruct, field1), 0, -1, MKCREFLECT_TYPES_INTEGER},
        {"int64_t", "field2", sizeof(int64_t[20]), offsetof(TestStruct, field2), 1, 20, MKCREFLECT_TYPES_INTEGER},
        {"char", "field3", sizeof(char[10]), offsetof(TestStruct, field3), 1, 10, MKCREFLECT_TYPES_STRING},
        {"BaseStruct", "field4", sizeof(BaseStruct), offsetof(TestStruct, field4), 0, -1, MKCREFLECT_TYPES_STRUCT},
        {"float", "field5", sizeof(float), offsetof(TestStruct, field5), 1, -1, MKCREFLECT_TYPES_FLOAT},
        {"double", "field6", sizeof(double), offsetof(TestStruct, field6), 1, -1, MKCREFLECT_TYPES_DOUBLE},
        {"void*", "field7", sizeof(void*[3]), offsetof(TestStruct, field7), 0, 3, MKCREFLECT_TYPES_POINTER}
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
