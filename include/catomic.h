/*
C89 compatible atomics. Choice of public domain or MIT-0. See license statements at the end of this file.

David Reid - mackron@gmail.com
*/

#ifndef C_ATOMIC_H
#define C_ATOMIC_H
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wlong-long"
    #if defined(__clang__)
        #pragma GCC diagnostic ignored "-Wc++11-long-long"
    #endif
#endif

#ifndef __ATOMIC_RELAXED
#   define __ATOMIC_RELAXED 0
#   define __ATOMIC_CONSUME 1
#   define __ATOMIC_ACQUIRE 2
#   define __ATOMIC_RELEASE 3
#   define __ATOMIC_ACQ_REL 4
#   define __ATOMIC_SEQ_CST 5
#endif
#ifndef _STDATOMIC_H
/* Memory ordering */
typedef enum {
	memory_order_relaxed = __ATOMIC_RELAXED,
	memory_order_consume = __ATOMIC_CONSUME,
	memory_order_acquire = __ATOMIC_ACQUIRE,
	memory_order_release = __ATOMIC_RELEASE,
	memory_order_acq_rel = __ATOMIC_ACQ_REL,
	memory_order_seq_cst = __ATOMIC_SEQ_CST,
} memory_order;
#endif
typedef int c89atomic_memory_order;

/* Sized Types */
typedef   signed char           c89atomic_int8;
typedef unsigned char           c89atomic_uint8;
typedef   signed short          c89atomic_int16;
typedef unsigned short          c89atomic_uint16;
typedef   signed int            c89atomic_int32;
typedef unsigned int            c89atomic_uint32;
#if defined(_MSC_VER) && !defined(__clang__)
    typedef   signed __int64    c89atomic_int64;
    typedef unsigned __int64    c89atomic_uint64;
#else
    typedef   signed long long  c89atomic_int64;
    typedef unsigned long long  c89atomic_uint64;
#endif

typedef unsigned char           c89atomic_bool;
/* End Sized Types */

#if defined(__TINYC__) || defined(_MSC_VER)
#   define c89atomic_is_lock_free(obj) (sizeof((obj)) <= sizeof(void *))
#endif


/* Architecture Detection */
#if !defined(C89ATOMIC_64BIT) && !defined(C89ATOMIC_32BIT)
#ifdef _WIN32
#ifdef _WIN64
#define C89ATOMIC_64BIT
#else
#define C89ATOMIC_32BIT
#endif
#endif
#endif

#if !defined(C89ATOMIC_64BIT) && !defined(C89ATOMIC_32BIT)
#ifdef __GNUC__
#ifdef __LP64__
#define C89ATOMIC_64BIT
#else
#define C89ATOMIC_32BIT
#endif
#endif
#endif

#if !defined(C89ATOMIC_64BIT) && !defined(C89ATOMIC_32BIT)
#if INTPTR_MAX == INT64_MAX
#define C89ATOMIC_64BIT
#else
#define C89ATOMIC_32BIT
#endif
#endif

#if defined(__arm__) || defined(_M_ARM)
#define C89ATOMIC_ARM32
#endif
#if defined(__arm64) || defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
#define C89ATOMIC_ARM64
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define C89ATOMIC_X64
#elif defined(__i386) || defined(_M_IX86)
#define C89ATOMIC_X86
#elif defined(C89ATOMIC_ARM32) || defined(C89ATOMIC_ARM64)
#define C89ATOMIC_ARM
#endif
/* End Architecture Detection */

/* Inline */
/* We want to encourage the compiler to inline. When adding support for a new compiler, make sure it's handled here. */
#if defined(_MSC_VER)
#   define C89ATOMIC_INLINE __forceinline
#elif defined(__GNUC__)
    /*
    I've had a bug report where GCC is emitting warnings about functions possibly not being inlineable. This warning happens when
    the __attribute__((always_inline)) attribute is defined without an "inline" statement. I think therefore there must be some
    case where "__inline__" is not always defined, thus the compiler emitting these warnings. When using -std=c89 or -ansi on the
    command line, we cannot use the "inline" keyword and instead need to use "__inline__". In an attempt to work around this issue
    I am using "__inline__" only when we're compiling in strict ANSI mode.
    */
    #if defined(__STRICT_ANSI__) || !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
        #define C89ATOMIC_INLINE __inline__ __attribute__((always_inline))
    #else
        #define C89ATOMIC_INLINE inline __attribute__((always_inline))
    #endif
#elif defined(__WATCOMC__) || defined(__DMC__)
    #define C89ATOMIC_INLINE __inline
#else
    #define C89ATOMIC_INLINE
#endif
/* End Inline */

/* Assume everything supports all standard sized atomics by default. We'll #undef these when not supported. */
#define C89ATOMIC_HAS_8
#define C89ATOMIC_HAS_16
#define C89ATOMIC_HAS_32
#define C89ATOMIC_HAS_64

