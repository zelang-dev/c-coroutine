/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <marcin.kolny@gmail.com> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return. Marcin Kolny
 * ----------------------------------------------------------------------------
 * http://github.com/loganek/mkcreflect
 */
#ifndef MKCREFLECT_TEST_STRUCT_H_
#define MKCREFLECT_TEST_STRUCT_H_

#include <mkcreflect.h>
#include <inttypes.h>

MKCREFLECT_DEFINE_STRUCT(TestStruct,
    (INTEGER, int, int_field),
    (STRING, char, array_field, 20),
    (INTEGER, size_t, size_field))

#endif /* MKCREFLECT_TEST_STRUCT_H_ */
