#ifndef ZE_LANG_H_
#define ZE_LANG_H_

#include "../include/coroutine.h"

#ifdef __cplusplus
extern "C" {
#endif

#define $(list, index) map_get((list), slice_find((list), index))->value
#define $$(list, index, value) map_put((list), slice_find((list), index), (value))

#define in ,
#define array_free map_free
#define kv(key, value) (key), (value)
#define has(i) map_macro_type((i))->value
#define has_t(i) map_macro_type((i))
#define indic(i) iter_key(i)
#define foreach_xp(X, A) X A
#define foreach_in(X, S) for(map_iter_t \
  *(X) = iter_new((map_t *)(S), true); \
  X != NULL; \
  X = iter_next(X))
#define reverse_in(X, S) for(map_iter_t \
  *(X) = iter_new((map_t *)(S), false); \
  X != NULL; \
  X = iter_next(X))
#define foreach(...) foreach_xp(foreach_in, (__VA_ARGS__))
#define reverse(...) foreach_xp(reverse_in, (__VA_ARGS__))
#define ranging(...) foreach(__VA_ARGS__)

#define c_int(data) co_value((data)).integer
#define c_long(data) co_value((data)).s_long
#define c_int64(data) co_value((data)).long_long
#define c_unsigned_int(data) co_value((data)).u_int
#define c_unsigned_long(data) co_value((data)).u_long
#define c_size_t(data) co_value((data)).max_size
#define c_const_char(data) co_value((data)).str
#define c_char(data) co_value((data)).schar
#define c_char_ptr(data) co_value((data)).char_ptr
#define c_bool(data) co_value((data)).boolean
#define c_float(data) co_value((data)).point
#define c_double(data) co_value((data)).precision
#define c_unsigned_char(data) co_value((data)).uchar
#define c_char_ptr_ptr(data) co_value((data)).array
#define c_unsigned_char_ptr(data) co_value((data)).uchar_ptr
#define c_short(data) co_value((data)).s_short
#define c_unsigned_short(data) co_value((data)).u_short
#define c_void_ptr(data) co_value((data)).object
#define c_callable(data) co_value((data)).func
#define c_void_cast(type, data) (type *)co_value((data)).object

#define c_integer(value) co_data((value)).integer
#define c_signed_long(value) co_data((value)).s_long
#define c_long_long(value) co_data((value)).long_long
#define c_unsigned_integer(value) co_data((value)).u_int
#define c_unsigned_long_int(value) co_data((value)).u_long
#define c_unsigned_long_long(value) co_data((value)).max_size
#define c_string(value) co_data((value)).str
#define c_signed_chars(value) co_data((value)).schar
#define c_const_chars(value) co_data((value)).char_ptr
#define c_boolean(value) co_data((value)).boolean
#define c_point(value) co_data((value)).point
#define c_precision(value) co_data((value)).precision
#define c_unsigned_chars(value) co_data((value)).uchar
#define c_chars_array(value) co_data((value)).array
#define c_unsigned_chars_ptr(value) co_data((value)).uchar_ptr
#define c_signed_shorts(value) co_data((value)).s_short
#define c_unsigned_shorts(value) co_data((value)).u_short
#define c_object(value) co_data((value)).object
#define c_func(value) co_data((value)).func
#define c_object_cast(type, value) (type *)co_data((value)).object

#define var_int(arg) (arg).value.integer
#define var_long(arg) (arg).value.s_long
#define var_long_long(arg) (arg).value.long_long
#define var_unsigned_int(arg) (arg).value.u_int
#define var_unsigned_long(arg) (arg).value.u_long
#define var_size_t(arg) (arg).value.max_size
#define var_const_char_512(arg) (arg).value.str
#define var_char(arg) (arg).value.schar
#define var_char_ptr(arg) (arg).value.char_ptr
#define var_bool(arg) (arg).value.boolean
#define var_float(arg) (arg).value.point
#define var_double(arg) (arg).value.precision
#define var_unsigned_char(arg) (arg).value.uchar
#define var_char_array(arg) (arg).value.array
#define var_unsigned_char_ptr(arg) (arg).value.uchar_ptr
#define var_signed_short(arg) (arg).value.s_short
#define var_unsigned_short(arg) (arg).value.u_short
#define var_ptr(arg) (arg).value.object
#define var_func(arg) (arg).value.func
#define var_cast_ptr(type, arg) (type *)(arg).value.object