#if (defined(_MSC_VER) /*&& !defined(__clang__)*/) || defined(__WATCOMC__) || defined(__DMC__)
    /* Visual C++. */
    #define C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, intrin, c89atomicType, msvcType)   \
        c89atomicType result; \
        switch (order) \
        { \
            case c89atomic_memory_order_relaxed: \
            { \
                result = (c89atomicType)intrin##_nf((volatile msvcType*)dst, (msvcType)src); \
            } break; \
            case c89atomic_memory_order_consume: \
            case c89atomic_memory_order_acquire: \
            { \
                result = (c89atomicType)intrin##_acq((volatile msvcType*)dst, (msvcType)src); \
            } break; \
            case c89atomic_memory_order_release: \
            { \
                result = (c89atomicType)intrin##_rel((volatile msvcType*)dst, (msvcType)src); \
            } break; \
            case c89atomic_memory_order_acq_rel: \
            case c89atomic_memory_order_seq_cst: \
            default: \
            { \
                result = (c89atomicType)intrin((volatile msvcType*)dst, (msvcType)src); \
            } break; \
        } \
        return result;

    #define C89ATOMIC_MSVC_ARM_INTRINSIC_COMPARE_EXCHANGE(ptr, expected, desired, order, intrin, c89atomicType, msvcType)   \
        c89atomicType result; \
        switch (order) \
        { \
            case c89atomic_memory_order_relaxed: \
            { \
                result = (c89atomicType)intrin##_nf((volatile msvcType*)ptr, (msvcType)expected, (msvcType)desired); \
            } break; \
            case c89atomic_memory_order_consume: \
            case c89atomic_memory_order_acquire: \
            { \
                result = (c89atomicType)intrin##_acq((volatile msvcType*)ptr, (msvcType)expected, (msvcType)desired); \
            } break; \
            case c89atomic_memory_order_release: \
            { \
                result = (c89atomicType)intrin##_rel((volatile msvcType*)ptr, (msvcType)expected, (msvcType)desired); \
            } break; \
            case c89atomic_memory_order_acq_rel: \
            case c89atomic_memory_order_seq_cst: \
            default: \
            { \
                result = (c89atomicType)intrin((volatile msvcType*)ptr, (msvcType)expected, (msvcType)desired); \
            } break; \
        } \
        return result;

    #define c89atomic_memory_order_relaxed  0
    #define c89atomic_memory_order_consume  1
    #define c89atomic_memory_order_acquire  2
    #define c89atomic_memory_order_release  3
    #define c89atomic_memory_order_acq_rel  4
    #define c89atomic_memory_order_seq_cst  5

    /*
    Visual Studio 2003 (_MSC_VER 1300) and earlier have no support for sized atomic operations.
    We'll need to use inlined assembly for these compilers.

    I've also had a report that 8-bit and 16-bit interlocked intrinsics were only added in Visual
    Studio 2010 (_MSC_VER 1600). We'll need to disable these on the 64-bit build because there's
    no way to implement them with inlined assembly since Microsoft has decided to drop support for
    it from their 64-bit compilers.

    To simplify the implementation, any older MSVC compilers targeting 32-bit will use inlined
    assembly for everything. I'm not going to use inlined assembly wholesale for all 32-bit builds
    regardless of the age of the compiler because I don't trust the compiler will optimize the
    inlined assembly properly.

    The inlined assembly path supports both MSVC, Digital Mars and OpenWatcom. OpenWatcom is a little
    bit too pedantic with it's warnings. A few notes:
      - The return value of these functions are defined by the AL/AX/EAX/EAX:EDX registers which
        means an explicit return statement is not actually necessary. This is helpful for performance
        reasons because it means we can avoid the cost of a declaring a local variable and moving the
        value in EAX into that variable, only to then return it. However, unfortunately OpenWatcom
        thinks this is a mistake and tries to be helpful by throwing a warning. To work around we're
        going to declare a "result" variable and incur this theoretical cost. Most likely the
        compiler will optimize this away and make it a non-issue.
      - Variables that are assigned within the inline assembly will not be detected as such, and
        OpenWatcom will throw a warning about the variable being used without being assigned. To work
        around this we just initialize our local variables to 0.
    */
    #if _MSC_VER < 1600 && defined(C89ATOMIC_X86)   /* 1600 = Visual Studio 2010 */
        #define C89ATOMIC_MSVC_USE_INLINED_ASSEMBLY
    #endif
    #if _MSC_VER < 1600
        #undef C89ATOMIC_HAS_8
        #undef C89ATOMIC_HAS_16
    #endif

    /* We need <intrin.h>, but only if we're not using inlined assembly. */
    #if !defined(C89ATOMIC_MSVC_USE_INLINED_ASSEMBLY)
        #include <intrin.h>
    #endif

    /* atomic_compare_and_swap */
    #if defined(C89ATOMIC_MSVC_USE_INLINED_ASSEMBLY)
        #if defined(C89ATOMIC_HAS_8)
            static C89ATOMIC_INLINE c89atomic_uint8 __stdcall c89atomic_compare_and_swap_8(volatile c89atomic_uint8* dst, c89atomic_uint8 expected, c89atomic_uint8 desired)
            {
                c89atomic_uint8 result = 0;

                __asm {
                    mov ecx, dst
                    mov al,  expected
                    mov dl,  desired
                    lock cmpxchg [ecx], dl  /* Writes to EAX which MSVC will treat as the return value. */
                    mov result, al
                }

                return result;
            }
        #endif
        #if defined(C89ATOMIC_HAS_16)
            static C89ATOMIC_INLINE c89atomic_uint16 __stdcall c89atomic_compare_and_swap_16(volatile c89atomic_uint16* dst, c89atomic_uint16 expected, c89atomic_uint16 desired)
            {
                c89atomic_uint16 result = 0;

                __asm {
                    mov ecx, dst
                    mov ax,  expected
                    mov dx,  desired
                    lock cmpxchg [ecx], dx  /* Writes to EAX which MSVC will treat as the return value. */
                    mov result, ax
                }

                return result;
            }
        #endif
        #if defined(C89ATOMIC_HAS_32)
            static C89ATOMIC_INLINE c89atomic_uint32 __stdcall c89atomic_compare_and_swap_32(volatile c89atomic_uint32* dst, c89atomic_uint32 expected, c89atomic_uint32 desired)
            {
                c89atomic_uint32 result = 0;

                __asm {
                    mov ecx, dst
                    mov eax, expected
                    mov edx, desired
                    lock cmpxchg [ecx], edx /* Writes to EAX which MSVC will treat as the return value. */
                    mov result, eax
                }

                return result;
            }
        #endif
        #if defined(C89ATOMIC_HAS_64)
            static C89ATOMIC_INLINE c89atomic_uint64 __stdcall c89atomic_compare_and_swap_64(volatile c89atomic_uint64* dst, c89atomic_uint64 expected, c89atomic_uint64 desired)
            {
                c89atomic_uint32 resultEAX = 0;
                c89atomic_uint32 resultEDX = 0;

                __asm {
                    mov esi, dst    /* From Microsoft documentation: "... you don't need to preserve the EAX, EBX, ECX, EDX, ESI, or EDI registers." Choosing ESI since it's the next available one in their list. */
                    mov eax, dword ptr expected
                    mov edx, dword ptr expected + 4
                    mov ebx, dword ptr desired
                    mov ecx, dword ptr desired + 4
                    lock cmpxchg8b qword ptr [esi]  /* Writes to EAX:EDX which MSVC will treat as the return value. */
                    mov resultEAX, eax
                    mov resultEDX, edx
                }

                return ((c89atomic_uint64)resultEDX << 32) | resultEAX;
            }
        #endif
    #else
        #if defined(C89ATOMIC_HAS_8)
            #define c89atomic_compare_and_swap_8( dst, expected, desired) (c89atomic_uint8 )_InterlockedCompareExchange8((volatile char*)dst, (char)desired, (char)expected)
        #endif
        #if defined(C89ATOMIC_HAS_16)
            #define c89atomic_compare_and_swap_16(dst, expected, desired) (c89atomic_uint16)_InterlockedCompareExchange16((volatile short*)dst, (short)desired, (short)expected)
        #endif
        #if defined(C89ATOMIC_HAS_32)
            #define c89atomic_compare_and_swap_32(dst, expected, desired) (c89atomic_uint32)_InterlockedCompareExchange((volatile long*)dst, (long)desired, (long)expected)
        #endif
        #if defined(C89ATOMIC_HAS_64)
            #define c89atomic_compare_and_swap_64(dst, expected, desired) (c89atomic_uint64)_InterlockedCompareExchange64((volatile c89atomic_int64*)dst, (c89atomic_int64)desired, (c89atomic_int64)expected)
        #endif
    #endif


    /* atomic_exchange_explicit */
    #if defined(C89ATOMIC_MSVC_USE_INLINED_ASSEMBLY)
        #if defined(C89ATOMIC_HAS_8)
            static C89ATOMIC_INLINE c89atomic_uint8 __stdcall c89atomic_exchange_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
            {
                c89atomic_uint8 result = 0;

                (void)order;
                __asm {
                    mov ecx, dst
                    mov al,  src
                    lock xchg [ecx], al
                    mov result, al
                }

                return result;
            }
        #endif
        #if defined(C89ATOMIC_HAS_16)
            static C89ATOMIC_INLINE c89atomic_uint16 __stdcall c89atomic_exchange_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
            {
                c89atomic_uint16 result = 0;

                (void)order;
                __asm {
                    mov ecx, dst
                    mov ax,  src
                    lock xchg [ecx], ax
                    mov result, ax
                }

                return result;
            }
        #endif
        #if defined(C89ATOMIC_HAS_32)
            static C89ATOMIC_INLINE c89atomic_uint32 __stdcall c89atomic_exchange_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
            {
                c89atomic_uint32 result = 0;

                (void)order;
                __asm {
                    mov ecx, dst
                    mov eax, src
                    lock xchg [ecx], eax
                    mov result, eax
                }

                return result;
            }
        #endif
    #else
        #if defined(C89ATOMIC_HAS_8)
            static C89ATOMIC_INLINE c89atomic_uint8 __stdcall c89atomic_exchange_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
            {
            #if defined(C89ATOMIC_ARM)
                C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedExchange8, c89atomic_uint8, char);
            #else
                (void)order;    /* Always using the strongest memory order. */
                return (c89atomic_uint8)_InterlockedExchange8((volatile char*)dst, (char)src);
            #endif
            }
        #endif
        #if defined(C89ATOMIC_HAS_16)
            static C89ATOMIC_INLINE c89atomic_uint16 __stdcall c89atomic_exchange_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
            {
            #if defined(C89ATOMIC_ARM)
                C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedExchange16, c89atomic_uint16, short);
            #else
                (void)order;    /* Always using the strongest memory order. */
                return (c89atomic_uint16)_InterlockedExchange16((volatile short*)dst, (short)src);
            #endif
            }
        #endif
        #if defined(C89ATOMIC_HAS_32)
            static C89ATOMIC_INLINE c89atomic_uint32 __stdcall c89atomic_exchange_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
            {
            #if defined(C89ATOMIC_ARM)
                C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedExchange, c89atomic_uint32, long);
            #else
                (void)order;    /* Always using the strongest memory order. */
                return (c89atomic_uint32)_InterlockedExchange((volatile long*)dst, (long)src);
            #endif
            }
        #endif
        #if defined(C89ATOMIC_HAS_64) && defined(C89ATOMIC_64BIT)
            static C89ATOMIC_INLINE c89atomic_uint64 __stdcall c89atomic_exchange_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
            {
            #if defined(C89ATOMIC_ARM)
                C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedExchange64, c89atomic_uint64, long long);
            #else
                (void)order;    /* Always using the strongest memory order. */
                return (c89atomic_uint64)_InterlockedExchange64((volatile long long*)dst, (long long)src);
            #endif
            }
        #else
            /* Implemented below. */
        #endif
    #endif
    #if defined(C89ATOMIC_HAS_64) && !defined(C89ATOMIC_64BIT)   /* atomic_exchange_explicit_64() must be implemented in terms of atomic_compare_and_swap() on 32-bit builds, no matter the version of Visual Studio. */
        static C89ATOMIC_INLINE c89atomic_uint64 __stdcall c89atomic_exchange_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            c89atomic_uint64 oldValue;

            do {
                oldValue = *dst;
            } while (c89atomic_compare_and_swap_64(dst, oldValue, src) != oldValue);

            (void)order;    /* Always using the strongest memory order. */
            return oldValue;
        }
    #endif


    /* atomic_fetch_add */
    #if defined(C89ATOMIC_MSVC_USE_INLINED_ASSEMBLY)
        #if defined(C89ATOMIC_HAS_8)
            static C89ATOMIC_INLINE c89atomic_uint8 __stdcall c89atomic_fetch_add_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
            {
                c89atomic_uint8 result = 0;

                (void)order;
                __asm {
                    mov ecx, dst
                    mov al,  src
                    lock xadd [ecx], al
                    mov result, al
                }

                return result;
            }
        #endif
        #if defined(C89ATOMIC_HAS_16)
            static C89ATOMIC_INLINE c89atomic_uint16 __stdcall c89atomic_fetch_add_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
            {
                c89atomic_uint16 result = 0;

                (void)order;
                __asm {
                    mov ecx, dst
                    mov ax,  src
                    lock xadd [ecx], ax
                    mov result, ax
                }

                return result;
            }
        #endif
        #if defined(C89ATOMIC_HAS_32)
            static C89ATOMIC_INLINE c89atomic_uint32 __stdcall c89atomic_fetch_add_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
            {
                c89atomic_uint32 result = 0;

                (void)order;
                __asm {
                    mov ecx, dst
                    mov eax, src
                    lock xadd [ecx], eax
                    mov result, eax
                }

                return result;
            }
        #endif
    #else
        #if defined(C89ATOMIC_HAS_8)
            static C89ATOMIC_INLINE c89atomic_uint8 __stdcall c89atomic_fetch_add_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
            {
            #if defined(C89ATOMIC_ARM)
                C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedExchangeAdd8, c89atomic_uint8, char);
            #else
                (void)order;    /* Always using the strongest memory order. */
                return (c89atomic_uint8)_InterlockedExchangeAdd8((volatile char*)dst, (char)src);
            #endif
            }
        #endif
        #if defined(C89ATOMIC_HAS_16)
            static C89ATOMIC_INLINE c89atomic_uint16 __stdcall c89atomic_fetch_add_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
            {
            #if defined(C89ATOMIC_ARM)
                C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedExchangeAdd16, c89atomic_uint16, short);
            #else
                (void)order;    /* Always using the strongest memory order. */
                return (c89atomic_uint16)_InterlockedExchangeAdd16((volatile short*)dst, (short)src);
            #endif
            }
        #endif
        #if defined(C89ATOMIC_HAS_32)
            static C89ATOMIC_INLINE c89atomic_uint32 __stdcall c89atomic_fetch_add_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
            {
            #if defined(C89ATOMIC_ARM)
                C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedExchangeAdd, c89atomic_uint32, long);
            #else
                (void)order;    /* Always using the strongest memory order. */
                return (c89atomic_uint32)_InterlockedExchangeAdd((volatile long*)dst, (long)src);
            #endif
            }
        #endif
        #if defined(C89ATOMIC_HAS_64) && defined(C89ATOMIC_64BIT)
            static C89ATOMIC_INLINE c89atomic_uint64 __stdcall c89atomic_fetch_add_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
            {
            #if defined(C89ATOMIC_ARM)
                C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedExchangeAdd64, c89atomic_uint64, long long);
            #else
                (void)order;    /* Always using the strongest memory order. */
                return (c89atomic_uint64)_InterlockedExchangeAdd64((volatile long long*)dst, (long long)src);
            #endif
            }
        #else
            /* Implemented below. */
        #endif
    #endif
    #if defined(C89ATOMIC_HAS_64) && !defined(C89ATOMIC_64BIT)   /* 9atomic_fetch_add_explicit_64() must be implemented in terms of atomic_compare_and_swap() on 32-bit builds, no matter the version of Visual Studio. */
        static C89ATOMIC_INLINE c89atomic_uint64 __stdcall c89atomic_fetch_add_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            c89atomic_uint64 oldValue;
            c89atomic_uint64 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue + src;
            } while (c89atomic_compare_and_swap_64(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }
    #endif


    /* atomic_thread_fence */
    #if defined(C89ATOMIC_MSVC_USE_INLINED_ASSEMBLY)
        static C89ATOMIC_INLINE void __stdcall c89atomic_thread_fence(c89atomic_memory_order order)
        {
            (void)order;
            __asm {
                lock add [esp], 0
            }
        }
    #else
        #define c89atomic_thread_fence(order)   MemoryBarrier()
    #endif


    /*
    I'm not sure how to implement a compiler barrier for old MSVC so I'm just making it a thread_fence() and hopefully the compiler will see the volatile and not do
    any reshuffling. If anybody has a better idea on this please let me know! Cannot use _ReadWriteBarrier() as it has been marked as deprecated.
    */
    #define c89atomic_compiler_fence()      c89atomic_thread_fence(c89atomic_memory_order_seq_cst)
    #define c89atomic_signal_fence(order)   _ReadWriteBarrier()


    /* Atomic loads can be implemented in terms of a compare-and-swap. Need to implement as functions to silence warnings about `order` being unused. */
    #if defined(C89ATOMIC_HAS_8)
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_load_explicit_8(volatile const c89atomic_uint8* ptr, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC_COMPARE_EXCHANGE(ptr, 0, 0, order, _InterlockedCompareExchange8, c89atomic_uint8, char);
        #else
            (void)order;    /* Always using the strongest memory order. */
            return c89atomic_compare_and_swap_8((volatile c89atomic_uint8*)ptr, 0, 0);
        #endif
        }
    #endif
    #if defined(C89ATOMIC_HAS_16)
        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_load_explicit_16(volatile const c89atomic_uint16* ptr, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC_COMPARE_EXCHANGE(ptr, 0, 0, order, _InterlockedCompareExchange16, c89atomic_uint16, short);
        #else
            (void)order;    /* Always using the strongest memory order. */
            return c89atomic_compare_and_swap_16((volatile c89atomic_uint16*)ptr, 0, 0);
        #endif
        }
    #endif
    #if defined(C89ATOMIC_HAS_32)
        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_load_explicit_32(volatile const c89atomic_uint32* ptr, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC_COMPARE_EXCHANGE(ptr, 0, 0, order, _InterlockedCompareExchange, c89atomic_uint32, long);
        #else
            (void)order;    /* Always using the strongest memory order. */
            return c89atomic_compare_and_swap_32((volatile c89atomic_uint32*)ptr, 0, 0);
        #endif
        }
    #endif
    #if defined(C89ATOMIC_HAS_64)
        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_load_explicit_64(volatile const c89atomic_uint64* ptr, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC_COMPARE_EXCHANGE(ptr, 0, 0, order, _InterlockedCompareExchange64, c89atomic_uint64, long long);
        #else
            (void)order;    /* Always using the strongest memory order. */
            return c89atomic_compare_and_swap_64((volatile c89atomic_uint64*)ptr, 0, 0);
        #endif
        }
    #endif


    /* atomic_store() is the same as atomic_exchange() but returns void. */
    #if defined(C89ATOMIC_HAS_8)
        #define c89atomic_store_explicit_8( dst, src, order) (void)c89atomic_exchange_explicit_8 (dst, src, order)
    #endif
    #if defined(C89ATOMIC_HAS_16)
        #define c89atomic_store_explicit_16(dst, src, order) (void)c89atomic_exchange_explicit_16(dst, src, order)
    #endif
    #if defined(C89ATOMIC_HAS_32)
        #define c89atomic_store_explicit_32(dst, src, order) (void)c89atomic_exchange_explicit_32(dst, src, order)
    #endif
    #if defined(C89ATOMIC_HAS_64)
        #define c89atomic_store_explicit_64(dst, src, order) (void)c89atomic_exchange_explicit_64(dst, src, order)
    #endif


    /* fetch_sub() */
    #if defined(C89ATOMIC_HAS_8)
        static C89ATOMIC_INLINE c89atomic_uint8 __stdcall c89atomic_fetch_sub_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            c89atomic_uint8 oldValue;
            c89atomic_uint8 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint8)(oldValue - src);
            } while (c89atomic_compare_and_swap_8(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }
    #endif
    #if defined(C89ATOMIC_HAS_16)
        static C89ATOMIC_INLINE c89atomic_uint16 __stdcall c89atomic_fetch_sub_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            c89atomic_uint16 oldValue;
            c89atomic_uint16 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint16)(oldValue - src);
            } while (c89atomic_compare_and_swap_16(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }
    #endif
    #if defined(C89ATOMIC_HAS_32)
        static C89ATOMIC_INLINE c89atomic_uint32 __stdcall c89atomic_fetch_sub_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            c89atomic_uint32 oldValue;
            c89atomic_uint32 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue - src;
            } while (c89atomic_compare_and_swap_32(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }
    #endif
    #if defined(C89ATOMIC_HAS_64)
        static C89ATOMIC_INLINE c89atomic_uint64 __stdcall c89atomic_fetch_sub_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            c89atomic_uint64 oldValue;
            c89atomic_uint64 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue - src;
            } while (c89atomic_compare_and_swap_64(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }
    #endif


    /* fetch_and() */
    #if defined(C89ATOMIC_HAS_8)
        static C89ATOMIC_INLINE c89atomic_uint8 __stdcall c89atomic_fetch_and_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedAnd8, c89atomic_uint8, char);
        #else
            c89atomic_uint8 oldValue;
            c89atomic_uint8 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint8)(oldValue & src);
            } while (c89atomic_compare_and_swap_8(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        #endif
        }
    #endif
    #if defined(C89ATOMIC_HAS_16)
        static C89ATOMIC_INLINE c89atomic_uint16 __stdcall c89atomic_fetch_and_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedAnd16, c89atomic_uint16, short);
        #else
            c89atomic_uint16 oldValue;
            c89atomic_uint16 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint16)(oldValue & src);
            } while (c89atomic_compare_and_swap_16(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        #endif
        }
    #endif
    #if defined(C89ATOMIC_HAS_32)
        static C89ATOMIC_INLINE c89atomic_uint32 __stdcall c89atomic_fetch_and_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedAnd, c89atomic_uint32, long);
        #else
            c89atomic_uint32 oldValue;
            c89atomic_uint32 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue & src;
            } while (c89atomic_compare_and_swap_32(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        #endif
        }
    #endif
    #if defined(C89ATOMIC_HAS_64)
        static C89ATOMIC_INLINE c89atomic_uint64 __stdcall c89atomic_fetch_and_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedAnd64, c89atomic_uint64, long long);
        #else
            c89atomic_uint64 oldValue;
            c89atomic_uint64 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue & src;
            } while (c89atomic_compare_and_swap_64(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        #endif
        }
    #endif


    /* fetch_xor() */
    #if defined(C89ATOMIC_HAS_8)
        static C89ATOMIC_INLINE c89atomic_uint8 __stdcall c89atomic_fetch_xor_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedXor8, c89atomic_uint8, char);
        #else
            c89atomic_uint8 oldValue;
            c89atomic_uint8 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint8)(oldValue ^ src);
            } while (c89atomic_compare_and_swap_8(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        #endif
        }
    #endif
    #if defined(C89ATOMIC_HAS_16)
        static C89ATOMIC_INLINE c89atomic_uint16 __stdcall c89atomic_fetch_xor_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedXor16, c89atomic_uint16, short);
        #else
            c89atomic_uint16 oldValue;
            c89atomic_uint16 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint16)(oldValue ^ src);
            } while (c89atomic_compare_and_swap_16(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        #endif
        }
    #endif
    #if defined(C89ATOMIC_HAS_32)
        static C89ATOMIC_INLINE c89atomic_uint32 __stdcall c89atomic_fetch_xor_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedXor, c89atomic_uint32, long);
        #else
            c89atomic_uint32 oldValue;
            c89atomic_uint32 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue ^ src;
            } while (c89atomic_compare_and_swap_32(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        #endif
        }
    #endif
    #if defined(C89ATOMIC_HAS_64)
        static C89ATOMIC_INLINE c89atomic_uint64 __stdcall c89atomic_fetch_xor_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedXor64, c89atomic_uint64, long long);
        #else
            c89atomic_uint64 oldValue;
            c89atomic_uint64 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue ^ src;
            } while (c89atomic_compare_and_swap_64(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        #endif
        }
    #endif


    /* fetch_or() */
    #if defined(C89ATOMIC_HAS_8)
        static C89ATOMIC_INLINE c89atomic_uint8 __stdcall c89atomic_fetch_or_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedOr8, c89atomic_uint8, char);
        #else
            c89atomic_uint8 oldValue;
            c89atomic_uint8 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint8)(oldValue | src);
            } while (c89atomic_compare_and_swap_8(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        #endif
        }
    #endif
    #if defined(C89ATOMIC_HAS_16)
        static C89ATOMIC_INLINE c89atomic_uint16 __stdcall c89atomic_fetch_or_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedOr16, c89atomic_uint16, short);
        #else
            c89atomic_uint16 oldValue;
            c89atomic_uint16 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint16)(oldValue | src);
            } while (c89atomic_compare_and_swap_16(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        #endif
        }
    #endif
    #if defined(C89ATOMIC_HAS_32)
        static C89ATOMIC_INLINE c89atomic_uint32 __stdcall c89atomic_fetch_or_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedOr, c89atomic_uint32, long);
        #else
            c89atomic_uint32 oldValue;
            c89atomic_uint32 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue | src;
            } while (c89atomic_compare_and_swap_32(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        #endif
        }
    #endif
    #if defined(C89ATOMIC_HAS_64)
        static C89ATOMIC_INLINE c89atomic_uint64 __stdcall c89atomic_fetch_or_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_ARM)
            C89ATOMIC_MSVC_ARM_INTRINSIC(dst, src, order, _InterlockedOr64, c89atomic_uint64, long long);
        #else
            c89atomic_uint64 oldValue;
            c89atomic_uint64 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue | src;
            } while (c89atomic_compare_and_swap_64(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        #endif
        }
    #endif

    /* test_and_set() */
    #if defined(C89ATOMIC_HAS_8)
        #define c89atomic_test_and_set_explicit_8( dst, order) c89atomic_exchange_explicit_8 (dst, 1, order)
    #endif
    #if defined(C89ATOMIC_HAS_16)
        #define c89atomic_test_and_set_explicit_16(dst, order) c89atomic_exchange_explicit_16(dst, 1, order)
    #endif
    #if defined(C89ATOMIC_HAS_32)
        #define c89atomic_test_and_set_explicit_32(dst, order) c89atomic_exchange_explicit_32(dst, 1, order)
    #endif
    #if defined(C89ATOMIC_HAS_64)
        #define c89atomic_test_and_set_explicit_64(dst, order) c89atomic_exchange_explicit_64(dst, 1, order)
    #endif

    /* clear() */
    #if defined(C89ATOMIC_HAS_8)
        #define c89atomic_clear_explicit_8( dst, order) c89atomic_store_explicit_8 (dst, 0, order)
    #endif
    #if defined(C89ATOMIC_HAS_16)
        #define c89atomic_clear_explicit_16(dst, order) c89atomic_store_explicit_16(dst, 0, order)
    #endif
    #if defined(C89ATOMIC_HAS_32)
        #define c89atomic_clear_explicit_32(dst, order) c89atomic_store_explicit_32(dst, 0, order)
    #endif
    #if defined(C89ATOMIC_HAS_64)
        #define c89atomic_clear_explicit_64(dst, order) c89atomic_store_explicit_64(dst, 0, order)
    #endif

    /* Prefer 8-bit flags, but fall back to 32-bit if we don't support 8-bit atomics (possible on 64-bit MSVC prior to Visual Studio 2010). */
    #if defined(C89ATOMIC_HAS_8)
        typedef c89atomic_uint8 c89atomic_flag;
        #define c89atomic_flag_test_and_set_explicit(ptr, order)    (c89atomic_bool)c89atomic_test_and_set_explicit_8(ptr, order)
        #define c89atomic_flag_clear_explicit(ptr, order)           c89atomic_clear_explicit_8(ptr, order)
        #define c89atomic_flag_load_explicit(ptr, order)            c89atomic_load_explicit_8(ptr, order)
    #else
        typedef c89atomic_uint32 c89atomic_flag;
        #define c89atomic_flag_test_and_set_explicit(ptr, order)    (c89atomic_bool)c89atomic_test_and_set_explicit_32(ptr, order)
        #define c89atomic_flag_clear_explicit(ptr, order)           c89atomic_clear_explicit_32(ptr, order)
        #define c89atomic_flag_load_explicit(ptr, order)            c89atomic_load_explicit_32(ptr, order)
    #endif
#elif defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)))
    /* Modern GCC atomic built-ins. */
    #define C89ATOMIC_HAS_NATIVE_COMPARE_EXCHANGE
    #define C89ATOMIC_HAS_NATIVE_IS_LOCK_FREE

    #define c89atomic_memory_order_relaxed                          __ATOMIC_RELAXED
    #define c89atomic_memory_order_consume                          __ATOMIC_CONSUME
    #define c89atomic_memory_order_acquire                          __ATOMIC_ACQUIRE
    #define c89atomic_memory_order_release                          __ATOMIC_RELEASE
    #define c89atomic_memory_order_acq_rel                          __ATOMIC_ACQ_REL
    #define c89atomic_memory_order_seq_cst                          __ATOMIC_SEQ_CST

    #define c89atomic_compiler_fence()                              __asm__ __volatile__("":::"memory")
    #define c89atomic_thread_fence(order)                           __atomic_thread_fence(order)
    #define c89atomic_signal_fence(order)                           __atomic_signal_fence(order)

    #define c89atomic_is_lock_free_8(ptr)                           __atomic_is_lock_free(1, ptr)
    #define c89atomic_is_lock_free_16(ptr)                          __atomic_is_lock_free(2, ptr)
    #define c89atomic_is_lock_free_32(ptr)                          __atomic_is_lock_free(4, ptr)
    #define c89atomic_is_lock_free_64(ptr)                          __atomic_is_lock_free(8, ptr)

    #define c89atomic_test_and_set_explicit_8( dst, order)          __atomic_exchange_n(dst, 1, order)
    #define c89atomic_test_and_set_explicit_16(dst, order)          __atomic_exchange_n(dst, 1, order)
    #define c89atomic_test_and_set_explicit_32(dst, order)          __atomic_exchange_n(dst, 1, order)
    #define c89atomic_test_and_set_explicit_64(dst, order)          __atomic_exchange_n(dst, 1, order)

    #define c89atomic_clear_explicit_8( dst, order)                 __atomic_store_n(dst, 0, order)
    #define c89atomic_clear_explicit_16(dst, order)                 __atomic_store_n(dst, 0, order)
    #define c89atomic_clear_explicit_32(dst, order)                 __atomic_store_n(dst, 0, order)
    #define c89atomic_clear_explicit_64(dst, order)                 __atomic_store_n(dst, 0, order)

    #define c89atomic_store_explicit_8( dst, src, order)            __atomic_store_n(dst, src, order)
    #define c89atomic_store_explicit_16(dst, src, order)            __atomic_store_n(dst, src, order)
    #define c89atomic_store_explicit_32(dst, src, order)            __atomic_store_n(dst, src, order)
    #define c89atomic_store_explicit_64(dst, src, order)            __atomic_store_n(dst, src, order)

    #define c89atomic_load_explicit_8( dst, order)                  __atomic_load_n(dst, order)
    #define c89atomic_load_explicit_16(dst, order)                  __atomic_load_n(dst, order)
    #define c89atomic_load_explicit_32(dst, order)                  __atomic_load_n(dst, order)
    #define c89atomic_load_explicit_64(dst, order)                  __atomic_load_n(dst, order)

    #define c89atomic_exchange_explicit_8( dst, src, order)         __atomic_exchange_n(dst, src, order)
    #define c89atomic_exchange_explicit_16(dst, src, order)         __atomic_exchange_n(dst, src, order)
    #define c89atomic_exchange_explicit_32(dst, src, order)         __atomic_exchange_n(dst, src, order)
    #define c89atomic_exchange_explicit_64(dst, src, order)         __atomic_exchange_n(dst, src, order)

    #define c89atomic_compare_exchange_strong_explicit_8( dst, expected, desired, successOrder, failureOrder)   __atomic_compare_exchange_n(dst, expected, desired, 0, successOrder, failureOrder)
    #define c89atomic_compare_exchange_strong_explicit_16(dst, expected, desired, successOrder, failureOrder)   __atomic_compare_exchange_n(dst, expected, desired, 0, successOrder, failureOrder)
    #define c89atomic_compare_exchange_strong_explicit_32(dst, expected, desired, successOrder, failureOrder)   __atomic_compare_exchange_n(dst, expected, desired, 0, successOrder, failureOrder)
    #define c89atomic_compare_exchange_strong_explicit_64(dst, expected, desired, successOrder, failureOrder)   __atomic_compare_exchange_n(dst, expected, desired, 0, successOrder, failureOrder)

    #define c89atomic_compare_exchange_weak_explicit_8( dst, expected, desired, successOrder, failureOrder)     __atomic_compare_exchange_n(dst, expected, desired, 1, successOrder, failureOrder)
    #define c89atomic_compare_exchange_weak_explicit_16(dst, expected, desired, successOrder, failureOrder)     __atomic_compare_exchange_n(dst, expected, desired, 1, successOrder, failureOrder)
    #define c89atomic_compare_exchange_weak_explicit_32(dst, expected, desired, successOrder, failureOrder)     __atomic_compare_exchange_n(dst, expected, desired, 1, successOrder, failureOrder)
    #define c89atomic_compare_exchange_weak_explicit_64(dst, expected, desired, successOrder, failureOrder)     __atomic_compare_exchange_n(dst, expected, desired, 1, successOrder, failureOrder)

    #define c89atomic_fetch_add_explicit_8( dst, src, order)        __atomic_fetch_add(dst, src, order)
    #define c89atomic_fetch_add_explicit_16(dst, src, order)        __atomic_fetch_add(dst, src, order)
    #define c89atomic_fetch_add_explicit_32(dst, src, order)        __atomic_fetch_add(dst, src, order)
    #define c89atomic_fetch_add_explicit_64(dst, src, order)        __atomic_fetch_add(dst, src, order)

    #define c89atomic_fetch_sub_explicit_8( dst, src, order)        __atomic_fetch_sub(dst, src, order)
    #define c89atomic_fetch_sub_explicit_16(dst, src, order)        __atomic_fetch_sub(dst, src, order)
    #define c89atomic_fetch_sub_explicit_32(dst, src, order)        __atomic_fetch_sub(dst, src, order)
    #define c89atomic_fetch_sub_explicit_64(dst, src, order)        __atomic_fetch_sub(dst, src, order)

    #define c89atomic_fetch_or_explicit_8( dst, src, order)         __atomic_fetch_or(dst, src, order)
    #define c89atomic_fetch_or_explicit_16(dst, src, order)         __atomic_fetch_or(dst, src, order)
    #define c89atomic_fetch_or_explicit_32(dst, src, order)         __atomic_fetch_or(dst, src, order)
    #define c89atomic_fetch_or_explicit_64(dst, src, order)         __atomic_fetch_or(dst, src, order)

    #define c89atomic_fetch_xor_explicit_8( dst, src, order)        __atomic_fetch_xor(dst, src, order)
    #define c89atomic_fetch_xor_explicit_16(dst, src, order)        __atomic_fetch_xor(dst, src, order)
    #define c89atomic_fetch_xor_explicit_32(dst, src, order)        __atomic_fetch_xor(dst, src, order)
    #define c89atomic_fetch_xor_explicit_64(dst, src, order)        __atomic_fetch_xor(dst, src, order)

    #define c89atomic_fetch_and_explicit_8( dst, src, order)        __atomic_fetch_and(dst, src, order)
    #define c89atomic_fetch_and_explicit_16(dst, src, order)        __atomic_fetch_and(dst, src, order)
    #define c89atomic_fetch_and_explicit_32(dst, src, order)        __atomic_fetch_and(dst, src, order)
    #define c89atomic_fetch_and_explicit_64(dst, src, order)        __atomic_fetch_and(dst, src, order)

    /* CAS needs to be implemented as a function because _atomic_compare_exchange_n() needs to take the address of the expected value. */
    static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_compare_and_swap_8(volatile c89atomic_uint8* dst, c89atomic_uint8 expected, c89atomic_uint8 desired)
    {
        __atomic_compare_exchange_n(dst, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        return expected;
    }

    static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_compare_and_swap_16(volatile c89atomic_uint16* dst, c89atomic_uint16 expected, c89atomic_uint16 desired)
    {
        __atomic_compare_exchange_n(dst, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        return expected;
    }

    static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_compare_and_swap_32(volatile c89atomic_uint32* dst, c89atomic_uint32 expected, c89atomic_uint32 desired)
    {
        __atomic_compare_exchange_n(dst, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        return expected;
    }

    /*
    The compare_and_swap function below will result in the following warning when compiling via Android Studio
    which uses Clang:

        warning: misaligned atomic operation may incur significant performance penalty [-Watomic-alignment]

    What I think is happening is that Clang is looking at this function in a bubble and not considering the
    broader context in which the function is being used, which is surprising to me considering the function is
    marked as force-inlined. So I think this warning is being reported incorrectly.

    I've only seen this warning with compare_and_swap_64(). If this is happening with compare_and_swap_32/16/8(),
    just move the pragmas up to encapsulate the affected functions.
    */
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Watomic-alignment"
    static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_compare_and_swap_64(volatile c89atomic_uint64* dst, c89atomic_uint64 expected, c89atomic_uint64 desired)
    {
        __atomic_compare_exchange_n(dst, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        return expected;
    }
    #pragma clang diagnostic pop

    typedef c89atomic_uint8 c89atomic_flag;
    #define c89atomic_flag_test_and_set_explicit(dst, order)        (c89atomic_bool)__atomic_test_and_set(dst, order)
    #define c89atomic_flag_clear_explicit(dst, order)               __atomic_clear(dst, order)
    #define c89atomic_flag_load_explicit(ptr, order)                c89atomic_load_explicit_8(ptr, order)
#else
    /* GCC and compilers supporting GCC-style inlined assembly. */
    #define c89atomic_memory_order_relaxed  1
    #define c89atomic_memory_order_consume  2
    #define c89atomic_memory_order_acquire  3
    #define c89atomic_memory_order_release  4
    #define c89atomic_memory_order_acq_rel  5
    #define c89atomic_memory_order_seq_cst  6

    #define c89atomic_compiler_fence() __asm__ __volatile__("":::"memory")

    #if defined(__GNUC__)
        /* Legacy GCC atomic built-ins. Everything is a full memory barrier. */
        #define c89atomic_thread_fence(order) __sync_synchronize(), (void)order

        /* exchange() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_exchange_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            if (order > c89atomic_memory_order_acquire) {
                __sync_synchronize();
            }

            return __sync_lock_test_and_set(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_exchange_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            c89atomic_uint16 oldValue;

            do {
                oldValue = *dst;
            } while (__sync_val_compare_and_swap(dst, oldValue, src) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_exchange_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            c89atomic_uint32 oldValue;

            do {
                oldValue = *dst;
            } while (__sync_val_compare_and_swap(dst, oldValue, src) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_exchange_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            c89atomic_uint64 oldValue;

            do {
                oldValue = *dst;
            } while (__sync_val_compare_and_swap(dst, oldValue, src) != oldValue);

            (void)order;
            return oldValue;
        }


        /* fetch_add() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_fetch_add_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_add(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_fetch_add_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_add(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_fetch_add_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_add(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_fetch_add_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_add(dst, src);
        }


        /* fetch_sub() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_fetch_sub_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_sub(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_fetch_sub_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_sub(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_fetch_sub_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_sub(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_fetch_sub_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_sub(dst, src);
        }


        /* fetch_or() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_fetch_or_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_or(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_fetch_or_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_or(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_fetch_or_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_or(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_fetch_or_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_or(dst, src);
        }


        /* fetch_xor() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_fetch_xor_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_xor(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_fetch_xor_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_xor(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_fetch_xor_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_xor(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_fetch_xor_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_xor(dst, src);
        }

        /* fetch_and() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_fetch_and_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_and(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_fetch_and_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_and(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_fetch_and_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_and(dst, src);
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_fetch_and_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            (void)order;
            return __sync_fetch_and_and(dst, src);
        }

        #define c89atomic_compare_and_swap_8( dst, expected, desired)   __sync_val_compare_and_swap(dst, expected, desired)
        #define c89atomic_compare_and_swap_16(dst, expected, desired)   __sync_val_compare_and_swap(dst, expected, desired)
        #define c89atomic_compare_and_swap_32(dst, expected, desired)   __sync_val_compare_and_swap(dst, expected, desired)
        #define c89atomic_compare_and_swap_64(dst, expected, desired)   __sync_val_compare_and_swap(dst, expected, desired)
    #else
        /* Non-GCC compilers supporting GCC-style inlined assembly. The inlined assembly below uses Gas syntax. */

        /*
        It's actually kind of confusing as to the best way to implement a memory barrier on x86/64. From my quick research, it looks like
        there's a few options:
            - SFENCE/LFENCE/MFENCE
            - LOCK ADD
            - XCHG (with a memory operand, not two register operands)

        It looks like the SFENCE instruction was added in the Pentium III series, whereas the LFENCE and MFENCE instructions were added in
        the Pentium 4 series. It's not clear how this actually differs to a LOCK-prefixed instruction or an XCHG instruction with a memory
        operand. For simplicity and compatibility, I'm just using a LOCK-prefixed ADD instruction which adds 0 to the value pointed to by
        the ESP register. The use of the ESP register is that it should theoretically have a high likelyhood to be in cache. For now, just
        to keep things simple, this is always doing a full memory barrier which means the `order` parameter is ignored on x86/64.

        I want a thread fence to also act as a compiler fence, so therefore I'm forcing this to be inlined by implementing it as a define.
        */
        #if defined(C89ATOMIC_X86)
            #define c89atomic_thread_fence(order) __asm__ __volatile__("lock; addl $0, (%%esp)" ::: "memory", "cc")
        #elif defined(C89ATOMIC_X64)
            #define c89atomic_thread_fence(order) __asm__ __volatile__("lock; addq $0, (%%rsp)" ::: "memory", "cc")
        #else
            #error Unsupported architecture. Please submit a feature request.
        #endif

        /* compare_and_swap() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_compare_and_swap_8(volatile c89atomic_uint8* dst, c89atomic_uint8 expected, c89atomic_uint8 desired)
        {
            c89atomic_uint8 result;

        #if defined(C89ATOMIC_X86) || defined(C89ATOMIC_X64)
            #if defined(__TINYC__) && defined(_WIN32) && defined(__arm__) && !defined(_MSC_VER)
                return (c89atomic_uint8)atomic_compare_exchange_strong((atomic_uchar *)dst, (unsigned char*)&expected, (unsigned char)desired);
            #else
                __asm__ __volatile__("lock; cmpxchg %3, %0" : "+m"(*dst), "=a"(result) : "a"(expected), "d"(desired) : "cc");
            #endif
        #else
            #error Unsupported architecture. Please submit a feature request.
        #endif

            return result;
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_compare_and_swap_16(volatile c89atomic_uint16* dst, c89atomic_uint16 expected, c89atomic_uint16 desired)
        {
            c89atomic_uint16 result;

        #if defined(C89ATOMIC_X86) || defined(C89ATOMIC_X64)
            #if defined(__TINYC__) && defined(_WIN32) && defined(__arm__) && !defined(_MSC_VER)
                return (c89atomic_uint16)atomic_compare_exchange_strong((atomic_ushort *)dst, (unsigned short*)&expected, (unsigned short)desired);
            #else
                __asm__ __volatile__("lock; cmpxchg %3, %0" : "+m"(*dst), "=a"(result) : "a"(expected), "d"(desired) : "cc");
            #endif
        #else
            #error Unsupported architecture. Please submit a feature request.
        #endif

            return result;
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_compare_and_swap_32(volatile c89atomic_uint32* dst, c89atomic_uint32 expected, c89atomic_uint32 desired)
        {
            c89atomic_uint32 result;

        #if defined(C89ATOMIC_X86) || defined(C89ATOMIC_X64)
            #if defined(__TINYC__) && defined(_WIN32) && defined(__arm__) && !defined(_MSC_VER)
                return (c89atomic_uint32)atomic_compare_exchange_strong((atomic_int *)dst, (unsigned int*)&expected, (unsigned int)desired);
            #else
                __asm__ __volatile__("lock; cmpxchg %3, %0" : "+m"(*dst), "=a"(result) : "a"(expected), "d"(desired) : "cc");
            #endif
        #else
            #error Unsupported architecture. Please submit a feature request.
        #endif

            return result;
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_compare_and_swap_64(volatile c89atomic_uint64* dst, c89atomic_uint64 expected, c89atomic_uint64 desired)
        {
            volatile c89atomic_uint64 result;

        #if defined(C89ATOMIC_X86)
            /*
            We can't use the standard CMPXCHG here because x86 does not support it with 64-bit values. We need to instead use CMPXCHG8B
            which is a bit harder to use. The annoying part with this is the use of the -fPIC compiler switch which requires the EBX
            register never be modified. The problem is that CMPXCHG8B requires us to write our desired value to it. I'm resolving this
            by just pushing and popping the EBX register manually.
            */
            c89atomic_uint32 resultEAX;
            c89atomic_uint32 resultEDX;
            #if defined(__TINYC__) && defined(_WIN32) && defined(__arm__) && !defined(_MSC_VER)
                return (c89atomic_uint64)atomic_compare_exchange_strong((atomic_ulong *)dst, (unsigned long*)&expected, (unsigned long)desired);
            #else
                __asm__ __volatile__("push %%ebx; xchg %5, %%ebx; lock; cmpxchg8b %0; pop %%ebx" : "+m"(*dst), "=a"(resultEAX), "=d"(resultEDX) : "a"(expected & 0xFFFFFFFF), "d"(expected >> 32), "r"(desired & 0xFFFFFFFF), "c"(desired >> 32) : "cc");
                result = ((c89atomic_uint64)resultEDX << 32) | resultEAX;
            #endif
        #elif defined(C89ATOMIC_X64)
            __asm__ __volatile__("lock; cmpxchg %3, %0" : "+m"(*dst), "=a"(result) : "a"(expected), "d"(desired) : "cc");
        #else
            #error Unsupported architecture. Please submit a feature request.
        #endif

            return result;
        }


        /* exchange() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_exchange_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            c89atomic_uint8 result = 0;

            (void)order;

        #if defined(C89ATOMIC_X86) || defined(C89ATOMIC_X64)
            #if defined(__TINYC__) && defined(_WIN32) && defined(__arm__) && !defined(_MSC_VER)
                return (c89atomic_uint8)atomic_exchange_explicit((atomic_uchar *)dst, (unsigned char)src, order);
            #else
                __asm__ __volatile__("lock; xchg %1, %0" : "+m"(*dst), "=a"(result) : "a"(src));
            #endif
        #else
            #error Unsupported architecture. Please submit a feature request.
        #endif

            return result;
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_exchange_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            c89atomic_uint16 result = 0;

            (void)order;

        #if defined(C89ATOMIC_X86) || defined(C89ATOMIC_X64)
            #if defined(__TINYC__) && defined(_WIN32) && defined(__arm__) && !defined(_MSC_VER)
                result = (c89atomic_uint16)atomic_exchange_explicit((atomic_ushort *)dst, (unsigned short)src, order);
            #else
                __asm__ __volatile__("lock; xchg %1, %0" : "+m"(*dst), "=a"(result) : "a"(src));
            #endif
        #else
            #error Unsupported architecture. Please submit a feature request.
        #endif

            return result;
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_exchange_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            c89atomic_uint32 result;

            (void)order;

        #if defined(C89ATOMIC_X86) || defined(C89ATOMIC_X64)
            #if defined(__TINYC__) && defined(_WIN32) && defined(__arm__) && !defined(_MSC_VER)
                result = (c89atomic_uint32)atomic_exchange_explicit((atomic_uint *)dst, (unsigned int)src, order);
            #else
                __asm__ __volatile__("lock; xchg %1, %0" : "+m"(*dst), "=a"(result) : "a"(src));
            #endif
        #else
            #error Unsupported architecture. Please submit a feature request.
        #endif

            return result;
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_exchange_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            c89atomic_uint64 result;

            (void)order;

        #if defined(C89ATOMIC_X86)
            do {
                result = *dst;
            } while (c89atomic_compare_and_swap_64(dst, result, src) != result);
        #elif defined(C89ATOMIC_X64)
            #if defined(__TINYC__) && defined(_WIN32) && defined(__arm__) && !defined(_MSC_VER)
                result = (c89atomic_uint64)atomic_exchange_explicit((atomic_ulong *)dst, (unsigned long)src, order);
            #else
                __asm__ __volatile__("lock; xchg %1, %0" : "+m"(*dst), "=a"(result) : "a"(src));
            #endif
        #else
            #error Unsupported architecture. Please submit a feature request.
        #endif

            return result;
        }


        /* fetch_add() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_fetch_add_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            c89atomic_uint8 result;

            (void)order;

        #if defined(C89ATOMIC_X86) || defined(C89ATOMIC_X64)
            #if defined(__TINYC__) && defined(_WIN32) && defined(__arm__) && !defined(_MSC_VER)
                result = (c89atomic_uint8)atomic_fetch_add_explicit((atomic_uchar *)dst, (unsigned char)src, order);
            #else
                __asm__ __volatile__("lock; xadd %1, %0" : "+m"(*dst), "=a"(result) : "a"(src) : "cc");
            #endif
        #else
            #error Unsupported architecture. Please submit a feature request.
        #endif

            return result;
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_fetch_add_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            c89atomic_uint16 result;

            (void)order;

        #if defined(C89ATOMIC_X86) || defined(C89ATOMIC_X64)
            #if defined(__TINYC__) && defined(_WIN32) && defined(__arm__) && !defined(_MSC_VER)
                result = (c89atomic_uint16)atomic_fetch_add_explicit((atomic_ushort *)dst, (unsigned short)src, order);
            #else
                __asm__ __volatile__("lock; xadd %1, %0" : "+m"(*dst), "=a"(result) : "a"(src) : "cc");
            #endif
        #else
            #error Unsupported architecture. Please submit a feature request.
        #endif

            return result;
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_fetch_add_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            c89atomic_uint32 result;

            (void)order;

        #if defined(C89ATOMIC_X86) || defined(C89ATOMIC_X64)
            #if defined(__TINYC__) && defined(_WIN32) && defined(__arm__) && !defined(_MSC_VER)
                result = (c89atomic_uint32)atomic_fetch_add_explicit((atomic_uint *)dst, (unsigned int)src, order);
            #else
                __asm__ __volatile__("lock; xadd %1, %0" : "+m"(*dst), "=a"(result) : "a"(src) : "cc");
            #endif
        #else
            #error Unsupported architecture. Please submit a feature request.
        #endif

            return result;
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_fetch_add_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
        #if defined(C89ATOMIC_X86)
            c89atomic_uint64 oldValue;
            c89atomic_uint64 newValue;

            (void)order;

            do {
                oldValue = *dst;
                newValue = oldValue + src;
            } while (c89atomic_compare_and_swap_64(dst, oldValue, newValue) != oldValue);

            return oldValue;
        #elif defined(C89ATOMIC_X64)
            c89atomic_uint64 result;

            (void)order;

            #if defined(__TINYC__) && defined(_WIN32) && defined(__arm__) && !defined(_MSC_VER)
                result = (c89atomic_uint64)atomic_fetch_add_explicit((atomic_ulong *)dst, (unsigned long)src, order);
            #else
                __asm__ __volatile__("lock; xadd %1, %0" : "+m"(*dst), "=a"(result) : "a"(src) : "cc");
            #endif

            return result;
        #endif
        }

        /* fetch_sub() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_fetch_sub_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            c89atomic_uint8 oldValue;
            c89atomic_uint8 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint8)(oldValue - src);
            } while (c89atomic_compare_and_swap_8(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_fetch_sub_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            c89atomic_uint16 oldValue;
            c89atomic_uint16 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint16)(oldValue - src);
            } while (c89atomic_compare_and_swap_16(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_fetch_sub_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            c89atomic_uint32 oldValue;
            c89atomic_uint32 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue - src;
            } while (c89atomic_compare_and_swap_32(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_fetch_sub_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            c89atomic_uint64 oldValue;
            c89atomic_uint64 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue - src;
            } while (c89atomic_compare_and_swap_64(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }


        /* fetch_and() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_fetch_and_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            c89atomic_uint8 oldValue;
            c89atomic_uint8 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint8)(oldValue & src);
            } while (c89atomic_compare_and_swap_8(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_fetch_and_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            c89atomic_uint16 oldValue;
            c89atomic_uint16 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint16)(oldValue & src);
            } while (c89atomic_compare_and_swap_16(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_fetch_and_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            c89atomic_uint32 oldValue;
            c89atomic_uint32 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue & src;
            } while (c89atomic_compare_and_swap_32(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_fetch_and_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            c89atomic_uint64 oldValue;
            c89atomic_uint64 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue & src;
            } while (c89atomic_compare_and_swap_64(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }


        /* fetch_xor() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_fetch_xor_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            c89atomic_uint8 oldValue;
            c89atomic_uint8 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint8)(oldValue ^ src);
            } while (c89atomic_compare_and_swap_8(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_fetch_xor_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            c89atomic_uint16 oldValue;
            c89atomic_uint16 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint16)(oldValue ^ src);
            } while (c89atomic_compare_and_swap_16(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_fetch_xor_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            c89atomic_uint32 oldValue;
            c89atomic_uint32 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue ^ src;
            } while (c89atomic_compare_and_swap_32(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_fetch_xor_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            c89atomic_uint64 oldValue;
            c89atomic_uint64 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue ^ src;
            } while (c89atomic_compare_and_swap_64(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }


        /* fetch_or() */
        static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_fetch_or_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8 src, c89atomic_memory_order order)
        {
            c89atomic_uint8 oldValue;
            c89atomic_uint8 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint8)(oldValue | src);
            } while (c89atomic_compare_and_swap_8(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_fetch_or_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16 src, c89atomic_memory_order order)
        {
            c89atomic_uint16 oldValue;
            c89atomic_uint16 newValue;

            do {
                oldValue = *dst;
                newValue = (c89atomic_uint16)(oldValue | src);
            } while (c89atomic_compare_and_swap_16(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_fetch_or_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32 src, c89atomic_memory_order order)
        {
            c89atomic_uint32 oldValue;
            c89atomic_uint32 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue | src;
            } while (c89atomic_compare_and_swap_32(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }

        static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_fetch_or_explicit_64(volatile c89atomic_uint64* dst, c89atomic_uint64 src, c89atomic_memory_order order)
        {
            c89atomic_uint64 oldValue;
            c89atomic_uint64 newValue;

            do {
                oldValue = *dst;
                newValue = oldValue | src;
            } while (c89atomic_compare_and_swap_64(dst, oldValue, newValue) != oldValue);

            (void)order;
            return oldValue;
        }
    #endif

    #define c89atomic_signal_fence(order)                           c89atomic_thread_fence(order)

    /* Atomic loads can be implemented in terms of a compare-and-swap. Need to implement as functions to silence warnings about `order` being unused. */
    static C89ATOMIC_INLINE c89atomic_uint8 c89atomic_load_explicit_8(volatile const c89atomic_uint8* ptr, c89atomic_memory_order order)
    {
        (void)order;    /* Always using the strongest memory order. */
        return c89atomic_compare_and_swap_8((c89atomic_uint8*)ptr, 0, 0);
    }

    static C89ATOMIC_INLINE c89atomic_uint16 c89atomic_load_explicit_16(volatile const c89atomic_uint16* ptr, c89atomic_memory_order order)
    {
        (void)order;    /* Always using the strongest memory order. */
        return c89atomic_compare_and_swap_16((c89atomic_uint16*)ptr, 0, 0);
    }

    static C89ATOMIC_INLINE c89atomic_uint32 c89atomic_load_explicit_32(volatile const c89atomic_uint32* ptr, c89atomic_memory_order order)
    {
        (void)order;    /* Always using the strongest memory order. */
        return c89atomic_compare_and_swap_32((c89atomic_uint32*)ptr, 0, 0);
    }

    static C89ATOMIC_INLINE c89atomic_uint64 c89atomic_load_explicit_64(volatile const c89atomic_uint64* ptr, c89atomic_memory_order order)
    {
        (void)order;    /* Always using the strongest memory order. */
        return c89atomic_compare_and_swap_64((c89atomic_uint64*)ptr, 0, 0);
    }

    #define c89atomic_store_explicit_8( dst, src, order)            (void)c89atomic_exchange_explicit_8 (dst, src, order)
    #define c89atomic_store_explicit_16(dst, src, order)            (void)c89atomic_exchange_explicit_16(dst, src, order)
    #define c89atomic_store_explicit_32(dst, src, order)            (void)c89atomic_exchange_explicit_32(dst, src, order)
    #define c89atomic_store_explicit_64(dst, src, order)            (void)c89atomic_exchange_explicit_64(dst, src, order)

    #define c89atomic_test_and_set_explicit_8( dst, order)          c89atomic_exchange_explicit_8 (dst, 1, order)
    #define c89atomic_test_and_set_explicit_16(dst, order)          c89atomic_exchange_explicit_16(dst, 1, order)
    #define c89atomic_test_and_set_explicit_32(dst, order)          c89atomic_exchange_explicit_32(dst, 1, order)
    #define c89atomic_test_and_set_explicit_64(dst, order)          c89atomic_exchange_explicit_64(dst, 1, order)

    #define c89atomic_clear_explicit_8( dst, order)                 c89atomic_store_explicit_8 (dst, 0, order)
    #define c89atomic_clear_explicit_16(dst, order)                 c89atomic_store_explicit_16(dst, 0, order)
    #define c89atomic_clear_explicit_32(dst, order)                 c89atomic_store_explicit_32(dst, 0, order)
    #define c89atomic_clear_explicit_64(dst, order)                 c89atomic_store_explicit_64(dst, 0, order)

    typedef c89atomic_uint8 c89atomic_flag;
    #define c89atomic_flag_test_and_set_explicit(ptr, order)        (c89atomic_bool)c89atomic_test_and_set_explicit_8(ptr, order)
    #define c89atomic_flag_clear_explicit(ptr, order)               c89atomic_clear_explicit_8(ptr, order)
    #define c89atomic_flag_load_explicit(ptr, order)                c89atomic_load_explicit_8(ptr, order)
#endif

/* compare_exchange() */
#if !defined(C89ATOMIC_HAS_NATIVE_COMPARE_EXCHANGE)
    #if defined(C89ATOMIC_HAS_8)
        static C89ATOMIC_INLINE c89atomic_bool c89atomic_compare_exchange_strong_explicit_8(volatile c89atomic_uint8* dst, c89atomic_uint8* expected, c89atomic_uint8 desired, c89atomic_memory_order successOrder, c89atomic_memory_order failureOrder)
        {
            c89atomic_uint8 expectedValue;
            c89atomic_uint8 result;

            (void)successOrder;
            (void)failureOrder;

            expectedValue = c89atomic_load_explicit_8(expected, c89atomic_memory_order_seq_cst);
            result = c89atomic_compare_and_swap_8(dst, expectedValue, desired);
            if (result == expectedValue) {
                return 1;
            } else {
                c89atomic_store_explicit_8(expected, result, failureOrder);
                return 0;
            }
        }
    #endif
    #if defined(C89ATOMIC_HAS_16)
        static C89ATOMIC_INLINE c89atomic_bool c89atomic_compare_exchange_strong_explicit_16(volatile c89atomic_uint16* dst, c89atomic_uint16* expected, c89atomic_uint16 desired, c89atomic_memory_order successOrder, c89atomic_memory_order failureOrder)
        {
            c89atomic_uint16 expectedValue;
            c89atomic_uint16 result;

            (void)successOrder;
            (void)failureOrder;

            expectedValue = c89atomic_load_explicit_16(expected, c89atomic_memory_order_seq_cst);
            result = c89atomic_compare_and_swap_16(dst, expectedValue, desired);
            if (result == expectedValue) {
                return 1;
            } else {
                c89atomic_store_explicit_16(expected, result, failureOrder);
                return 0;
            }
        }
    #endif
    #if defined(C89ATOMIC_HAS_32)
        static C89ATOMIC_INLINE c89atomic_bool c89atomic_compare_exchange_strong_explicit_32(volatile c89atomic_uint32* dst, c89atomic_uint32* expected, c89atomic_uint32 desired, c89atomic_memory_order successOrder, c89atomic_memory_order failureOrder)
        {
            c89atomic_uint32 expectedValue;
            c89atomic_uint32 result;

            (void)successOrder;
            (void)failureOrder;

            expectedValue = c89atomic_load_explicit_32(expected, c89atomic_memory_order_seq_cst);
            result = c89atomic_compare_and_swap_32(dst, expectedValue, desired);
            if (result == expectedValue) {
                return 1;
            } else {
                c89atomic_store_explicit_32(expected, result, failureOrder);
                return 0;
            }
        }
    #endif
    #if defined(C89ATOMIC_HAS_64)
        static C89ATOMIC_INLINE c89atomic_bool c89atomic_compare_exchange_strong_explicit_64(volatile c89atomic_uint64* dst, volatile c89atomic_uint64* expected, c89atomic_uint64 desired, c89atomic_memory_order successOrder, c89atomic_memory_order failureOrder)
        {
            c89atomic_uint64 expectedValue;
            c89atomic_uint64 result;

            (void)successOrder;
            (void)failureOrder;

            expectedValue = c89atomic_load_explicit_64(expected, c89atomic_memory_order_seq_cst);
            result = c89atomic_compare_and_swap_64(dst, expectedValue, desired);
            if (result == expectedValue) {
                return 1;
            } else {
                c89atomic_store_explicit_64(expected, result, failureOrder);
                return 0;
            }
        }
    #endif

    #define c89atomic_compare_exchange_weak_explicit_8( dst, expected, desired, successOrder, failureOrder) c89atomic_compare_exchange_strong_explicit_8 (dst, expected, desired, successOrder, failureOrder)
    #define c89atomic_compare_exchange_weak_explicit_16(dst, expected, desired, successOrder, failureOrder) c89atomic_compare_exchange_strong_explicit_16(dst, expected, desired, successOrder, failureOrder)
    #define c89atomic_compare_exchange_weak_explicit_32(dst, expected, desired, successOrder, failureOrder) c89atomic_compare_exchange_strong_explicit_32(dst, expected, desired, successOrder, failureOrder)
    #define c89atomic_compare_exchange_weak_explicit_64(dst, expected, desired, successOrder, failureOrder) c89atomic_compare_exchange_strong_explicit_64(dst, expected, desired, successOrder, failureOrder)
#endif  /* C89ATOMIC_HAS_NATIVE_COMPARE_EXCHANGE */

#if !defined(C89ATOMIC_HAS_NATIVE_IS_LOCK_FREE)
    static C89ATOMIC_INLINE c89atomic_bool c89atomic_is_lock_free_8(volatile void* ptr)
    {
        return c89atomic_is_lock_free(ptr);
    }

    static C89ATOMIC_INLINE c89atomic_bool c89atomic_is_lock_free_16(volatile void* ptr)
    {
        return c89atomic_is_lock_free(ptr);
    }

    static C89ATOMIC_INLINE c89atomic_bool c89atomic_is_lock_free_32(volatile void* ptr)
    {
        return c89atomic_is_lock_free(ptr);
    }

    static C89ATOMIC_INLINE c89atomic_bool c89atomic_is_lock_free_64(volatile void* ptr)
    {
        return c89atomic_is_lock_free(ptr);
    }
#endif  /* C89ATOMIC_HAS_NATIVE_IS_LOCK_FREE */

/*
Pointer versions of relevant operations. Note that some functions cannot be implemented as #defines because for some reason, some compilers
complain with a warning if you don't use the return value. I'm not fully sure why this happens, but to work around this, those particular
functions are just implemented as inlined functions.
*/
#if defined(C89ATOMIC_64BIT)
    static C89ATOMIC_INLINE c89atomic_bool c89atomic_is_lock_free_ptr(volatile void** ptr)
    {
        return c89atomic_is_lock_free_64(ptr);
    }

    static C89ATOMIC_INLINE void* c89atomic_load_explicit_ptr(volatile void** ptr, c89atomic_memory_order order)
    {
        return (void*)c89atomic_load_explicit_64((volatile c89atomic_uint64*)ptr, order);
    }

    static C89ATOMIC_INLINE void c89atomic_store_explicit_ptr(volatile void** dst, void* src, c89atomic_memory_order order)
    {
        c89atomic_store_explicit_64((volatile c89atomic_uint64*)dst, (c89atomic_uint64)src, order);
    }

    static C89ATOMIC_INLINE void* c89atomic_exchange_explicit_ptr(volatile void** dst, void* src, c89atomic_memory_order order)
    {
        return (void*)c89atomic_exchange_explicit_64((volatile c89atomic_uint64*)dst, (c89atomic_uint64)src, order);
    }

    static C89ATOMIC_INLINE c89atomic_bool c89atomic_compare_exchange_strong_explicit_ptr(volatile void** dst, void** expected, void* desired, c89atomic_memory_order successOrder, c89atomic_memory_order failureOrder)
    {
        return c89atomic_compare_exchange_strong_explicit_64((volatile c89atomic_uint64*)dst, (c89atomic_uint64*)expected, (c89atomic_uint64)desired, successOrder, failureOrder);
    }

    static C89ATOMIC_INLINE c89atomic_bool c89atomic_compare_exchange_weak_explicit_ptr(volatile void** dst, void** expected, void* desired, c89atomic_memory_order successOrder, c89atomic_memory_order failureOrder)
    {
        return c89atomic_compare_exchange_weak_explicit_64((volatile c89atomic_uint64*)dst, (c89atomic_uint64*)expected, (c89atomic_uint64)desired, successOrder, failureOrder);
    }

    static C89ATOMIC_INLINE void* c89atomic_compare_and_swap_ptr(volatile void** dst, void* expected, void* desired)
    {
        return (void*)c89atomic_compare_and_swap_64((volatile c89atomic_uint64*)dst, (c89atomic_uint64)expected, (c89atomic_uint64)desired);
    }
#elif defined(C89ATOMIC_32BIT)
    static C89ATOMIC_INLINE c89atomic_bool c89atomic_is_lock_free_ptr(volatile void** ptr)
    {
        return c89atomic_is_lock_free_32((volatile c89atomic_uint32*)ptr);
    }

    static C89ATOMIC_INLINE void* c89atomic_load_explicit_ptr(volatile void** ptr, c89atomic_memory_order order)
    {
        return (void*)c89atomic_load_explicit_32((volatile c89atomic_uint32*)ptr, order);
    }

    static C89ATOMIC_INLINE void c89atomic_store_explicit_ptr(volatile void** dst, void* src, c89atomic_memory_order order)
    {
        c89atomic_store_explicit_32((volatile c89atomic_uint32*)dst, (c89atomic_uint32)src, order);
    }

    static C89ATOMIC_INLINE void* c89atomic_exchange_explicit_ptr(volatile void** dst, void* src, c89atomic_memory_order order)
    {
        return (void*)c89atomic_exchange_explicit_32((volatile c89atomic_uint32*)dst, (c89atomic_uint32)src, order);
    }

    static C89ATOMIC_INLINE c89atomic_bool c89atomic_compare_exchange_strong_explicit_ptr(volatile void** dst, void** expected, void* desired, c89atomic_memory_order successOrder, c89atomic_memory_order failureOrder)
    {
        return c89atomic_compare_exchange_strong_explicit_32((volatile c89atomic_uint32*)dst, (c89atomic_uint32*)expected, (c89atomic_uint32)desired, successOrder, failureOrder);
    }

    static C89ATOMIC_INLINE c89atomic_bool c89atomic_compare_exchange_weak_explicit_ptr(volatile void** dst, void** expected, void* desired, c89atomic_memory_order successOrder, c89atomic_memory_order failureOrder)
    {
        return c89atomic_compare_exchange_weak_explicit_32((volatile c89atomic_uint32*)dst, (c89atomic_uint32*)expected, (c89atomic_uint32)desired, successOrder, failureOrder);
    }

    static C89ATOMIC_INLINE void* c89atomic_compare_and_swap_ptr(volatile void** dst, void* expected, void* desired)
    {
        return (void*)c89atomic_compare_and_swap_32((volatile c89atomic_uint32*)dst, (c89atomic_uint32)expected, (c89atomic_uint32)desired);
    }
#else
    #error Unsupported architecture.
#endif


/* Implicit Flags. */
#define c89atomic_flag_test_and_set(ptr)                                c89atomic_flag_test_and_set_explicit(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_flag_clear(ptr)                                       c89atomic_flag_clear_explicit(ptr, c89atomic_memory_order_seq_cst)


/* Implicit Pointer. */
#define c89atomic_store_ptr(dst, src)                                   c89atomic_store_explicit_ptr((volatile void**)dst, (void*)src, c89atomic_memory_order_seq_cst)
#define c89atomic_load_ptr(ptr)                                         c89atomic_load_explicit_ptr((volatile void**)ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_exchange_ptr(dst, src)                                c89atomic_exchange_explicit_ptr((volatile void**)dst, (void*)src, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_strong_ptr(dst, expected, desired)   c89atomic_compare_exchange_strong_explicit_ptr((volatile void**)dst, (void**)expected, (void*)desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_weak_ptr(dst, expected, desired)     c89atomic_compare_exchange_weak_explicit_ptr((volatile void**)dst, (void**)expected, (void*)desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)


/* Implicit Unsigned Integer. */
#define c89atomic_test_and_set_8( ptr)                                  c89atomic_test_and_set_explicit_8( ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_test_and_set_16(ptr)                                  c89atomic_test_and_set_explicit_16(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_test_and_set_32(ptr)                                  c89atomic_test_and_set_explicit_32(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_test_and_set_64(ptr)                                  c89atomic_test_and_set_explicit_64(ptr, c89atomic_memory_order_seq_cst)

#define c89atomic_clear_8( ptr)                                         c89atomic_clear_explicit_8( ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_clear_16(ptr)                                         c89atomic_clear_explicit_16(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_clear_32(ptr)                                         c89atomic_clear_explicit_32(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_clear_64(ptr)                                         c89atomic_clear_explicit_64(ptr, c89atomic_memory_order_seq_cst)

#define c89atomic_store_8( dst, src)                                    c89atomic_store_explicit_8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_store_16(dst, src)                                    c89atomic_store_explicit_16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_store_32(dst, src)                                    c89atomic_store_explicit_32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_store_64(dst, src)                                    c89atomic_store_explicit_64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_load_8( ptr)                                          c89atomic_load_explicit_8( ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_load_16(ptr)                                          c89atomic_load_explicit_16(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_load_32(ptr)                                          c89atomic_load_explicit_32(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_load_64(ptr)                                          c89atomic_load_explicit_64(ptr, c89atomic_memory_order_seq_cst)

#define c89atomic_exchange_8( dst, src)                                 c89atomic_exchange_explicit_8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_exchange_16(dst, src)                                 c89atomic_exchange_explicit_16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_exchange_32(dst, src)                                 c89atomic_exchange_explicit_32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_exchange_64(dst, src)                                 c89atomic_exchange_explicit_64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_compare_exchange_strong_8( dst, expected, desired)    c89atomic_compare_exchange_strong_explicit_8( dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_strong_16(dst, expected, desired)    c89atomic_compare_exchange_strong_explicit_16(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_strong_32(dst, expected, desired)    c89atomic_compare_exchange_strong_explicit_32(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_strong_64(dst, expected, desired)    c89atomic_compare_exchange_strong_explicit_64(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)

#define c89atomic_compare_exchange_weak_8(  dst, expected, desired)     c89atomic_compare_exchange_weak_explicit_8( dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_weak_16( dst, expected, desired)     c89atomic_compare_exchange_weak_explicit_16(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_weak_32( dst, expected, desired)     c89atomic_compare_exchange_weak_explicit_32(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_weak_64( dst, expected, desired)     c89atomic_compare_exchange_weak_explicit_64(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_add_8( dst, src)                                c89atomic_fetch_add_explicit_8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_add_16(dst, src)                                c89atomic_fetch_add_explicit_16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_add_32(dst, src)                                c89atomic_fetch_add_explicit_32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_add_64(dst, src)                                c89atomic_fetch_add_explicit_64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_sub_8( dst, src)                                c89atomic_fetch_sub_explicit_8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_sub_16(dst, src)                                c89atomic_fetch_sub_explicit_16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_sub_32(dst, src)                                c89atomic_fetch_sub_explicit_32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_sub_64(dst, src)                                c89atomic_fetch_sub_explicit_64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_or_8( dst, src)                                 c89atomic_fetch_or_explicit_8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_or_16(dst, src)                                 c89atomic_fetch_or_explicit_16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_or_32(dst, src)                                 c89atomic_fetch_or_explicit_32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_or_64(dst, src)                                 c89atomic_fetch_or_explicit_64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_xor_8( dst, src)                                c89atomic_fetch_xor_explicit_8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_xor_16(dst, src)                                c89atomic_fetch_xor_explicit_16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_xor_32(dst, src)                                c89atomic_fetch_xor_explicit_32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_xor_64(dst, src)                                c89atomic_fetch_xor_explicit_64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_and_8( dst, src)                                c89atomic_fetch_and_explicit_8 (dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_and_16(dst, src)                                c89atomic_fetch_and_explicit_16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_and_32(dst, src)                                c89atomic_fetch_and_explicit_32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_and_64(dst, src)                                c89atomic_fetch_and_explicit_64(dst, src, c89atomic_memory_order_seq_cst)


/* Explicit Signed Integer. */
#define c89atomic_test_and_set_explicit_i8( ptr, order)                 (c89atomic_int8 )c89atomic_test_and_set_explicit_8( (c89atomic_uint8* )ptr, order)
#define c89atomic_test_and_set_explicit_i16(ptr, order)                 (c89atomic_int16)c89atomic_test_and_set_explicit_16((c89atomic_uint16*)ptr, order)
#define c89atomic_test_and_set_explicit_i32(ptr, order)                 (c89atomic_int32)c89atomic_test_and_set_explicit_32((c89atomic_uint32*)ptr, order)
#define c89atomic_test_and_set_explicit_i64(ptr, order)                 (c89atomic_int64)c89atomic_test_and_set_explicit_64((c89atomic_uint64*)ptr, order)

#define c89atomic_clear_explicit_i8( ptr, order)                        c89atomic_clear_explicit_8( (c89atomic_uint8* )ptr, order)
#define c89atomic_clear_explicit_i16(ptr, order)                        c89atomic_clear_explicit_16((c89atomic_uint16*)ptr, order)
#define c89atomic_clear_explicit_i32(ptr, order)                        c89atomic_clear_explicit_32((c89atomic_uint32*)ptr, order)
#define c89atomic_clear_explicit_i64(ptr, order)                        c89atomic_clear_explicit_64((c89atomic_uint64*)ptr, order)

#define c89atomic_store_explicit_i8( dst, src, order)                   c89atomic_store_explicit_8( (c89atomic_uint8* )dst, (c89atomic_uint8 )src, order)
#define c89atomic_store_explicit_i16(dst, src, order)                   c89atomic_store_explicit_16((c89atomic_uint16*)dst, (c89atomic_uint16)src, order)
#define c89atomic_store_explicit_i32(dst, src, order)                   c89atomic_store_explicit_32((c89atomic_uint32*)dst, (c89atomic_uint32)src, order)
#define c89atomic_store_explicit_i64(dst, src, order)                   c89atomic_store_explicit_64((c89atomic_uint64*)dst, (c89atomic_uint64)src, order)

#define c89atomic_load_explicit_i8( ptr, order)                         (c89atomic_int8 )c89atomic_load_explicit_8( (c89atomic_uint8* )ptr, order)
#define c89atomic_load_explicit_i16(ptr, order)                         (c89atomic_int16)c89atomic_load_explicit_16((c89atomic_uint16*)ptr, order)
#define c89atomic_load_explicit_i32(ptr, order)                         (c89atomic_int32)c89atomic_load_explicit_32((c89atomic_uint32*)ptr, order)
#define c89atomic_load_explicit_i64(ptr, order)                         (c89atomic_int64)c89atomic_load_explicit_64((c89atomic_uint64*)ptr, order)

#define c89atomic_exchange_explicit_i8( dst, src, order)                (c89atomic_int8 )c89atomic_exchange_explicit_8 ((c89atomic_uint8* )dst, (c89atomic_uint8 )src, order)
#define c89atomic_exchange_explicit_i16(dst, src, order)                (c89atomic_int16)c89atomic_exchange_explicit_16((c89atomic_uint16*)dst, (c89atomic_uint16)src, order)
#define c89atomic_exchange_explicit_i32(dst, src, order)                (c89atomic_int32)c89atomic_exchange_explicit_32((c89atomic_uint32*)dst, (c89atomic_uint32)src, order)
#define c89atomic_exchange_explicit_i64(dst, src, order)                (c89atomic_int64)c89atomic_exchange_explicit_64((c89atomic_uint64*)dst, (c89atomic_uint64)src, order)

#define c89atomic_compare_exchange_strong_explicit_i8( dst, expected, desired, successOrder, failureOrder)  c89atomic_compare_exchange_strong_explicit_8( (c89atomic_uint8* )dst, (c89atomic_uint8* )expected, (c89atomic_uint8 )desired, successOrder, failureOrder)
#define c89atomic_compare_exchange_strong_explicit_i16(dst, expected, desired, successOrder, failureOrder)  c89atomic_compare_exchange_strong_explicit_16((c89atomic_uint16*)dst, (c89atomic_uint16*)expected, (c89atomic_uint16)desired, successOrder, failureOrder)
#define c89atomic_compare_exchange_strong_explicit_i32(dst, expected, desired, successOrder, failureOrder)  c89atomic_compare_exchange_strong_explicit_32((c89atomic_uint32*)dst, (c89atomic_uint32*)expected, (c89atomic_uint32)desired, successOrder, failureOrder)
#define c89atomic_compare_exchange_strong_explicit_i64(dst, expected, desired, successOrder, failureOrder)  c89atomic_compare_exchange_strong_explicit_64((c89atomic_uint64*)dst, (c89atomic_uint64*)expected, (c89atomic_uint64)desired, successOrder, failureOrder)

#define c89atomic_compare_exchange_weak_explicit_i8( dst, expected, desired, successOrder, failureOrder)    c89atomic_compare_exchange_weak_explicit_8( (c89atomic_uint8* )dst, (c89atomic_uint8* )expected, (c89atomic_uint8 )desired, successOrder, failureOrder)
#define c89atomic_compare_exchange_weak_explicit_i16(dst, expected, desired, successOrder, failureOrder)    c89atomic_compare_exchange_weak_explicit_16((c89atomic_uint16*)dst, (c89atomic_uint16*)expected, (c89atomic_uint16)desired, successOrder, failureOrder)
#define c89atomic_compare_exchange_weak_explicit_i32(dst, expected, desired, successOrder, failureOrder)    c89atomic_compare_exchange_weak_explicit_32((c89atomic_uint32*)dst, (c89atomic_uint32*)expected, (c89atomic_uint32)desired, successOrder, failureOrder)
#define c89atomic_compare_exchange_weak_explicit_i64(dst, expected, desired, successOrder, failureOrder)    c89atomic_compare_exchange_weak_explicit_64((c89atomic_uint64*)dst, (c89atomic_uint64*)expected, (c89atomic_uint64)desired, successOrder, failureOrder)

#define c89atomic_fetch_add_explicit_i8( dst, src, order)               (c89atomic_int8 )c89atomic_fetch_add_explicit_8( (c89atomic_uint8* )dst, (c89atomic_uint8 )src, order)
#define c89atomic_fetch_add_explicit_i16(dst, src, order)               (c89atomic_int16)c89atomic_fetch_add_explicit_16((c89atomic_uint16*)dst, (c89atomic_uint16)src, order)
#define c89atomic_fetch_add_explicit_i32(dst, src, order)               (c89atomic_int32)c89atomic_fetch_add_explicit_32((c89atomic_uint32*)dst, (c89atomic_uint32)src, order)
#define c89atomic_fetch_add_explicit_i64(dst, src, order)               (c89atomic_int64)c89atomic_fetch_add_explicit_64((c89atomic_uint64*)dst, (c89atomic_uint64)src, order)

#define c89atomic_fetch_sub_explicit_i8( dst, src, order)               (c89atomic_int8 )c89atomic_fetch_sub_explicit_8( (c89atomic_uint8* )dst, (c89atomic_uint8 )src, order)
#define c89atomic_fetch_sub_explicit_i16(dst, src, order)               (c89atomic_int16)c89atomic_fetch_sub_explicit_16((c89atomic_uint16*)dst, (c89atomic_uint16)src, order)
#define c89atomic_fetch_sub_explicit_i32(dst, src, order)               (c89atomic_int32)c89atomic_fetch_sub_explicit_32((c89atomic_uint32*)dst, (c89atomic_uint32)src, order)
#define c89atomic_fetch_sub_explicit_i64(dst, src, order)               (c89atomic_int64)c89atomic_fetch_sub_explicit_64((c89atomic_uint64*)dst, (c89atomic_uint64)src, order)

#define c89atomic_fetch_or_explicit_i8( dst, src, order)                (c89atomic_int8 )c89atomic_fetch_or_explicit_8( (c89atomic_uint8* )dst, (c89atomic_uint8 )src, order)
#define c89atomic_fetch_or_explicit_i16(dst, src, order)                (c89atomic_int16)c89atomic_fetch_or_explicit_16((c89atomic_uint16*)dst, (c89atomic_uint16)src, order)
#define c89atomic_fetch_or_explicit_i32(dst, src, order)                (c89atomic_int32)c89atomic_fetch_or_explicit_32((c89atomic_uint32*)dst, (c89atomic_uint32)src, order)
#define c89atomic_fetch_or_explicit_i64(dst, src, order)                (c89atomic_int64)c89atomic_fetch_or_explicit_64((c89atomic_uint64*)dst, (c89atomic_uint64)src, order)

#define c89atomic_fetch_xor_explicit_i8( dst, src, order)               (c89atomic_int8 )c89atomic_fetch_xor_explicit_8( (c89atomic_uint8* )dst, (c89atomic_uint8 )src, order)
#define c89atomic_fetch_xor_explicit_i16(dst, src, order)               (c89atomic_int16)c89atomic_fetch_xor_explicit_16((c89atomic_uint16*)dst, (c89atomic_uint16)src, order)
#define c89atomic_fetch_xor_explicit_i32(dst, src, order)               (c89atomic_int32)c89atomic_fetch_xor_explicit_32((c89atomic_uint32*)dst, (c89atomic_uint32)src, order)
#define c89atomic_fetch_xor_explicit_i64(dst, src, order)               (c89atomic_int64)c89atomic_fetch_xor_explicit_64((c89atomic_uint64*)dst, (c89atomic_uint64)src, order)

#define c89atomic_fetch_and_explicit_i8( dst, src, order)               (c89atomic_int8 )c89atomic_fetch_and_explicit_8( (c89atomic_uint8* )dst, (c89atomic_uint8 )src, order)
#define c89atomic_fetch_and_explicit_i16(dst, src, order)               (c89atomic_int16)c89atomic_fetch_and_explicit_16((c89atomic_uint16*)dst, (c89atomic_uint16)src, order)
#define c89atomic_fetch_and_explicit_i32(dst, src, order)               (c89atomic_int32)c89atomic_fetch_and_explicit_32((c89atomic_uint32*)dst, (c89atomic_uint32)src, order)
#define c89atomic_fetch_and_explicit_i64(dst, src, order)               (c89atomic_int64)c89atomic_fetch_and_explicit_64((c89atomic_uint64*)dst, (c89atomic_uint64)src, order)


/* Implicit Signed Integer. */
#define c89atomic_test_and_set_i8( ptr)                                 c89atomic_test_and_set_explicit_i8( ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_test_and_set_i16(ptr)                                 c89atomic_test_and_set_explicit_i16(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_test_and_set_i32(ptr)                                 c89atomic_test_and_set_explicit_i32(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_test_and_set_i64(ptr)                                 c89atomic_test_and_set_explicit_i64(ptr, c89atomic_memory_order_seq_cst)

#define c89atomic_clear_i8( ptr)                                        c89atomic_clear_explicit_i8( ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_clear_i16(ptr)                                        c89atomic_clear_explicit_i16(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_clear_i32(ptr)                                        c89atomic_clear_explicit_i32(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_clear_i64(ptr)                                        c89atomic_clear_explicit_i64(ptr, c89atomic_memory_order_seq_cst)

#define c89atomic_store_i8( dst, src)                                   c89atomic_store_explicit_i8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_store_i16(dst, src)                                   c89atomic_store_explicit_i16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_store_i32(dst, src)                                   c89atomic_store_explicit_i32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_store_i64(dst, src)                                   c89atomic_store_explicit_i64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_load_i8( ptr)                                         c89atomic_load_explicit_i8( ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_load_i16(ptr)                                         c89atomic_load_explicit_i16(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_load_i32(ptr)                                         c89atomic_load_explicit_i32(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_load_i64(ptr)                                         c89atomic_load_explicit_i64(ptr, c89atomic_memory_order_seq_cst)

#define c89atomic_exchange_i8( dst, src)                                c89atomic_exchange_explicit_i8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_exchange_i16(dst, src)                                c89atomic_exchange_explicit_i16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_exchange_i32(dst, src)                                c89atomic_exchange_explicit_i32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_exchange_i64(dst, src)                                c89atomic_exchange_explicit_i64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_compare_exchange_strong_i8( dst, expected, desired)   c89atomic_compare_exchange_strong_explicit_i8( dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_strong_i16(dst, expected, desired)   c89atomic_compare_exchange_strong_explicit_i16(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_strong_i32(dst, expected, desired)   c89atomic_compare_exchange_strong_explicit_i32(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_strong_i64(dst, expected, desired)   c89atomic_compare_exchange_strong_explicit_i64(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)

#define c89atomic_compare_exchange_weak_i8( dst, expected, desired)     c89atomic_compare_exchange_weak_explicit_i8( dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_weak_i16(dst, expected, desired)     c89atomic_compare_exchange_weak_explicit_i16(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_weak_i32(dst, expected, desired)     c89atomic_compare_exchange_weak_explicit_i32(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_weak_i64(dst, expected, desired)     c89atomic_compare_exchange_weak_explicit_i64(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_add_i8( dst, src)                               c89atomic_fetch_add_explicit_i8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_add_i16(dst, src)                               c89atomic_fetch_add_explicit_i16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_add_i32(dst, src)                               c89atomic_fetch_add_explicit_i32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_add_i64(dst, src)                               c89atomic_fetch_add_explicit_i64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_sub_i8( dst, src)                               c89atomic_fetch_sub_explicit_i8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_sub_i16(dst, src)                               c89atomic_fetch_sub_explicit_i16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_sub_i32(dst, src)                               c89atomic_fetch_sub_explicit_i32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_sub_i64(dst, src)                               c89atomic_fetch_sub_explicit_i64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_or_i8( dst, src)                                c89atomic_fetch_or_explicit_i8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_or_i16(dst, src)                                c89atomic_fetch_or_explicit_i16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_or_i32(dst, src)                                c89atomic_fetch_or_explicit_i32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_or_i64(dst, src)                                c89atomic_fetch_or_explicit_i64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_xor_i8( dst, src)                               c89atomic_fetch_xor_explicit_i8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_xor_i16(dst, src)                               c89atomic_fetch_xor_explicit_i16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_xor_i32(dst, src)                               c89atomic_fetch_xor_explicit_i32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_xor_i64(dst, src)                               c89atomic_fetch_xor_explicit_i64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_and_i8( dst, src)                               c89atomic_fetch_and_explicit_i8( dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_and_i16(dst, src)                               c89atomic_fetch_and_explicit_i16(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_and_i32(dst, src)                               c89atomic_fetch_and_explicit_i32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_and_i64(dst, src)                               c89atomic_fetch_and_explicit_i64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_compare_and_swap_i8( dst, expected, dedsired)         (c89atomic_int8 )c89atomic_compare_and_swap_8( (c89atomic_uint8* )dst, (c89atomic_uint8 )expected, (c89atomic_uint8 )dedsired)
#define c89atomic_compare_and_swap_i16(dst, expected, dedsired)         (c89atomic_int16)c89atomic_compare_and_swap_16((c89atomic_uint16*)dst, (c89atomic_uint16)expected, (c89atomic_uint16)dedsired)
#define c89atomic_compare_and_swap_i32(dst, expected, dedsired)         (c89atomic_int32)c89atomic_compare_and_swap_32((c89atomic_uint32*)dst, (c89atomic_uint32)expected, (c89atomic_uint32)dedsired)
#define c89atomic_compare_and_swap_i64(dst, expected, dedsired)         (c89atomic_int64)c89atomic_compare_and_swap_64((c89atomic_uint64*)dst, (c89atomic_uint64)expected, (c89atomic_uint64)dedsired)


/* Floating Point Explicit. */
typedef union
{
    c89atomic_uint32 i;
    float f;
} c89atomic_if32;

typedef union
{
    c89atomic_uint64 i;
    double f;
} c89atomic_if64;

#define c89atomic_clear_explicit_f32(ptr, order)                        c89atomic_clear_explicit_32((c89atomic_uint32*)ptr, order)
#define c89atomic_clear_explicit_f64(ptr, order)                        c89atomic_clear_explicit_64((c89atomic_uint64*)ptr, order)

static C89ATOMIC_INLINE void c89atomic_store_explicit_f32(volatile float* dst, float src, c89atomic_memory_order order)
{
    c89atomic_if32 x;
    x.f = src;
    c89atomic_store_explicit_32((volatile c89atomic_uint32*)dst, x.i, order);
}

static C89ATOMIC_INLINE void c89atomic_store_explicit_f64(volatile double* dst, double src, c89atomic_memory_order order)
{
    c89atomic_if64 x;
    x.f = src;
    c89atomic_store_explicit_64((volatile c89atomic_uint64*)dst, x.i, order);
}


static C89ATOMIC_INLINE float c89atomic_load_explicit_f32(volatile const float* ptr, c89atomic_memory_order order)
{
    c89atomic_if32 r;
    r.i = c89atomic_load_explicit_32((volatile const c89atomic_uint32*)ptr, order);
    return r.f;
}

static C89ATOMIC_INLINE double c89atomic_load_explicit_f64(volatile const double* ptr, c89atomic_memory_order order)
{
    c89atomic_if64 r;
    r.i = c89atomic_load_explicit_64((volatile const c89atomic_uint64*)ptr, order);
    return r.f;
}


static C89ATOMIC_INLINE float c89atomic_exchange_explicit_f32(volatile float* dst, float src, c89atomic_memory_order order)
{
    c89atomic_if32 r;
    c89atomic_if32 x;
    x.f = src;
    r.i = c89atomic_exchange_explicit_32((volatile c89atomic_uint32*)dst, x.i, order);
    return r.f;
}

static C89ATOMIC_INLINE double c89atomic_exchange_explicit_f64(volatile double* dst, double src, c89atomic_memory_order order)
{
    c89atomic_if64 r;
    c89atomic_if64 x;
    x.f = src;
    r.i = c89atomic_exchange_explicit_64((volatile c89atomic_uint64*)dst, x.i, order);
    return r.f;
}


static C89ATOMIC_INLINE c89atomic_bool c89atomic_compare_exchange_strong_explicit_f32(volatile float* dst, float* expected, float desired, c89atomic_memory_order successOrder, c89atomic_memory_order failureOrder)
{
    c89atomic_if32 d;
    d.f = desired;
    return c89atomic_compare_exchange_strong_explicit_32((volatile c89atomic_uint32*)dst, (c89atomic_uint32*)expected, d.i, successOrder, failureOrder);
}

static C89ATOMIC_INLINE c89atomic_bool c89atomic_compare_exchange_strong_explicit_f64(volatile double* dst, double* expected, double desired, c89atomic_memory_order successOrder, c89atomic_memory_order failureOrder)
{
    c89atomic_if64 d;
    d.f = desired;
    return c89atomic_compare_exchange_strong_explicit_64((volatile c89atomic_uint64*)dst, (c89atomic_uint64*)expected, d.i, successOrder, failureOrder);
}


static C89ATOMIC_INLINE c89atomic_bool c89atomic_compare_exchange_weak_explicit_f32(volatile float* dst, float* expected, float desired, c89atomic_memory_order successOrder, c89atomic_memory_order failureOrder)
{
    c89atomic_if32 d;
    d.f = desired;
    return c89atomic_compare_exchange_weak_explicit_32((volatile c89atomic_uint32*)dst, (c89atomic_uint32*)expected, d.i, successOrder, failureOrder);
}

static C89ATOMIC_INLINE c89atomic_bool c89atomic_compare_exchange_weak_explicit_f64(volatile double* dst, double* expected, double desired, c89atomic_memory_order successOrder, c89atomic_memory_order failureOrder)
{
    c89atomic_if64 d;
    d.f = desired;
    return c89atomic_compare_exchange_weak_explicit_64((volatile c89atomic_uint64*)dst, (c89atomic_uint64*)expected, d.i, successOrder, failureOrder);
}


static C89ATOMIC_INLINE float c89atomic_fetch_add_explicit_f32(volatile float* dst, float src, c89atomic_memory_order order)
{
    c89atomic_if32 r;
    c89atomic_if32 x;
    x.f = src;
    r.i = c89atomic_fetch_add_explicit_32((volatile c89atomic_uint32*)dst, x.i, order);
    return r.f;
}

static C89ATOMIC_INLINE double c89atomic_fetch_add_explicit_f64(volatile double* dst, double src, c89atomic_memory_order order)
{
    c89atomic_if64 r;
    c89atomic_if64 x;
    x.f = src;
    r.i = c89atomic_fetch_add_explicit_64((volatile c89atomic_uint64*)dst, x.i, order);
    return r.f;
}


static C89ATOMIC_INLINE float c89atomic_fetch_sub_explicit_f32(volatile float* dst, float src, c89atomic_memory_order order)
{
    c89atomic_if32 r;
    c89atomic_if32 x;
    x.f = src;
    r.i = c89atomic_fetch_sub_explicit_32((volatile c89atomic_uint32*)dst, x.i, order);
    return r.f;
}

static C89ATOMIC_INLINE double c89atomic_fetch_sub_explicit_f64(volatile double* dst, double src, c89atomic_memory_order order)
{
    c89atomic_if64 r;
    c89atomic_if64 x;
    x.f = src;
    r.i = c89atomic_fetch_sub_explicit_64((volatile c89atomic_uint64*)dst, x.i, order);
    return r.f;
}


static C89ATOMIC_INLINE float c89atomic_fetch_or_explicit_f32(volatile float* dst, float src, c89atomic_memory_order order)
{
    c89atomic_if32 r;
    c89atomic_if32 x;
    x.f = src;
    r.i = c89atomic_fetch_or_explicit_32((volatile c89atomic_uint32*)dst, x.i, order);
    return r.f;
}

static C89ATOMIC_INLINE double c89atomic_fetch_or_explicit_f64(volatile double* dst, double src, c89atomic_memory_order order)
{
    c89atomic_if64 r;
    c89atomic_if64 x;
    x.f = src;
    r.i = c89atomic_fetch_or_explicit_64((volatile c89atomic_uint64*)dst, x.i, order);
    return r.f;
}


static C89ATOMIC_INLINE float c89atomic_fetch_xor_explicit_f32(volatile float* dst, float src, c89atomic_memory_order order)
{
    c89atomic_if32 r;
    c89atomic_if32 x;
    x.f = src;
    r.i = c89atomic_fetch_xor_explicit_32((volatile c89atomic_uint32*)dst, x.i, order);
    return r.f;
}

static C89ATOMIC_INLINE double c89atomic_fetch_xor_explicit_f64(volatile double* dst, double src, c89atomic_memory_order order)
{
    c89atomic_if64 r;
    c89atomic_if64 x;
    x.f = src;
    r.i = c89atomic_fetch_xor_explicit_64((volatile c89atomic_uint64*)dst, x.i, order);
    return r.f;
}


static C89ATOMIC_INLINE float c89atomic_fetch_and_explicit_f32(volatile float* dst, float src, c89atomic_memory_order order)
{
    c89atomic_if32 r;
    c89atomic_if32 x;
    x.f = src;
    r.i = c89atomic_fetch_and_explicit_32((volatile c89atomic_uint32*)dst, x.i, order);
    return r.f;
}

static C89ATOMIC_INLINE double c89atomic_fetch_and_explicit_f64(volatile double* dst, double src, c89atomic_memory_order order)
{
    c89atomic_if64 r;
    c89atomic_if64 x;
    x.f = src;
    r.i = c89atomic_fetch_and_explicit_64((volatile c89atomic_uint64*)dst, x.i, order);
    return r.f;
}



/* Float Point Implicit */
#define c89atomic_clear_f32(ptr)                                        (float )c89atomic_clear_explicit_f32(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_clear_f64(ptr)                                        (double)c89atomic_clear_explicit_f64(ptr, c89atomic_memory_order_seq_cst)

#define c89atomic_store_f32(dst, src)                                   c89atomic_store_explicit_f32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_store_f64(dst, src)                                   c89atomic_store_explicit_f64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_load_f32(ptr)                                         (float )c89atomic_load_explicit_f32(ptr, c89atomic_memory_order_seq_cst)
#define c89atomic_load_f64(ptr)                                         (double)c89atomic_load_explicit_f64(ptr, c89atomic_memory_order_seq_cst)

#define c89atomic_exchange_f32(dst, src)                                (float )c89atomic_exchange_explicit_f32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_exchange_f64(dst, src)                                (double)c89atomic_exchange_explicit_f64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_compare_exchange_strong_f32(dst, expected, desired)   c89atomic_compare_exchange_strong_explicit_f32(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_strong_f64(dst, expected, desired)   c89atomic_compare_exchange_strong_explicit_f64(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)

#define c89atomic_compare_exchange_weak_f32(dst, expected, desired)     c89atomic_compare_exchange_weak_explicit_f32(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)
#define c89atomic_compare_exchange_weak_f64(dst, expected, desired)     c89atomic_compare_exchange_weak_explicit_f64(dst, expected, desired, c89atomic_memory_order_seq_cst, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_add_f32(dst, src)                               c89atomic_fetch_add_explicit_f32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_add_f64(dst, src)                               c89atomic_fetch_add_explicit_f64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_sub_f32(dst, src)                               c89atomic_fetch_sub_explicit_f32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_sub_f64(dst, src)                               c89atomic_fetch_sub_explicit_f64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_or_f32(dst, src)                                c89atomic_fetch_or_explicit_f32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_or_f64(dst, src)                                c89atomic_fetch_or_explicit_f64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_xor_f32(dst, src)                               c89atomic_fetch_xor_explicit_f32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_xor_f64(dst, src)                               c89atomic_fetch_xor_explicit_f64(dst, src, c89atomic_memory_order_seq_cst)

#define c89atomic_fetch_and_f32(dst, src)                               c89atomic_fetch_and_explicit_f32(dst, src, c89atomic_memory_order_seq_cst)
#define c89atomic_fetch_and_f64(dst, src)                               c89atomic_fetch_and_explicit_f64(dst, src, c89atomic_memory_order_seq_cst)

static C89ATOMIC_INLINE float c89atomic_compare_and_swap_f32(volatile float* dst, float expected, float desired)
{
    c89atomic_if32 r;
    c89atomic_if32 e, d;
    e.f = expected;
    d.f = desired;
    r.i = c89atomic_compare_and_swap_32((volatile c89atomic_uint32*)dst, e.i, d.i);
    return r.f;
}

static C89ATOMIC_INLINE double c89atomic_compare_and_swap_f64(volatile double* dst, double expected, double desired)
{
    c89atomic_if64 r;
    c89atomic_if64 e, d;
    e.f = expected;
    d.f = desired;
    r.i = c89atomic_compare_and_swap_64((volatile c89atomic_uint64*)dst, e.i, d.i);
    return r.f;
}



/* Spinlock */
typedef c89atomic_flag c89atomic_spinlock;

static C89ATOMIC_INLINE void c89atomic_spinlock_lock(volatile c89atomic_spinlock* pSpinlock)
{
    for (;;) {
        if (c89atomic_flag_test_and_set_explicit(pSpinlock, c89atomic_memory_order_acquire) == 0) {
            break;
        }

        while (c89atomic_flag_load_explicit(pSpinlock, c89atomic_memory_order_relaxed) == 1) {
            /* Do nothing. */
        }
    }
}

static C89ATOMIC_INLINE void c89atomic_spinlock_unlock(volatile c89atomic_spinlock* pSpinlock)
{
    c89atomic_flag_clear_explicit(pSpinlock, c89atomic_memory_order_release);
}

#ifdef _WIN32
    typedef volatile void *atomic_ptr_t;
#ifndef _STDATOMIC_H
    typedef volatile c89atomic_flag atomic_flag;
    typedef volatile c89atomic_bool atomic_bool;
    typedef volatile c89atomic_int8 atomic_char;
    typedef volatile c89atomic_int8 atomic_schar;
    typedef volatile c89atomic_uint8 atomic_uchar;
    typedef volatile c89atomic_int16 atomic_short;
    typedef volatile c89atomic_uint16 atomic_ushort;
    typedef volatile c89atomic_int32 atomic_int;
    typedef volatile c89atomic_uint32 atomic_uint;
    typedef volatile signed long atomic_long;
    typedef volatile unsigned long atomic_ulong;
    typedef volatile c89atomic_int64 atomic_llong;
    typedef volatile c89atomic_uint64 atomic_ullong;
    typedef volatile intptr_t atomic_intptr_t;
    typedef volatile uintptr_t atomic_uintptr_t;
    typedef volatile size_t atomic_size_t;
    typedef volatile ptrdiff_t atomic_ptrdiff_t;
    typedef volatile intmax_t atomic_intmax_t;
    typedef volatile uintmax_t atomic_uintmax_t;
#endif

    static C89ATOMIC_INLINE c89atomic_bool c89atomic_cas_32(atomic_uint *a, c89atomic_uint32 *cmp, c89atomic_uint32 set) {
#if defined(__TINYC__) && defined(_WIN32) && (defined(__arm__) || defined(__i386__)) && !defined(_MSC_VER)
        return (c89atomic_bool)atomic_compare_exchange_strong((atomic_uint *)a, (unsigned int *)&cmp, set);
#else
        c89atomic_int32 initial_cmp = *cmp;
        c89atomic_int32 initial_a = _InterlockedCompareExchange((atomic_long *)a, set, initial_cmp);
        c89atomic_bool ret = (initial_a == initial_cmp);
        if (!ret)
            *cmp = initial_a;

        return ret;
#endif
    }

    static C89ATOMIC_INLINE c89atomic_bool c89atomic_cas_64(atomic_ullong *a, c89atomic_uint64 *cmp, c89atomic_uint64 set) {
#if defined(__TINYC__) && defined(_WIN32) && (defined(__arm__) || defined(__i386__)) && !defined(_MSC_VER)
        return (c89atomic_bool)atomic_compare_exchange_strong((atomic_ulong *)a, (unsigned long *)&cmp, set);
#else
        c89atomic_int64 initial_cmp = *cmp;
        c89atomic_int64 initial_a = _InterlockedCompareExchange64((atomic_llong *)a, (c89atomic_int64)set, (c89atomic_int64)initial_cmp);
        c89atomic_bool ret = (initial_a == initial_cmp);
        if (!ret)
            *cmp = initial_a;

        return ret;
#endif
    }

    static C89ATOMIC_INLINE c89atomic_bool c89atomic_swap(atomic_ptr_t *a, void **cmp, void *set) {
#if defined(__TINYC__) && defined(_WIN32) && (defined(__arm__) || defined(__i386__)) && !defined(_MSC_VER)
        return (c89atomic_bool)atomic_compare_exchange_strong((atomic_uintptr_t *)a, (uintptr_t *)&cmp, (uintptr_t *)&set);
#else
        void *initial_cmp = *cmp;
        void *initial_a = _InterlockedCompareExchangePointer((void *volatile *)a, set, initial_cmp);
        c89atomic_bool ret = (initial_a == initial_cmp);
        if (!ret)
            *cmp = initial_a;

        return ret;
#endif
    }

#define make_atomic(type, var)  typedef volatile type var;
#define atomic_cas_32(obj, expected, desired) c89atomic_cas_32((atomic_int *)obj, (c89atomic_int32 *)expected, (c89atomic_int32)desired)
#define atomic_cas(obj, expected, desired) c89atomic_cas_64((atomic_llong *)obj, (c89atomic_int64 *)expected, (c89atomic_int64)desired)
#define atomic_swap(obj, expected, desired) c89atomic_swap((atomic_ptr_t *)obj, (void **)expected, (void *)desired)
#else
typedef volatile _Atomic(void *)atomic_ptr_t;
#   define make_atomic(type, var)  typedef volatile _Atomic(type)var;

#if defined(_STDATOMIC_H)
#   define atomic_cas_32(P, E, D)   atomic_compare_exchange_strong((P), (E), (D))
#   define atomic_cas(P, E, D)  atomic_compare_exchange_strong((P), (E), (D))
#   define atomic_swap(P, E, D) atomic_compare_exchange_strong((P), (E), (D))
#else
#   define atomic_cas_32(P, E, D)  __atomic_compare_exchange_n((P), (E), (D), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#   define atomic_cas(P, E, D)  __atomic_compare_exchange_n((P), (E), (D), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#   define atomic_swap(P, E, D)    __atomic_compare_exchange_n((P), (E), (D), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#endif
#ifndef _STDATOMIC_H
typedef volatile _Atomic(c89atomic_flag)atomic_flag;
typedef volatile _Atomic(c89atomic_bool)atomic_bool;
typedef volatile _Atomic(c89atomic_int8)atomic_char;
typedef volatile _Atomic(c89atomic_int8)atomic_schar;
typedef volatile _Atomic(c89atomic_uint8)atomic_uchar;
typedef volatile _Atomic(c89atomic_int16)atomic_short;
typedef volatile _Atomic(c89atomic_uint16)atomic_ushort;
typedef volatile _Atomic(c89atomic_int32)atomic_int;
typedef volatile _Atomic(c89atomic_uint32)atomic_uint;
typedef volatile _Atomic(signed long)atomic_long;
typedef volatile _Atomic(unsigned long)atomic_ulong;
typedef volatile _Atomic(c89atomic_int64)atomic_llong;
typedef volatile _Atomic(c89atomic_uint64)atomic_ullong;
typedef volatile _Atomic(intptr_t)atomic_intptr_t;
typedef volatile _Atomic(uintptr_t)atomic_uintptr_t;
typedef volatile _Atomic(__SIZE_TYPE__)atomic_size_t;
typedef volatile _Atomic(__PTRDIFF_TYPE__)atomic_ptrdiff_t;
typedef volatile _Atomic(intmax_t)atomic_intmax_t;
typedef volatile _Atomic(uintmax_t)atomic_uintmax_t;
#endif
#endif

#if defined(__i386__) || defined(__ppc__) || defined(__arm__) || defined(_M_ARM) || defined(__i386) || defined(_M_IX86)
/* reads a value from an atomic object then cast to type */
#   define atomic_get(type, obj)  (type)c89atomic_load_32((c89atomic_uint32 *)obj)
#else
/* reads a value from an atomic object then cast to type */
#   define atomic_get(type, obj)  (type)c89atomic_load_64((c89atomic_uint64 *)obj)
#endif

#if !defined(_STDATOMIC_H)
/* reads an atomic_flag */
#define atomic_flag_load(ptr)	c89atomic_flag_load_explicit((atomic_flag *)ptr, memory_order_seq_cst)

/* sets an atomic_flag to false */
#define atomic_flag_clear(ptr)	c89atomic_flag_clear((atomic_flag *)ptr)
/* sets an atomic_flag to false */
#define atomic_flag_clear_explicit	c89atomic_flag_clear_explicit

#define atomic_compiler_fence	c89atomic_compiler_fence
/* generic memory order-dependent fence synchronization primitive */
#define atomic_thread_fence(order)	c89atomic_thread_fence(order)
/* fence between a thread and a signal handler executed in the same thread */
#define atomic_signal_fence(order)	c89atomic_signal_fence(order)

/* sets an atomic_flag to true and returns the old value */
#define atomic_flag_test_and_set(obj)	c89atomic_flag_test_and_set((atomic_flag *)obj)
/* sets an atomic_flag to true and returns the old value */
#define atomic_flag_test_and_set_explicit(obj, order)	c89atomic_flag_test_and_set_explicit((atomic_flag *)obj, order)

#if defined(__i386__) || defined(__ppc__) || defined(__arm__) || defined(_M_ARM) || defined(__i386) || defined(_M_IX86)
/* sets an atomic_flag to true and returns the old value */
#define atomic_is_lock_free(obj)  c89atomic_is_lock_free_32((c89atomic_uint32 *)obj)

/* stores a value in an atomic object */
#define atomic_store(obj, desired)	c89atomic_store_32((c89atomic_uint32 *)obj, (c89atomic_uint32)desired)
/* stores a value in an atomic object */
#define atomic_store_explicit(obj, desired, order)	c89atomic_store_explicit_32((c89atomic_uint32 *)obj, (c89atomic_uint32)desired, order)

/* reads a value from an atomic object */
#define atomic_load(obj)	c89atomic_load_32((c89atomic_uint32 *)obj)
/* reads a value from an atomic object */
#define atomic_load_explicit(obj, order)	c89atomic_load_explicit_32((c89atomic_uint32 *)obj, order)

/* initializes an existing atomic object */
#define atomic_init(obj, desired)  c89atomic_store_32((c89atomic_uint32 *)obj, (c89atomic_uint32)desired)

/* atomic addition */
#define atomic_fetch_add(obj, arg)	c89atomic_fetch_add_32((c89atomic_uint32 *)obj, (c89atomic_uint32)arg)
/* atomic addition */
#define atomic_fetch_add_explicit(obj, arg, order)	c89atomic_fetch_add_explicit_32((c89atomic_uint32 *)obj, (c89atomic_uint32)arg, order)

/* atomic subtraction */
#define atomic_fetch_sub(obj, arg)	c89atomic_fetch_sub_32((c89atomic_uint32 *)obj, (c89atomic_uint32)arg)
/* atomic subtraction */
#define atomic_fetch_sub_explicit(obj, arg, order)	c89atomic_fetch_sub_explicit_32((c89atomic_uint32 *)obj, (c89atomic_uint32)arg, order)

/* atomic bitwise OR */
#define atomic_fetch_or(obj, arg)	c89atomic_fetch_or_32((c89atomic_uint32 *)obj, (c89atomic_uint32)arg)
/* atomic bitwise OR */
#define atomic_fetch_or_explicit(obj, arg, order)	c89atomic_fetch_or_explicit_32((c89atomic_uint32 *)obj, (c89atomic_uint32)arg, order)

/* atomic bitwise exclusive OR */
#define atomic_fetch_xor(obj, arg)	c89atomic_fetch_xor_32((c89atomic_uint32 *)obj, (c89atomic_uint32)arg)
/* atomic bitwise exclusive OR */
#define atomic_fetch_xor_explicit(obj, arg, order)	c89atomic_fetch_xor_explicit_32((c89atomic_uint32 *)obj, (c89atomic_uint32)arg, order)

/* atomic bitwise AND */
#define atomic_fetch_and(obj, arg)	                                                \
    c89atomic_fetch_and_32((c89atomic_uint32 *)obj, (c89atomic_uint32)arg)
/* atomic bitwise AND */
#define atomic_fetch_and_explicit(obj, arg, order)                                  \
	c89atomic_fetch_and_explicit_32((c89atomic_uint32 *)obj, (c89atomic_uint32)arg, order)

/* swaps a value with the value of an atomic object */
#define atomic_exchange(obj, desired)	                                            \
    c89atomic_exchange_32((c89atomic_uint32 *)obj, (c89atomic_uint32)desired)
/* swaps a value with the value of an atomic object */
#define atomic_exchange_explicit(obj, desired, order)	                            \
    c89atomic_exchange_explicit_32((c89atomic_uint32 *)obj, (c89atomic_uint32)desired, order)

/* swaps a value with an atomic object if the old value is what is expected, otherwise reads the old value */
#define atomic_compare_exchange_weak(obj, expected, desired)    atomic_cas_32(obj, expected, desired)
/* swaps a value with an atomic object if the old value is what is expected, otherwise reads the old value */
#define atomic_compare_exchange_weak_explicit(obj, expected, desired, succ, fail)	\
    c89atomic_compare_exchange_weak_explicit_32((c89atomic_uint32 *)obj, (c89atomic_uint32)expected, (c89atomic_uint32)desired, succ, fail)

/* swaps a value with an atomic object if the old value is what is expected, otherwise reads the old value */
#define atomic_compare_exchange_strong(obj, expected, desired)  atomic_cas_32(obj, expected, desired)
/* swaps a value with an atomic object if the old value is what is expected, otherwise reads the old value */
#define atomic_compare_exchange_strong_explicit(obj, expected, desired, succ, fail)	\
    c89atomic_compare_exchange_strong_explicit_32((c89atomic_uint32 *)obj, (c89atomic_uint32)expected, (c89atomic_uint32)desired, succ, fail)
#else
/* indicates whether the atomic object is lock-free */
#define atomic_is_lock_free(obj)  c89atomic_is_lock_free_64((c89atomic_uint64 *)obj)

/* stores a value in an atomic object */
#define atomic_store(obj, desired)	c89atomic_store_64((c89atomic_uint64 *)obj, (c89atomic_uint64)desired)
/* stores a value in an atomic object */
#define atomic_store_explicit(obj, desired, order)	c89atomic_store_explicit_64((c89atomic_uint64 *)obj, (c89atomic_uint64)desired, order)

/* reads a value from an atomic object */
#define atomic_load(obj)    c89atomic_load_64((c89atomic_uint64 *)obj)
/* reads a value from an atomic object */
#define atomic_load_explicit(obj, order)	c89atomic_load_explicit_64((c89atomic_uint64 *)obj, order)

/* initializes an existing atomic object */
#define atomic_init(obj, desired)  c89atomic_store_64((c89atomic_uint64 *)obj, (c89atomic_uint64)desired)

/* atomic addition */
#define atomic_fetch_add(obj, arg)	c89atomic_fetch_add_64((c89atomic_uint64 *)obj, (c89atomic_uint64)arg)
/* atomic addition */
#define atomic_fetch_add_explicit(obj, arg, order)	c89atomic_fetch_add_explicit_64((c89atomic_uint64 *)obj, (c89atomic_uint64)arg, order)

/* atomic subtraction */
#define atomic_fetch_sub(obj, arg)	c89atomic_fetch_sub_64((c89atomic_uint64 *)obj, (c89atomic_uint64)arg)
/* atomic subtraction */
#define atomic_fetch_sub_explicit(obj, arg, order)	c89atomic_fetch_sub_explicit_64(obj, (c89atomic_uint64)arg, order)

/* atomic bitwise OR */
#define atomic_fetch_or(obj, arg)	c89atomic_fetch_or_64((c89atomic_uint64 *)obj, (c89atomic_uint64 *)arg)
/* atomic bitwise OR */
#define atomic_fetch_or_explicit(obj, arg, order)	c89atomic_fetch_or_explicit_64((c89atomic_uint64 *)obj, (c89atomic_uint64)arg, order)

/* atomic bitwise exclusive OR */
#define atomic_fetch_xor(obj, arg)	                            \
    c89atomic_fetch_xor_64((c89atomic_uint64 *)obj, (c89atomic_uint64)arg)
/* atomic bitwise exclusive OR */
#define atomic_fetch_xor_explicit(obj, arg, order)	            \
    c89atomic_fetch_xor_explicit_64((c89atomic_uint64 *)obj, (c89atomic_uint64)arg, order)

/* atomic bitwise AND */
#define atomic_fetch_and(obj, arg)	                            \
    c89atomic_fetch_and_64((c89atomic_uint64 *)obj, (c89atomic_uint64)arg)
/* atomic bitwise AND */
#define atomic_fetch_and_explicit(obj, arg, order)	            \
    c89atomic_fetch_and_explicit_64((c89atomic_uint64 *)obj, (c89atomic_uint64)arg, order)

/* swaps a value with the value of an atomic object */
#define atomic_exchange(obj, desired)	                        \
    c89atomic_exchange_64((c89atomic_uint64 *)obj, (c89atomic_uint64)desired)
/* swaps a value with the value of an atomic object */
#define atomic_exchange_explicit(obj, desired, order)	        \
    c89atomic_exchange_explicit_64((c89atomic_uint64 *)obj, (c89atomic_uint64)(c89atomic_uint64)desired, order)

/* swaps a value with an atomic object if the old value is what is expected, otherwise reads the old value */
#define atomic_compare_exchange_weak(obj, expected, desired)    atomic_cas(obj, expected, desired)
/* swaps a value with an atomic object if the old value is what is expected, otherwise reads the old value */
#define atomic_compare_exchange_weak_explicit(obj, expected, desired, succ, fail)	\
    c89atomic_compare_exchange_weak_explicit_64((c89atomic_uint64 *)obj, (c89atomic_uint64)expected, (c89atomic_uint64)desired, succ, fail)

/* swaps a value with an atomic object if the old value is what is expected, otherwise reads the old value */
#define atomic_compare_exchange_strong(obj, expected, desired)	atomic_cas(obj, expected, desired)
/* swaps a value with an atomic object if the old value is what is expected, otherwise reads the old value */
#define atomic_compare_exchange_strong_explicit(obj, expected, desired, succ, fail)	\
    c89atomic_compare_exchange_strong_explicit_64((c89atomic_uint64 *)obj, (c89atomic_uint64)expected, (c89atomic_uint64)desired, succ, fail)
#endif
#endif

#if defined(__arm__) || defined(_M_ARM) || defined(_M_ARM64) || defined(__mips) || defined(__mips__) || defined(__mips64) || defined(__mips32) || defined(__MIPSEL__) || defined(__MIPSEB__) || defined(__sparc__) || defined(__sparc64__) || defined(__sparc_v9__) || defined(__sparcv9) || defined(__riscv) || defined(__ARM64__)
#   define __ATOMIC_PAD_LINE 32
#elif defined(__m68k__)
#   define __ATOMIC_PAD_LINE 16
#elif defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(__ppc__) || defined(__ppc) || defined(__powerpc__) || defined(_M_MPPC) || defined(_M_PPC) ||  defined(__aarch64__)  || defined(__ppc64__) || defined(__powerpc64__) || defined(__arc__)
#   define __ATOMIC_PAD_LINE 128
#elif defined(__s390__) || defined(__s390x__)
#   define __ATOMIC_PAD_LINE 256
#else
#   define __ATOMIC_PAD_LINE 64
#endif

/* The estimated size of the CPU's cache line when atomically updating memory.
 Add this much padding or align to this boundary to avoid atomically-updated
 memory from forcing cache invalidations on near, but non-atomic, memory.

 https://en.wikipedia.org/wiki/False_sharing
 https://github.com/golang/go/search?q=CacheLinePadSize
 https://github.com/ziglang/zig/blob/a69d403cb2c82ce6257bfa1ee7eba52f895c14e7/lib/std/atomic.zig#L445
*/
#define __ATOMIC_CACHE_LINE __ATOMIC_PAD_LINE

#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
    #pragma GCC diagnostic pop  /* long long warnings with Clang. */
#endif

#if defined(__cplusplus)
}
#endif
#endif  /* C_ATOMIC_H */

/*
This software is available as a choice of the following licenses. Choose
whichever you prefer.

===============================================================================
ALTERNATIVE 1 - Public Domain (www.unlicense.org)
===============================================================================
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

===============================================================================
ALTERNATIVE 2 - MIT No Attribution
===============================================================================
Copyright 2020 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
