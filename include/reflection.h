#ifndef REFLECT_H_
#define REFLECT_H_

/*
 * Modified from:
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <marcin.kolny@gmail.com> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return. Marcin Kolny
 * http://github.com/loganek/mkcreflect
 * ----------------------------------------------------------------------------
 */

#include <stddef.h>
#include <stdbool.h>

#ifndef C_API
    #define C_API extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RE_NULL = -1,
    RE_INT,
    RE_ENUM,
    RE_INTEGER,
    RE_UINT,
    RE_SLONG,
    RE_ULONG,
    RE_LLONG,
    RE_MAXSIZE,
    RE_FLOAT,
    RE_DOUBLE,
    RE_BOOL,
    RE_SHORT,
    RE_USHORT,
    RE_CHAR,
    RE_UCHAR,
    RE_UCHAR_P,
    RE_CHAR_P,
    RE_CONST_CHAR,
    RE_STRING,
    RE_ARRAY,
    RE_HASH,
    RE_OBJ,
    RE_PTR,
    RE_FUNC,
    RE_NONE,
    RE_DEF_ARR,
    RE_DEF_FUNC,
    RE_REFLECT_TYPE,
    RE_REFLECT_INFO,
    RE_REFLECT_VALUE,
    RE_MAP_VALUE,
    RE_MAP_STRUCT,
    RE_MAP_ITER,
    RE_MAP_ARR,
    RE_ERR_PTR,
    RE_ERR_CONTEXT,
    RE_PROMISE,
    RE_FUTURE,
    RE_FUTURE_ARG,
    RE_EVENT_ARG,
    RE_SCHED,
    RE_CHANNEL,
    RE_STRUCT,
    RE_UNION,
    RE_VALUE,
    RE_NO_INSTANCE
} reflect_types;

typedef struct reflect_field_s {
    reflect_types data_type;
    const char *field_type;
    const char *field_name;
    size_t size;
    size_t offset;
    bool is_signed;
    int array_size;
} reflect_field_t;

typedef struct reflect_type_s {
    reflect_types data_type;
    void *instance;
    const char *name;
    size_t fields_count;
    size_t size;
    size_t packed_size;
    reflect_field_t *fields;
} reflect_type_t;

#define RE_EXPAND_(X) X
#define RE_EXPAND_VA_(...) __VA_ARGS__
#define RE_FOREACH_1_(FNC, USER_DATA, ARG) FNC(ARG, USER_DATA)
#define RE_FOREACH_2_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_1_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_3_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_2_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_4_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_3_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_5_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_4_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_6_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_5_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_7_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_6_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_8_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_7_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_9_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_8_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_10_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_9_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_11_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_10_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_12_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_11_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_13_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_12_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_14_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_13_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_15_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_14_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_16_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_15_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_17_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_16_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_18_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_17_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_19_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_18_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_20_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_19_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_21_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_20_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_22_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_21_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_23_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_22_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_24_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_23_(FNC, USER_DATA, __VA_ARGS__))
#define RE_FOREACH_25_(FNC, USER_DATA, ARG, ...) \
    FNC(ARG, USER_DATA) RE_EXPAND_(RE_FOREACH_24_(FNC, USER_DATA, __VA_ARGS__))

