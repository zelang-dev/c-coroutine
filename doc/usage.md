# License

**c-coroutine** is released under the ISC license.

## Foreword

**c-coroutine** is a cross-platform, permissively licensed implementation of
cooperative-multithreading; a feature that is sorely lacking from the ISO C/C++
standard.

The library is designed for maximum speed and portability, and not for safety or
features. If safety or extra functionality is desired, a wrapper API can easily
be written to encapsulate all library functions.

Behavior of executing operations that are listed as not permitted below result
in undefined behavior. They may work anyway, they may cause undesired / unknown
behavior, or they may crash the program entirely.

The goal of this library was to simplify the base API as much as possible,
implementing only that which cannot be implemented using pure C. Additional
functionality after this would only complicate ports of this library to new
platforms.

## Porting

This document is included as a reference for porting **c-coroutine**.

Note that there are a variety of compile-time options in `c_coro.h`,
so if you want to use **c-coroutine** on a platform where it is not supported by default,
you may be able to configure the implementation appropriately without having
to make a whole new port.

## Synopsis

```c
typedef void* co_routine_t;

co_routine_t *co_active(void);
co_routine_t *co_create(unsigned int heap_size, void (*func)(void));
void       co_delete(co_routine_t *handle);
void       co_switch(co_routine_t *handle);
```

## Usage

### co_routine_t

```c
typedef void* co_routine_t;
```

Handle to coroutine.

Handle must be of type `void*`.

A value of null (0) indicates an uninitialized or invalid handle, whereas a
non-zero value indicates a valid handle. A valid handle is backed by execution
state to which the execution can be co_switch()ed to.

## co_active

```c
co_routine_t *co_active(void);
```

Return handle to current coroutine.

Note that the handle is valid even if the function is called from a non-coroutine
context. To achieve this, we save the execution state in an internal buffer,
instead of using the user-provided memory. Since this handle is valid, it can
be used to co_switch to this context from another coroutine. In multi-threaded
applications, make sure to not switch non-coroutine context across CPU cores,
to prevent any possible conflicts with the OS scheduler.

## co_derive

```c
co_routine_t *co_derive(void* memory,
                     unsigned int heap_size,
                     void (*func)(void));
```

Initializes new coroutine.

This function is identical to `co_create`, only it attempts to use the provided
memory instead of allocating new memory on the heap. Please note that certain
implementations (currently only Windows Fibers) cannot be created using existing
memory, and as such, this function will fail.

## co_create

```c
co_routine_t *co_create(unsigned int heap_size,
                     void (*func)(void));
```

Create new coroutine.

`heap_size` is the amount of memory allocated for the coroutine stack, specified
in bytes. This is unfortunately impossible to make fully portable. It is
recommended to specify sizes using `n * sizeof(void*)`. It is better to err
on the side of caution and allocate more memory than will be needed to ensure
compatibility with other platforms, within reason. A typical heap_size for a
32-bit architecture is ~1MB.

When the new coroutine is first called, program execution jumps to func.
This function does not take any arguments, due to portability issues with
passing function arguments. However, arguments can be simulated by the use
of global variables, which can be set before the first call to each coroutine.

`func()` must not return, and should end with an appropriate `co_switch()`
statement. Behavior is undefined if entry point returns normally.

Library is responsible for allocating coroutine stack memory, to free
the user from needing to allocate special memory capable of being used
as program stack memory on platforms where this is required.

User is always responsible for deleting coroutines with `co_delete()`.

Return value of `null` (0) indicates coroutine creation failed.

## co_delete

```c
void co_delete(co_routine_t *handle);
```

Delete specified coroutine.

`null` (0) or invalid coroutine handle is not allowed.

Passing handle of active coroutine to this function is not allowed.

Passing handle of primary coroutine is not allowed.

## co_serializable

```c
int co_serializable(void);
```

Returns non-zero if the implementation keeps the entire coroutine state in the
buffer passed to `co_derive()`. That is, if `co_serializable()` returns
non-zero, and if your coroutine does not modify the heap or any process-wide
state, then you can "snapshot" the coroutine's state by taking a copy of the
buffer originally passed to `co_derive()`, and "restore" a previous state
by copying the snapshot back into the buffer it came from.

## co_switch

```c
void co_switch(co_routine_t *handle);
```

Switch to specified coroutine.

`null` (0) or invalid coroutine handle is not allowed.

Passing handle of active coroutine to this function is not allowed.
