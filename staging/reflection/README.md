# mkcreflect
[![Build Status](https://github.com/loganek/mkcreflect/actions/workflows/build_on_linux_windows.yml/badge.svg?event=push)

A reflection library for C

## Features
This library allows you to inspect your data structures at runtime:
 * field types
 * field names
 * size of array fields
 * size of field

## Documentation
There's only one public macro which you should use:
```c
MKCREFLECT_DEFINE_STRUCT(StructName,
    (DATA_TYPE, FIELD1_NAME, C_TYPE1[, ARRAY_SIZE_1])[,
    (DATA_TYPE, FIELD2_NAME, C_TYPE2[, ARRAY_SIZE_2])[, ...])
```
 * **StructName** - name of your structure
 * **(DATA_TYPE, FIELD1_NAME, C_TYPE1[, ARRAY_SIZE_1])** - comma-separated list of fields in the structure
   * **DATA_TYPE** - type of field (INTEGER, STRING or STRUCT)
   * **FIELD_NAME** - name of the field
   * **C_TYPE** - type of the field (e.g. int, uint64, char, etc.)
   * **ARRAY_SIZE** - size of array, if a field is an array

Also, see [examples](examples).

## Integration to your project
Just copy a **lib/include/mkcreflect.h** file to your project and include it wherever you want to use it.

## Example
There are a few examples in [examples](examples) directory.

## License
Distributed under the [Beerware license](LICENSE).
