# Supported targets

In the following lists, supported targets are only those that have been tested
and confirmed working. It is quite possible that **c-coroutine** will work on more
processors, compilers and operating systems than those listed below.

The "Overhead" is the cost of switching co-routines, as compared to an ordinary
C function call.

## Target - x86

* **Overhead:** ~5x
* **Supported processor(s):** 32-bit x86
* **Supported compiler(s):** any
* **Supported operating system(s):**
  * Windows
  * Mac OS X
  * Linux
  * BSD

## Target - amd64

* **Overhead:** ~10x (Windows), ~6x (all other platforms)
* **Supported processor(s):** 64-bit amd64
* **Supported compiler(s):** any
* **Supported operating system(s):**
  * Windows
  * Mac OS X
  * Linux
  * BSD

## Target - ppc

* **Overhead:** ~20x
* **Supported processor(s):** 32-bit PowerPC, 64-bit PowerPC
* **Supported compiler(s):** GNU GCC
* **Supported operating system(s):**
  * Mac OS X
  * Linux
  * BSD
  * Playstation 3

**Note:** this module contains compiler flags to enable/disable FPU and Altivec
support.

## Target - fiber

This uses Windows' "fibers" API.

* **Overhead:** ~15x
* **Supported processor(s):** Processor independent
* **Supported compiler(s):** any
* **Supported operating system(s):**
  * Windows

## Target - sjlj

This uses the C standard library's `setjump`/`longjmp` APIs.

* **Overhead:** ~30x
* **Supported processor(s):** Processor independent
* **Supported compiler(s):** any
* **Supported operating system(s):**
  * Mac OS X
  * Linux
  * BSD
  * Solaris

## Target - ucontext

This uses the POSIX "ucontext" API.

* **Overhead:** ***~300x***
* **Supported processor(s):** Processor independent
* **Supported compiler(s):** any
* **Supported operating system(s):**
  * Linux
  * BSD
