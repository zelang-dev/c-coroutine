#include "coroutine.h"

CO_FORCE_INLINE bool is_json(json_t *schema) {
    return (is_empty(schema) || json_value_get_type(schema) == JSONError) ? false : true;
}

string json_serialize(json_t *json, bool is_pretty) {
    string json_string = NULL;
    if (!is_empty(json)) {
        if (is_pretty)
            json_string = json_serialize_to_string_pretty((const json_t *)json);
        else
            json_string = json_serialize_to_string((const json_t *)json);

        defer(json_free_serialized_string, json_string);
    }

    return json_string;
}

CO_FORCE_INLINE int json_write(string_t filename, string_t text) {
    return fs_write_file(filename, text);
}

CO_FORCE_INLINE json_t *json_parse(string_t text) {
    return json_parse_string(text);
}

json_t *json_read(string_t filename) {
    string file_contents = fs_readfile(filename);
    if (is_empty(file_contents))
        return NULL;

    return json_parse(file_contents);
}

json_t *json_encode(string_t desc, ...) {
    int count = (int)strlen(desc);
    json_t *json_root = json_value_init_object();
    defer(json_value_free, json_root);
    JSON_Object *json_object = json_value_get_object(json_root);

    va_list argp;
    string key, value_char;
    int value_bool;
    JSON_Status status = JSONSuccess;
    void_t value_any = NULL;
    JSON_Array *value_array = NULL;
    double value_float = 0;
    int64_t value_int = 0;
    size_t value_max = 0;
    bool is_dot = false, is_array = false, is_double = false, is_int = false, is_max = false;

    va_start(argp, desc);
    for (int i = 0; i < count; i++) {
        switch (*desc++) {
            case '.':
                is_dot = true;
                break;
            case 'e':
                if (is_array) {
                    is_array = false;
                    value_array = NULL;
                    is_dot = false;
                }
                break;
            case 'a':
                if (!is_array) {
                    key = va_arg(argp, string);
                    status = json_object_set_value(json_object, key, json_value_init_array());
                    value_array = json_object_get_array(json_object, key);
                    is_array = true;
                    is_dot = false;
                }
                break;
            case 'n':
                if (!is_array)
                    key = va_arg(argp, string);

                if (is_array)
                    status = json_array_append_null(value_array);
                else if (is_dot)
                    status = json_object_dotset_null(json_object, key);
                else
                    status = json_object_set_null(json_object, key);
                is_dot = false;
                break;
            case 'd':
                is_int = true;
            case 'f':
                if (!is_int)
                    is_double = true;
            case 'i':
                if (!is_double && !is_int)
                    is_max = true;

                if (!is_array)
                    key = va_arg(argp, string);

                if (is_double)
                    value_float = va_arg(argp, double);
                else if (is_int)
                    value_int = va_arg(argp, int64_t);
                else
                    value_max = va_arg(argp, size_t);

                if (is_array)
                    status = json_array_append_number(value_array, (is_double ? value_float
                                                                    : is_int ? (int)value_int
                                                                    : (unsigned long)value_max));
                else if (is_dot)
                    status = json_object_dotset_number(json_object, key, (is_double ? value_float
                                                                          : is_int ? (int)value_int
                                                                          : (unsigned long)value_max));
                else
                    status = json_object_set_number(json_object, key, (is_double ? value_float
                                                                       : is_int ? (int)value_int
                                                                       : (unsigned long)value_max));

                is_dot = false;
                is_double = false;
                is_int = false;
                is_max = false;
                break;
            case 'b':
                if (!is_array)
                    key = va_arg(argp, string);

                value_bool = va_arg(argp, int);
                if (is_array)
                    status = json_array_append_boolean(value_array, value_bool);
                else if (is_dot)
                    status = json_object_dotset_boolean(json_object, key, value_bool);
                else
                    status = json_object_set_boolean(json_object, key, value_bool);
                is_dot = false;
                break;
            case 's':
                if (!is_array)
                    key = va_arg(argp, string);

                value_char = va_arg(argp, string);
                if (is_array)
                    status = json_array_append_string(value_array, value_char);
                else if (is_dot)
                    status = json_object_dotset_string(json_object, key, value_char);
                else
                    status = json_object_set_string(json_object, key, value_char);
                is_dot = false;
                break;
            case 'v':
                if (!is_array)
                    key = va_arg(argp, string);

                value_any = va_arg(argp, void_t);
                if (is_array)
                    status = json_array_append_value(value_array, value_any);
                else if (is_dot)
                    status = json_object_dotset_value(json_object, key, value_any);
                else
                    status = json_object_set_value(json_object, key, value_any);
                is_dot = false;
                break;
            default:
                break;
        }
    }
    va_end(argp);

    return json_root;
}
