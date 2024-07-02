#ifndef RAII_DEFINE_TYPES_H
#define RAII_DEFINE_TYPES_H

#include <stddef.h>

#if defined(_WIN32) || defined(_WIN64)
    #include "compat/sys/time.h"
    #include <excpt.h>
    #ifndef SYS_CONSOLE
        /* O.S. physical ~input/output~ console `DEVICE`. */
        #define SYS_CONSOLE "\\\\?\\CON"
        /* O.S. physical ~null~ `DEVICE`. */
        #define SYS_NULL "\\\\?\\NUL"
        /* O.S. physical ~pipe~ prefix `string name` including trailing slash. */
        #define SYS_PIPE "\\\\.\\pipe\\"
    #endif
#else
    #ifndef SYS_CONSOLE
        /* O.S. physical ~input/output~ console `DEVICE`. */
        #define SYS_CONSOLE "/dev/tty"
        /* O.S. physical ~null~ `DEVICE`. */
        #define SYS_NULL "/dev/null"
        /* O.S. physical ~pipe~ prefix `string name` including trailing slash. */
        #define SYS_PIPE "./"
    #endif
    #include <libgen.h>
#endif

/* Unsigned types. */
typedef unsigned char u8;
/* Unsigned types. */
typedef unsigned short u16;
/* Unsigned types. */
typedef unsigned int u32;
/* Unsigned types. */
typedef unsigned long long u64;

/* Signed types. */
typedef signed char s8;
/* Signed types. */
typedef signed short s16;
/* Signed types. */
typedef signed int s32;
/* Signed types. */
typedef signed long long s64;

/* Regular types. */
typedef char i8;
/* Regular types. */
typedef short i16;
/* Regular types. */
typedef int i32;
/* Regular types. */
typedef long long i64;

/* Floating point types. */
typedef float f32;
/* Double floating point types. */
typedef double f64;

/* Boolean types. */
typedef u8 b8;
/* Boolean types. */
typedef u32 b32;

/* Void pointer types. */
typedef void *void_t;
/* Const void pointer types. */
typedef const void *const_t;

/* Char pointer types. */
typedef char *string;
/* Const char pointer types. */
typedef const char *string_t;
/* Unsigned char pointer types. */
typedef unsigned char *u_string;
/* Const unsigned char pointer types. */
typedef const unsigned char *u_string_t;
/* Const unsigned char types. */
typedef const unsigned char u_char_t;

#ifndef __cplusplus
#define nullptr ((void*)0)
#endif

/**
 * Simple macro for making sure memory addresses are aligned
 * to the nearest power of two
 */
#ifndef align_up
#define align_up(num, align) (((num) + ((align)-1)) & ~((align)-1))
#endif

#ifndef container_of
#define container_of(ptr, type, member) \
                      ((type *) ((char *)(ptr) - offsetof(type, member)))
#endif

#if defined(__clang__)
#  define COMPILER_CLANG
#elif defined(_MSC_VER)
#  define COMPILER_CL
#elif defined(__GNUC__)
#  define COMPILER_GCC
#endif

#if defined(COMPILER_CLANG)
#  define FILE_NAME __FILE_NAME__
#else
#  define FILE_NAME __FILE__
#endif

#define Statement(s) do {\
s\
} while (0)

#define trace Statement(printf("%s:%d: Trace\n", FILE_NAME, __LINE__);)
#define unreachable Statement(printf("How did we get here? In %s on line %d\n", FILE_NAME, __LINE__);)
#define PATH_MAX 4096

#define Gb(count) (u64) (count * 1024 * 1024 * 1024)
#define Mb(count) (u64) (count * 1024 * 1024)
#define Kb(count) (u64) (count * 1024)

#endif /* RAII_DEFINE_TYPES_H */
