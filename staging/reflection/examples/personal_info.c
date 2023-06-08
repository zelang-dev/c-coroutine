/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <marcin.kolny@gmail.com> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return. Marcin Kolny
 * ----------------------------------------------------------------------------
 * http://github.com/loganek/mkcreflect
 */

#define MKCREFLECT_IMPL
#include <mkcreflect.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    MKCREFLECT_TypeInfo* info;
} Base;

#define TO_BASE(var) ((Base*)(var))

#define GENERATE_INIT_FOR_STRING(...)
#define GENERATE_INIT_FOR_INTEGER(...)
#define GENERATE_INIT_FOR_FLOAT(...)
#define GENERATE_INIT_FOR_DOUBLE(...)
#define GENERATE_INIT_FOR_POINTER(...)
#define GENERATE_INIT_FOR_STRUCT(TYPE_NAME, FIELD_NAME) initialize_##TYPE_NAME##_type(&obj->FIELD_NAME);

#define GENERATE_INIT_BASE_ARRAY(IGNORE, DATA_TYPE, C_TYPE, FIELD_NAME, ...) GENERATE_INIT_FOR_##DATA_TYPE(C_TYPE, FIELD_NAME)
#define GENERATE_INIT_BASE_TYPE(IGNORE, DATA_TYPE, C_TYPE, FIELD_NAME) GENERATE_INIT_FOR_##DATA_TYPE(C_TYPE, FIELD_NAME)

#define GENERATE_INIT_(...) \
    MKCREFLECT_EXPAND_(MKCREFLECT_OVERRIDE_5( \
        __VA_ARGS__, \
        GENERATE_INIT_BASE_ARRAY, \
        GENERATE_INIT_BASE_TYPE, \
        MKCREFLECT_OVERRIDE_5_PLACEHOLDER)(__VA_ARGS__)) \

#define GENERATE_INIT(X, USER_DATA) GENERATE_INIT_(USER_DATA, MKCREFLECT_EXPAND_VA_ X)

#define GENERATE_INIT_METHOD(TypeName, ...) \
    static void initialize_##TypeName##_type(TypeName* obj) \
    { \
        obj->base.info = mkcreflect_get_##TypeName##_type_info(); \
        MKCREFLECT_FOREACH(GENERATE_INIT, TypeName, __VA_ARGS__) \
    }

#define DEFINE_STRUCT(TypeName, ...) \
    MKCREFLECT_DEFINE_STRUCT(TypeName, (STRUCT, Base, base), __VA_ARGS__) \
    GENERATE_INIT_METHOD(TypeName, __VA_ARGS__)

static void set_value(Base* variable, MKCREFLECT_FieldInfo* field_info, void* value)
{
    memcpy((char*)variable + field_info->offset, value, field_info->size);
}

static void get_value(Base* variable, MKCREFLECT_FieldInfo* field_info, void* out)
{
    memcpy((char*)out, (char*)variable + field_info->offset, field_info->size);
}

#define SET_INTEGER_VALUE_(PREFIX, SIZE) \
    case SIZE/8: { \
        PREFIX##int##SIZE##_t c_data = (PREFIX##int##SIZE##_t)data; \
        set_value(entity, field_info, &c_data); \
    } break

#define SET_INTEGER_VALUE(PREFIX) \
    switch (field_info->size) \
    { \
    SET_INTEGER_VALUE_(PREFIX,8); \
    SET_INTEGER_VALUE_(PREFIX,16); \
    SET_INTEGER_VALUE_(PREFIX,32); \
    SET_INTEGER_VALUE_(PREFIX,64); \
    default: \
        printf("Unsupported size\n"); \
    }

#define GET_INTEGER_VALUE_(PREFIX, SIZE) \
    case SIZE/8: { \
        PREFIX##int##SIZE##_t c_data = 0; \
        get_value(entity, field_info, &c_data); \
        data = c_data; \
    } break

#define GET_INTEGER_VALUE(PREFIX) \
    switch (field_info->size) \
    { \
    GET_INTEGER_VALUE_(PREFIX,8); \
    GET_INTEGER_VALUE_(PREFIX,16); \
    GET_INTEGER_VALUE_(PREFIX,32); \
    SET_INTEGER_VALUE_(PREFIX,64); \
    default: \
        printf("Unsupported size\n"); \
    }

static void unknown_type_error(Base* base)
{
    printf("I don't know how to handle the %s type", base->info->name);
}

static void load_user_data(void* value, int indent)
{
    Base* entity = TO_BASE(value);
    for (size_t i = 0; i < entity->info->fields_count; i++)
    {
        MKCREFLECT_FieldInfo* field_info = &entity->info->fields[i];
        if (strcmp(field_info->field_name, "base") == 0)
        {
            continue;
        }

        printf("%*sEnter %s: ", indent, "", field_info->field_name);

        switch (field_info->data_type)
        {
        case MKCREFLECT_TYPES_INTEGER:
            if (field_info->is_signed)
            {
                long long int data;
                scanf("%lli", &data);
                SET_INTEGER_VALUE()
            }
            else
            {
                unsigned long long int data;
                scanf("%llu", &data);
                SET_INTEGER_VALUE(u)
            }
            break;
        case MKCREFLECT_TYPES_STRING:
        {
            char* data = (char*)malloc(field_info->size);
            scanf("%s", data);
            set_value(entity, field_info, data);
            free(data);
            break;
        }
        case MKCREFLECT_TYPES_STRUCT:
            printf("\n");
            load_user_data(TO_BASE((char*)entity + field_info->offset), indent + 2);
            break;
        default:
            unknown_type_error(TO_BASE((char*)entity + field_info->offset));
            break;
        }
    }
}

static void print_user_data(void* value, int indent)
{
    Base* entity = TO_BASE(value);
    for (size_t i = 0; i < entity->info->fields_count; i++)
    {
        MKCREFLECT_FieldInfo* field_info = &entity->info->fields[i];
        if (strcmp(field_info->field_name, "base") == 0)
        {
            continue;
        }

        printf("%*s%s: ", indent, "", field_info->field_name);

        switch (field_info->data_type)
        {
        case MKCREFLECT_TYPES_INTEGER:
            if (field_info->is_signed)
            {
                long long int data;
                GET_INTEGER_VALUE()
                printf("%lli", data);
            }
            else
            {
                unsigned long long int data;
                GET_INTEGER_VALUE(u)
                printf("%llu", data);
            }
            break;
        case MKCREFLECT_TYPES_STRING:
        {
            char* data = (char*)malloc(field_info->size);
            get_value(entity, field_info, data);
            printf("%s", data);
            free(data);
            break;
        }
        case MKCREFLECT_TYPES_STRUCT:
            printf("\n");
            print_user_data(TO_BASE((char*)entity + field_info->offset), indent + 2);
            break;
        default:
            unknown_type_error(TO_BASE((char*)entity + field_info->offset));
        }
        printf("\n");
    }
}

DEFINE_STRUCT(Address,
              (STRING, char, street, 20),
              (INTEGER, unsigned int, house_number))

DEFINE_STRUCT(PersonalInfo,
              (STRING, char, name, 20),
              (INTEGER, unsigned char, age),
              (STRUCT, Address, address))

int main(void)
{
    PersonalInfo my_info;
    initialize_PersonalInfo_type(&my_info);

    load_user_data(&my_info, 0);

    printf("Thanks for your data!\n");

    print_user_data(&my_info, 0);

    return 0;
}
