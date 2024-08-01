/*
 sys/mman.h
 mman-win32

mman library for Windows. mirror of <https://code.google.com/p/mman-win32/>

A light implementation of the mmap functions for MinGW.

The mmap-win32 library implements a wrapper for mmap functions around the memory mapping Windows API.

see [mmap(2) — Linux manual page](https://man7.org/linux/man-pages/man2/mmap.2.html)

Initial code forked from <https://github.com/alitrack/mman-win32>

License: MIT License
 */

#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#include "types.h"
#include "stat.h"

#if defined(MMAN_LIBRARY_DLL)
/* Windows shared libraries (DLL) must be declared export when building the lib and import when building the
application which links against the library. */
#if defined(MMAN_LIBRARY)
#define MMANSHARED_EXPORT __declspec(dllexport)
#else
#define MMANSHARED_EXPORT __declspec(dllimport)
#endif /* MMAN_LIBRARY */
#else
/* Static libraries do not require a __declspec attribute.*/
#define MMANSHARED_EXPORT
#endif /* MMAN_LIBRARY_DLL */

/* Determine offset type */
#include <stdint.h>
#if defined(_WIN64)
typedef int64_t OffsetType;
#else
typedef uint32_t OffsetType;
#endif

#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PROT_NONE 0x0  /* Page can not be accessed. */
#define PROT_READ 0x1  /* Page can be read. */
#define PROT_WRITE 0x2 /* Page can be written. */
#define PROT_EXEC 0x4  /* Page can be executed. */

#define MAP_FILE 0
#define MAP_SHARED 0x01  /* Share changes. */
#define MAP_PRIVATE 0x02 /* Changes are private. */
#define MAP_TYPE 0xf
#define MAP_FIXED 0x10 /* Interpret addr exactly. */
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS

#define MAP_NOCACHE (0)
#define MAP_NOEXTEND 0x0100 /* for MAP_FILE, don't change file size */
#define MAP_FAILED ((void *)-1)

/* Flags for msync. */
#define MS_ASYNC 1
#define MS_SYNC 2
#define MS_INVALIDATE 4

MMANSHARED_EXPORT void *mmap(void *addr, size_t len, int prot, int flags, int fildes, OffsetType off);
MMANSHARED_EXPORT int munmap(void *addr, size_t len);
MMANSHARED_EXPORT int mprotect(void *addr, size_t len, int prot);
MMANSHARED_EXPORT int msync(void *addr, size_t len, int flags);
MMANSHARED_EXPORT int mlock(const void *addr, size_t len);
MMANSHARED_EXPORT int munlock(const void *addr, size_t len);
MMANSHARED_EXPORT long getpagesize(void);

#ifdef __cplusplus
}
#endif

#endif /*  _SYS_MMAN_H_ */