#define RE_OVERRIDE_0(FNC, ...) FNC
#define RE_OVERRIDE_0_PLACEHOLDER
#define RE_OVERRIDE_1(_1, FNC, ...) FNC
#define RE_OVERRIDE_1_PLACEHOLDER RE_OVERRIDE_0_PLACEHOLDER, 1
#define RE_OVERRIDE_2(_1, _2, FNC, ...) FNC
#define RE_OVERRIDE_2_PLACEHOLDER RE_OVERRIDE_1_PLACEHOLDER, 2
#define RE_OVERRIDE_3(_1, _2, _3, FNC, ...) FNC
#define RE_OVERRIDE_3_PLACEHOLDER RE_OVERRIDE_2_PLACEHOLDER, 3
#define RE_OVERRIDE_4(_1, _2, _3, _4, FNC, ...) FNC
#define RE_OVERRIDE_4_PLACEHOLDER RE_OVERRIDE_3_PLACEHOLDER, 4
#define RE_OVERRIDE_5(_1, _2, _3, _4, _5, FNC, ...) FNC
#define RE_OVERRIDE_5_PLACEHOLDER RE_OVERRIDE_4_PLACEHOLDER, 5
#define RE_OVERRIDE_14(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, FNC, ...) FNC
#define RE_OVERRIDE_14_PLACEHOLDER RE_OVERRIDE_5_PLACEHOLDER, 6, 7, 8, 9, 10, 11, 12, 13, 14
#define RE_OVERRIDE_20(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, FNC, ...) FNC
#define RE_OVERRIDE_20_PLACEHOLDER RE_OVERRIDE_14_PLACEHOLDER, 15, 16, 17, 18, 19, 20
#define RE_OVERRIDE_25(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, FNC, ...) FNC
#define RE_OVERRIDE_25_PLACEHOLDER RE_OVERRIDE_20_PLACEHOLDER, 21, 22, 23, 24, 25

#define RE_FOREACH(FNC, USER_DATA, ...) \
    RE_EXPAND_(RE_OVERRIDE_25( __VA_ARGS__,	\
    RE_FOREACH_25_, \
    RE_FOREACH_24_, \
    RE_FOREACH_23_, \
    RE_FOREACH_22_, \
    RE_FOREACH_21_, \
    RE_FOREACH_20_, \
    RE_FOREACH_19_, \
    RE_FOREACH_18_, \
    RE_FOREACH_17_, \
    RE_FOREACH_16_, \
    RE_FOREACH_15_, \
    RE_FOREACH_14_, \
    RE_FOREACH_13_, \
    RE_FOREACH_12_, \
    RE_FOREACH_11_, \
    RE_FOREACH_10_, \
    RE_FOREACH_9_, \
    RE_FOREACH_8_, \
    RE_FOREACH_7_, \
    RE_FOREACH_6_, \
    RE_FOREACH_5_, \
    RE_FOREACH_4_, \
    RE_FOREACH_3_, \
    RE_FOREACH_2_, \
    RE_FOREACH_1_)(FNC, USER_DATA, __VA_ARGS__))

#define RE_DECLARE_SIMPLE_FIELD_(IGNORE, TYPE, FIELD_NAME) \
    TYPE FIELD_NAME;

#define RE_DECLARE_ARRAY_FIELD_(IGNORE, TYPE, FIELD_NAME, ARRAY_SIZE) \
    TYPE FIELD_NAME[ARRAY_SIZE];

#define RE_DECLARE_FIELD_(...) RE_EXPAND_(RE_OVERRIDE_4( \
    __VA_ARGS__, \
    RE_DECLARE_ARRAY_FIELD_, \
    RE_DECLARE_SIMPLE_FIELD_, \
    RE_OVERRIDE_4_PLACEHOLDER)(__VA_ARGS__))

#define RE_DECLARE_FIELD(X, USER_DATA) RE_DECLARE_FIELD_ X

#define RE_SIZEOF_(IGNORE, C_TYPE, ...) +sizeof(C_TYPE)
#define RE_SIZEOF(X, USER_DATA) RE_SIZEOF_ X

#define RE_SUM(...) +1

#define RE_IS_TYPE_SIGNED_(C_TYPE) (C_TYPE)-1 < (C_TYPE)1
#define RE_IS_SIGNED_FUNC(C_TYPE) 0
#define RE_IS_SIGNED_PTR(C_TYPE) 0
#define RE_IS_SIGNED_OBJ(C_TYPE) 0
#define RE_IS_SIGNED_STRUCT(C_TYPE) 0
#define RE_IS_SIGNED_UNION(C_TYPE)0
#define RE_IS_SIGNED_CHAR_P(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_UCHAR_P(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_BOOL(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_MAXSIZE(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_CONST_CHAR(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_STRING(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_INTEGER(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_UINT(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_LLONG(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_ENUM(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_FLOAT(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)
#define RE_IS_SIGNED_DOUBLE(C_TYPE) RE_IS_TYPE_SIGNED_(C_TYPE)