#define has_int(arg) has_t((arg))->value.integer
#define has_long(arg) has_t((arg))->value.s_long
#define has_long_long(arg) has_t((arg))->value.long_long
#define has_unsigned_int(arg) has_t((arg))->value.u_int
#define has_unsigned_long(arg) has_t((arg))->value.u_long
#define has_size_t(arg) has_t((arg))->value.max_size
#define has_const_char_64(arg) has_t((arg))->value.str
#define has_char(arg) has_t((arg))->value.schar
#define has_char_ptr(arg) has_t((arg))->value.char_ptr
#define has_bool(arg) has_t((arg))->value.boolean
#define has_float(arg) has_t((arg))->value.point
#define has_double(arg) has_t((arg))->value.precision
#define has_unsigned_char(arg) has_t((arg))->value.uchar
#define has_char_ptr_ptr(arg) has_t((arg))->value.array
#define has_unsigned_char_ptr(arg) has_t((arg))->value.uchar_ptr
#define has_signed_short(arg) has_t((arg))->value.s_short
#define has_unsigned_short(arg) has_t((arg))->value.u_short
#define has_ptr(arg) has_t((arg))->value.object
#define has_func(arg) has_t((arg))->value.func
#define has_cast_ptr(type, arg) (type *)has_t((arg))->value.object

/* An macro that stops the ordinary flow of control and begins panicking,
throws an exception of given message. */
#define error(message) co_panic(message)
/**
* Creates a reflection structure and reflection function as `reflect_get_*TYPE_NAME*()`.
* Allows the inspect of data structures at runtime:
* - field types
* - field names
* - size of array fields
* - size of field
*
* `TYPE_NAME` - name of your structure
* (`DATA_TYPE`, `C_TYPE`, `FIELD_NAME`[, `ARRAY_SIZE`]) - comma-separated list of fields in the structure
*
* - DATA_TYPE - type of field (INTEGER, STRING, ENUM, PTR, FLOAT, DOUBLE, or STRUCT)
* - C_TYPE - type of the field (e.g. int, uint64, char, etc.)
* - FIELD_NAME - name of the field
* - ARRAY_SIZE - size of array, if a field is an array
*/
#define ze_struct(TYPE_NAME, ...) RE_DEFINE_STRUCT(TYPE_NAME, (ENUM, value_types, type), __VA_ARGS__)

/**
* Creates a reflection function as `reflect_get_*TYPE_NAME*()`.
*
* Allows the inspect of data structures at runtime:
* - field types
* - field names
* - size of array fields
* - size of field
*
* `TYPE_NAME` - name of your structure
* (`DATA_TYPE`, `C_TYPE`, `FIELD_NAME`[, `ARRAY_SIZE`]) - comma-separated list of fields in the structure
*
* - DATA_TYPE - type of field (INTEGER, STRING, ENUM, PTR, FLOAT, DOUBLE, or STRUCT)
* - C_TYPE - type of the field (e.g. int, uint64, char, etc.)
* - FIELD_NAME - name of the field
* - ARRAY_SIZE - size of array, if a field is an array
*/
#define ze_func(TYPE_NAME, ...) RE_DEFINE_METHOD(TYPE_NAME, (ENUM, value_types, type), __VA_ARGS__)

/**
* Creates a reflection function proto as `extern reflect_type_t *reflect_get_*TYPE_NAME*(void);`
*/
#define ze_proto(TYPE_NAME) RE_DEFINE_PROTO(TYPE_NAME)

#ifdef __cplusplus
}
#endif

#endif /* ZE_LANG_H_ */
