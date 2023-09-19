#include "../include/coroutine.h"

CO_FORCE_INLINE value_types type_of(void_t self) {
    return ((var_t *)self)->type;
}

CO_FORCE_INLINE bool is_type(void_t self, value_types check) {
    return type_of(self) == check;
}

CO_FORCE_INLINE bool is_instance_of(void_t self, void_t check) {
    return type_of(self) == type_of(check);
}

CO_FORCE_INLINE bool is_value(void_t self) {
    return (type_of(self) > CO_NULL) && (type_of(self) < CO_NONE);
}

CO_FORCE_INLINE bool is_instance(void_t self) {
    return (type_of(self) > CO_NONE) && (type_of(self) < CO_NO_INSTANCE);
}

CO_FORCE_INLINE bool is_valid(void_t self) {
    return is_value(self) || is_instance(self);
}

CO_FORCE_INLINE bool is_reflection(void_t self) {
    return ((reflect_kind_t *)self)->fields
        && ((reflect_kind_t *)self)->name
        && ((reflect_kind_t *)self)->num_fields
        && ((reflect_kind_t *)self)->size
        && ((reflect_kind_t *)self)->packed_size;
}

/*
TODO:
    RE_NULL = -1,
    RE_SLONG,
    RE_ULONG,
    RE_SHORT,
    RE_USHORT,
    RE_CHAR,
    RE_UCHAR,
    RE_ARRAY,
    RE_HASH,
    RE_NONE,
    RE_DEF_ARR,
    RE_DEF_FUNC,
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
    RE_UV_ARG,
    RE_SCHED,
    RE_CHANNEL,
    RE_VALUE,
    RE_NO_INSTANCE
*/


string_t reflect_kind(void_t value) {
    reflect_types res = (reflect_types)type_of(value);
    if (res == RE_STRUCT) {
        if (strcmp(reflect_type_of((reflect_type_t *)value), "var_t") == 0) {
            char out[CO_SCRAPE_SIZE];
            reflect_get_field((reflect_type_t *)value, 0, out);
            res = c_int(out);
        }
    }
    switch (res) {
        case RE_STRUCT:
            return "struct";
        case RE_CONST_CHAR:
            return "const char *";
        case RE_STRING:
        //    return "string";
        case RE_CHAR_P:
            return "char *";
        case RE_UCHAR_P:
            return "unsigned char *";
        case RE_INTEGER:
        case RE_INT:
            return "int";
        case RE_UINT:
            return "unsigned int";
        case RE_MAXSIZE:
            return "unsigned long long";
        case RE_LLONG:
            return "long long";
        case RE_ENUM:
            return "enum";
        case RE_BOOL:
            return "unsigned char";
        case RE_FLOAT:
            return "float";
        case RE_DOUBLE:
            return "double";
        case RE_OBJ:
        case RE_PTR:
            return "* ptr";
        case RE_FUNC:
            return "*(*)(*) callable";
        case RE_REFLECT_TYPE:
            return "<> reflect";
    }

    return "Unknown error";
}

CO_FORCE_INLINE void reflect_with(reflect_type_t *type, void_t value) {
    type->instance = value;
}

CO_FORCE_INLINE reflect_field_t *reflect_value_of(reflect_type_t *type) {
    return type->fields;
}

CO_FORCE_INLINE size_t reflect_num_fields(reflect_type_t *type) {
    return type->fields_count;
}

CO_FORCE_INLINE string_t reflect_type_of(reflect_type_t *type) {
    return type->name;
}

CO_FORCE_INLINE reflect_types reflect_type_enum(reflect_type_t *type) {
    return type->data_type;
}

CO_FORCE_INLINE size_t reflect_type_size(reflect_type_t *type) {
    return type->size;
}

CO_FORCE_INLINE size_t reflect_packed_size(reflect_type_t *type) {
    return type->packed_size;
}

CO_FORCE_INLINE string_t reflect_field_type(reflect_type_t *type, int slot) {
    return (type->fields + slot)->field_type;
}

CO_FORCE_INLINE string_t reflect_field_name(reflect_type_t *type, int slot) {
    return (type->fields + slot)->field_name;
}

CO_FORCE_INLINE size_t reflect_field_size(reflect_type_t *type, int slot) {
    return (type->fields + slot)->size;
}

CO_FORCE_INLINE size_t reflect_field_offset(reflect_type_t *type, int slot) {
    return (type->fields + slot)->offset;
}

CO_FORCE_INLINE bool reflect_field_is_signed(reflect_type_t *type, int slot) {
    return (type->fields + slot)->is_signed;
}

