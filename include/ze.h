#ifndef ZE_LANG_H_
#define ZE_LANG_H_

#include "../include/coroutine.h"

#define var(data) co_var((data))->value
#define as_var(variable, variable_type, data, enum_type) var_t *variable = (var_t *)co_new_by(1, sizeof(var_t)); \
    variable->type = enum_type; \
    variable->value = (variable_type *)co_new_by(1, sizeof(variable_type) + sizeof(data)); \
    memcpy(variable->value, &data, sizeof(data))

#define as_char(variable, data) as_var(variable, char, data, CO_CHAR_P)
#define as_string(variable, data) as_var(variable, char, data, CO_STRING)
#define as_long(variable, data) as_var(variable, long, data, CO_LONG)
#define as_int(variable, data) as_var(variable, int, data, CO_INTEGER)
#define as_uchar(variable, data) as_var(variable, unsigned char, data, CO_UCHAR)

#define as_reflect(variable, type, value) reflect_type_t *variable = reflect_get_##type(); \
    variable->instance = value;

#define as_var_ref(variable, type, data, enum_type) as_var(variable, type, data, enum_type); \
    as_reflect(variable##_r, var_t, variable)

#define as_instance(variable, variable_type) variable_type *variable = (variable_type *)co_new_by(1, sizeof(variable_type)); \
    variable->type = CO_STRUCT;

#define as_instance_ref(variable, type) as_instance(variable, type); \
    as_reflect(variable##_r, type, variable)

#define $(list, index) map_get((list), slice_find((list), index))->value
#define $$(list, index, value) map_put((list), slice_find((list), index), (value))
#define $$$(list, index) map_del((list), slice_find((list), index))

#define pop(list) map_pop((list))
#define push(list, value) map_push((list), (value))

#define unshift(list) map_unshift((list))
#define shift(list, value) map_shift((list), (value))

#define in ,
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

#define match(variable_type) switch (type_of(variable_type))
#define and(ENUM) case ENUM:
#define or(ENUM) break; case ENUM:
#define otherwise break; default:

/* The `for_select` macro sets up a coroutine to wait on multiple channel
operations. Must be closed out with `select_end`, and if no `select_case(channel)`, `select_case_if(channel)`, `select_break` provided, an infinite loop is created.

This behaves same as GoLang `select {}` statement. */
#define for_select       \
  bool ___##__FUNCTION__; \
  while (true)            \
  {                       \
      ___##__FUNCTION__ = false; \
    if (true)

#define select_end              \
  if (___##__FUNCTION__ == false) \
      coroutine_yield();          \
  }

#define select_case(ch)                                 \
  if ((ch)->select_ready && ___##__FUNCTION__ == false) \
  {                                                     \
      (ch)->select_ready = false; if (true)

#define select_break      \
  ___##__FUNCTION__ = true; \
  }

#define select_case_if(ch) \
  select_break else select_case(ch)

/* The `select_default` is run if no other case is ready.
Must also closed out with `select_break()`. */
#define select_default()    \
  select_break if (___##__FUNCTION__ == false) {

#define await(func, arg) co_execute(FUNC_VOID(func), arg)
#define defer(func, arg) co_defer(FUNC_VOID(func), arg)
#define launch(func, arg) co_go(func, arg)
#define sleep_for(ms) co_sleep(ms)

#define thread(func, arg) co_async(func, arg)
#define thread_await(futures) co_async_wait(futures)
#define thread_get(futures) co_async_get(futures)

#define message() channel()
#define message_buf(s) channel_buf(s)
#define send_msg(c, i) co_send(c, i)
#define recv_msg(c) co_recv(c)

/* An macro that stops the ordinary flow of control and begins panicking,
throws an exception of given message `issue`. */
#define error(issue) co_panic(issue)

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
extern "C" {
#endif

ze_proto(var_t);
ze_proto(co_array_t);
ze_proto(defer_t);
ze_proto(defer_func_t);
ze_proto(promise);
ze_proto(future);
ze_proto(future_arg);
ze_proto(channel_t);
ze_proto(map_t);
ze_proto(map_iter_t);
ze_proto(array_item_t);
ze_proto(ex_ptr_t);
ze_proto(ex_context_t);
ze_proto(co_scheduler_t);
ze_proto(uv_args_t);

#ifdef __cplusplus
}
#endif

#endif /* ZE_LANG_H_ */
