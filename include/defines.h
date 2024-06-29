#ifndef RAII_DEFINES_H
#define RAII_DEFINES_H

/* Unsigned types. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

/* Signed types. */
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

/* Regular types. */
typedef char i8;
typedef short i16;
typedef int i32;
typedef long long i64;

/* Floating point types. */
typedef float f32;
typedef double f64;

/* Boolean types. */
typedef u8  b8;
typedef u32 b32;

#ifndef __cplusplus
#define nullptr ((void*)0)
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

#endif /* RAII_DEFINES_H */