CO_FORCE_INLINE int reflect_field_array_size(reflect_type_t *type, int slot) {
    return (type->fields + slot)->array_size;
}

CO_FORCE_INLINE reflect_types reflect_field_enum(reflect_type_t *type, int slot) {
    return (type->fields + slot)->data_type;
}

CO_FORCE_INLINE void reflect_set_field(reflect_type_t *variable, int slot, void_t value) {
    memcpy((string)variable->instance + (variable->fields + slot)->offset, value, (variable->fields + slot)->size);
}

CO_FORCE_INLINE void reflect_get_field(reflect_type_t *variable, int slot, void_t out) {
    memcpy(out, (string)variable->instance + (variable->fields + slot)->offset, (variable->fields + slot)->size);
}


reflect_func(var_t,
             (PTR, void_t, value)
)
reflect_func(co_array_t,
             (PTR, void_t, base), (MAXSIZE, size_t, elements)
)
reflect_func(defer_t,
             (STRUCT, co_array_t, base)
)
reflect_func(defer_func_t,
             (FUNC, func_t, func), (PTR, void_t, data), (PTR, void_t, check)
)
reflect_func(promise,
             (STRUCT, co_value_t *, result), (STRUCT, pthread_mutex_t, mutex),
             (STRUCT, pthread_cond_t, cond),
             (BOOL, bool, done),
             (INTEGER, int, id)
)
reflect_func(future,
             (STRUCT, pthread_t, thread), (STRUCT, pthread_attr_t, attr),
             (FUNC, callable_t, func),
             (INTEGER, int, id),
             (STRUCT, promise *, value)
)
reflect_func(future_arg,
             (FUNC, callable_t, func), (PTR, void_t, arg), (STRUCT, promise *, value)
)
reflect_func(co_scheduler_t,
             (STRUCT, co_routine_t *, head), (STRUCT, co_routine_t *, tail)
)
reflect_func(uv_args_t,
             (STRUCT, co_value_t *, args),
             (STRUCT, co_routine_t *, context),
             (BOOL, bool, is_path),
             (ENUM, uv_fs_type, fs_type),
             (ENUM, uv_req_type, req_type),
             (ENUM, uv_handle_type, handle_type),
             (ENUM, uv_dirent_type_t, dirent_type),
             (ENUM, uv_tty_mode_t, tty_mode),
             (ENUM, uv_stdio_flags, stdio_flag),
             (ENUM, uv_errno_t, errno_code),
             (MAXSIZE, size_t, n_args)
)
reflect_func(channel_t,
             (UINT, unsigned int, bufsize),
             (UINT, unsigned int, elem_size),
             (UCHAR_P, unsigned char *, buf),
             (UINT, unsigned int, nbuf),
             (UINT, unsigned int, off),
             (UINT, unsigned int, id),
             (STRUCT, co_value_t *, tmp),
             (STRUCT, msg_queue_t, a_send),
             (STRUCT, msg_queue_t, a_recv),
             (STRING, string, name),
             (BOOL, bool, select_ready)
)
reflect_func(array_item_t,
             (STRUCT, map_value_t *, value),
             (STRUCT, array_item_t *, prev),
             (STRUCT, array_item_t *, next),
             (LLONG, int64_t, indic),
             (CONST_CHAR, string_t, key)
)
reflect_func(map_t,
             (STRUCT, array_item_t *, head),
             (STRUCT, array_item_t *, tail),
             (STRUCT, ht_map_t *, dict),
             (FUNC, map_value_dtor, dtor),
             (LLONG, int64_t, indices),
             (LLONG, int64_t, length),
             (INTEGER, int, no_slices),
             (STRUCT, slice_t **, slice),
             (ENUM, value_types, item_type),
             (ENUM, map_data_type, as),
             (BOOL, bool, started),
             (BOOL, bool, sliced)
)
reflect_func(map_iter_t,
             (STRUCT, map_t *, array),
             (STRUCT, array_item_t *, item),
             (BOOL, bool, forward)
)
reflect_func(ex_ptr_t,
             (STRUCT, ex_ptr_t *, next),
             (FUNC, func_t, func),
             (OBJ, void_t *, ptr)
)
reflect_func(ex_context_t,
             (STRUCT, ex_context_t *, next),
             (STRUCT, ex_ptr_t *, stack),
             (STRUCT, co_routine_t *, co),
             (CONST_CHAR, volatile const char *, function),
             (CONST_CHAR, volatile const char *, ex),
             (CONST_CHAR, volatile const char *, file),
             (INTEGER, int volatile, line),
             (INTEGER, int volatile, state),
             (INTEGER, int, unstack)
)