#define RE_IS_SIGNED_(DATA_TYPE, CTYPE) RE_IS_SIGNED_##DATA_TYPE(CTYPE)

#define RE_ARRAY_FIELD_INFO_(TYPE_NAME, DATA_TYPE, C_TYPE, FIELD_NAME, ARRAY_SIZE) \
    RE_##DATA_TYPE, #C_TYPE, #FIELD_NAME, sizeof(C_TYPE) * ARRAY_SIZE, offsetof(TYPE_NAME, FIELD_NAME), \
    RE_IS_SIGNED_(DATA_TYPE, C_TYPE), ARRAY_SIZE

#define RE_SIMPLE_FIELD_INFO_(TYPE_NAME, DATA_TYPE, C_TYPE, FIELD_NAME) \
    RE_##DATA_TYPE, #C_TYPE, #FIELD_NAME, sizeof(C_TYPE), offsetof(TYPE_NAME, FIELD_NAME), \
    RE_IS_SIGNED_(DATA_TYPE, C_TYPE), -1

#define RE_FIELD_INFO_(...) \
{ \
    RE_EXPAND_(RE_OVERRIDE_5( \
    __VA_ARGS__, \
    RE_ARRAY_FIELD_INFO_, \
    RE_SIMPLE_FIELD_INFO_, \
    RE_OVERRIDE_5_PLACEHOLDER)(__VA_ARGS__)) \
},

#define RE_FIELD_INFO(X, USER_DATA) \
    RE_FIELD_INFO_(USER_DATA, RE_EXPAND_VA_ X)

#define RE_DEFINE_METHOD(TYPE_NAME, ...) \
    reflect_type_t* reflect_get_##TYPE_NAME() \
    { \
        static reflect_field_t fields_info[RE_FOREACH(RE_SUM, 0, __VA_ARGS__)] = \
        { \
            RE_FOREACH(RE_FIELD_INFO, TYPE_NAME, __VA_ARGS__) \
        }; \
        static reflect_type_t type_info = \
        { \
            RE_STRUCT, \
            NULL, \
            #TYPE_NAME, \
            RE_FOREACH(RE_SUM, 0, __VA_ARGS__), \
            sizeof(TYPE_NAME), \
            RE_FOREACH(RE_SIZEOF, 0, __VA_ARGS__), \
            fields_info \
        }; \
        return &type_info; \
    }

#define RE_DEFINE_PROTO(TYPE_NAME) C_API reflect_type_t* reflect_get_##TYPE_NAME(void);

#define RE_DEFINE_STRUCT(TYPE_NAME, ...) \
    typedef struct \
    { \
        RE_FOREACH(RE_DECLARE_FIELD, 0, __VA_ARGS__) \
    } TYPE_NAME; \
    RE_DEFINE_PROTO(TYPE_NAME) \
    RE_DEFINE_METHOD(TYPE_NAME, __VA_ARGS__)

C_API size_t reflect_num_fields(reflect_type_t *type);
C_API const char *reflect_type_of(reflect_type_t *type);
C_API reflect_types reflect_type_enum(reflect_type_t *type);
C_API size_t reflect_type_size(reflect_type_t *type);
C_API size_t reflect_packed_size(reflect_type_t *type);
C_API reflect_field_t *reflect_value_of(reflect_type_t *type);

C_API const char *reflect_field_type(reflect_type_t *, int);
C_API const char *reflect_field_name(reflect_type_t *, int);
C_API size_t reflect_field_size(reflect_type_t *, int);
C_API size_t reflect_field_offset(reflect_type_t *, int);
C_API bool reflect_field_is_signed(reflect_type_t *, int);
C_API int reflect_field_array_size(reflect_type_t *, int);
C_API reflect_types reflect_field_enum(reflect_type_t *, int);

#ifdef __cplusplus
}
#endif

#endif /* REFLECT_H_ */
