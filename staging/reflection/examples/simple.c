/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <marcin.kolny@gmail.com> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return. Marcin Kolny
 * ----------------------------------------------------------------------------
 * http://github.com/loganek/mkcreflect
 */
#include "test_struct.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    MKCREFLECT_TypeInfo* info = mkcreflect_get_TestStruct_type_info();

    for (size_t i = 0; i < info->fields_count; i++)
    {
        MKCREFLECT_FieldInfo* field = &info->fields[i];
        printf(" * %s\n", field->field_name);
        printf("    Type: %s\n", field->field_type);
        printf("    Total size: %lu\n", field->size);
        printf("    Offset: %lu\n", field->offset);
        if (field->array_size != -1)
        {
            printf("    It is an array! Number of elements: %d, size of single element: %lu\n",
                    field->array_size, field->size / field->array_size);
        }
    }

    return 0;
}
