# c-coroutine

**c-coroutine** is a cooperative multithreading library written in C89. This library was initially a rework/refactor and merge of [libco](https://github.com/higan-emu/libco) with [minicoro](https://github.com/edubart/minicoro). These two differ among many [coru](https://github.com/geky/coru), [libdill](https://github.com/sustrik/libdill), [libmill](https://github.com/sustrik/libmill), [libwire](https://github.com/baruch/libwire), [libcoro](https://github.com/semistrict/libcoro), in that Windows is supported, and not using **ucontext**. That was until I came across [libtask](https://swtch.com/libtask), where the design is the underpinning of GoLang, and made it Windows compatible [libtask](https://github.com/symplely/libtask).

This library is currently being build as fully `C` representation of GoLang `Go` **routine**. **PR** are welcome.

To be clear, this is a programming paradigm on structuring your code. Which can be implemented in whatever language of choice. So this is also the `C` representation of my purely PHP [coroutine](https://github.com/symplely/coroutine) library by way of `yield`.

> "The role of the language, is to take care of the mechanics of the async pattern and provide a natural bridge to a language-specific implementation." -[Microsoft](https://learn.microsoft.com/en-us/archive/msdn-magazine/2018/june/c-effective-async-with-coroutines-and-c-winrt).

## Table of Contents

* [Introduction](#introduction)
* [Synopsis](#synopsis)
* [Usage](#usage)
* [Installation](#installation)
* [Todo](#todo)
* [Contributing](#contributing)
* [License](#license)

## Introduction

Although cooperative multithreading is limited to a single CPU core, it scales substantially better than preemptive multithreading.

For applications that need 100,000 or more context switches per second, the kernel overhead involved in preemptive multithreading can end up becoming the bottleneck in the application. Coroutines can easily scale to 10,000,000 or more context switches per second. Ideal use cases include servers (HTTP, RDBMS) and emulators (CPU cores, etc.)

The **libco** and **minicoro** library included _CPU_ backends for:

* x86, amd64, PowerPC, PowerPC64 ELFv1, PowerPC64 ELFv2, ARM 32-bit,  ARM 64-bit (AArch64), POSIX platforms (setjmp), Windows platforms (fibers)

In the following lists only _Windows and Linux_ targets been tested by **c-coroutine**, the others are reprints from **libco**. It is quite possible that this library will work on more processors, compilers and operating systems than those listed below.

The "Overhead" is the cost of switching coroutines, as compared to an ordinary `C` function call.

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

* **Overhead:** **~300x**
* **Supported processor(s):** Processor independent
* **Supported compiler(s):** any
* **Supported operating system(s):**
  * Linux
  * BSD

## Synopsis

```c
/* Write this function instead of main, this library provides its own main, the scheduler,
which will call this function as an coroutine! */
int co_main(int, char **);

/*
The `select_if()` macro sets up a coroutine to wait on multiple channel operations.
Must be closed out with `select_end()`, and if no `select_case(channel)`, `select_break()`
provided, an infinite loop is created.

This behaves same as GoLang `select {}` statement.
*/
select_if()
    select_case(channel_t channel)
        co_send(channel, void *s);
        // Or
        co_value_t *r = co_recv(channel);
    // Or
    select_case_if(channel)
        // co_send(); || co_recv();
    select_break()

    /* The `select_default()` is run if no other case is ready.
    Must also closed out with `select_break()`. */
    select_default()
select_end()

/* Creates an unbuffered channel, similar to golang channels. */
C_API channel_t *co_make(void);

/* Creates an buffered channel of given element count,
similar to golang channels. */
C_API channel_t *co_make_buf(int);

/* Send data to the channel. */
C_API int co_send(channel_t *, void *);

/* Receive data from the channel. */
C_API co_value_t *co_recv(channel_t *);

/* Creates an coroutine of given function with argument,
and add to schedular, same behavior as Go in golang. */
C_API int co_go(co_callable_t, void *);

/* Explicitly give up the CPU for at least ms milliseconds.
Other tasks continue to run during this time. */
C_API unsigned int co_sleep(unsigned int ms);

/* Allocate memory of given size in current coroutine,
will auto free on fuction exit/return, do not free! */
C_API void *co_new(size_t);

/* Defer execution of given function with argument,
to when current coroutine exits/returns. */
C_API void co_defer(defer_func, void *);

/* Generic simple union storage types. */
typedef union
{
    int integer;
    unsigned int u_integer;
    long big_int;
    unsigned long long max_int;
    float point;
    double precision;
    bool boolean;
    char *string;
    const char chars[512];
    char **array;
    void *object;
    co_callable_t function;
} value_t;

typedef struct co_value
{
    value_t value;
    unsigned int type;
    size_t s_args;
    size_t n_args;
} co_value_t;

/* Return an value in union type storage. */
C_API value_t co_value(void *);
```

The above is the **main** and most likely functions to be used, see [coroutine.h](https://github.com/symplely/c-coroutine/blob/main/coroutine.h) for additional.

> Note: None of the functions above require passing/handling the underlying `co_routine_t` object/structure.

## Usage

An **Go** example from <https://www.golinuxcloud.com/goroutines-golang/>

```go
package main

import (
   "fmt"
   "time"
)

func main() {
   fmt.Println("Start of main Goroutine")
   go greetings("John")
   go greetings("Mary")
   time.Sleep(time.Second * 10)
   fmt.Println("End of main Goroutine")

}

func greetings(name string) {
   for i := 0; i < 3; i++ {
       fmt.Println(i, "==>", name)
       time.Sleep(time.Millisecond)
  }
}
```

This **C** library version of it.

```c
#include "../coroutine.h"

void *greetings(void *arg)
{
    const char *name = co_value(arg).chars;
    for (int i = 0; i < 3; i++)
    {
        printf("%d ==> %s\n", i, name);
        co_sleep(1);
    }
    return 0;
}

int co_main(int argc, char **argv)
{
    puts("Start of main Goroutine");
    co_go(greetings, "John");
    co_go(greetings, "Mary");
    co_sleep(1000);
    puts("End of main Goroutine");
    return 0;
}
```

Another **Go** example from <https://www.programiz.com/golang/channel>

```go
package main
import "fmt"

func main() {

  // create channel
  ch := make(chan string)

  // function call with goroutine
  go sendData(ch)

  // receive channel data
  fmt.Println(<-ch)

}

func sendData(ch chan string) {

  // data sent to the channel
   ch <- "Received. Send Operation Successful"
   fmt.Println("No receiver! Send Operation Blocked")

}
```

This **C** library version of it.

```c
#include "../coroutine.h"

void *sendData(void *arg)
{
    channel_t *ch = (channel_t *)arg;

    // data sent to the channel
    co_send(ch, "Received. Send Operation Successful");
    puts("No receiver! Send Operation Blocked");

    return 0;
}

int co_main(int argc, char **argv)
{
    // create channel
    channel_t *ch = co_make();

    // function call with goroutine
    co_go(sendData, ch);
    // receive channel data
    printf("%s\n", co_recv(ch)->value.chars);

    return 0;
}
```

Another **Go** example from <https://go.dev/tour/concurrency/5>

```go
package main

import "fmt"

func fibonacci(c, quit chan int) {
    x, y := 0, 1
    for {
        select {
        case c <- x:
            x, y = y, x+y
        case <-quit:
            fmt.Println("quit")
            return
        }
    }
}

func main() {
    c := make(chan int)
    quit := make(chan int)
    go func() {
        for i := 0; i < 10; i++ {
            fmt.Println(<-c)
        }
        quit <- 0
    }()
    fibonacci(c, quit)
}
```

This **C** library version of it.

```c
#include "../coroutine.h"

int fibonacci(channel_t *c, channel_t *quit)
{
    int x = 0;
    int y = 1;
    select_if()
        select_case(c)
            co_send(c, &x);
            x = y;
            y = x + y;
        select_break()
        select_case(quit)
            co_recv(quit);
            puts("quit");
            return 0;
        select_break()
    select_end()
}

void *func(void *args)
{
    channel_t *c = ((channel_t **)args)[0];
    channel_t *quit = ((channel_t **)args)[1];
    for (int i = 0; i < 10; i++)
    {
        printf("%d\n", co_recv(c)->value.integer);
    }
    co_send(quit, 0);

    return 0;
}

int co_main(int argc, char **argv)
{
    channel_t *args[2];
    channel_t *c = co_make();
    channel_t *quit = co_make();

    args[0] = c;
    args[1] = quit;
    co_go(func, args);
    return fibonacci(c, quit);
}
```

see [examples](https://github.com/symplely/c-coroutine/tree/main/examples) folder.

## Installation

## Todo

## Contributing

Contributions are encouraged and welcome; I am always happy to get feedback or pull requests on Github :) Create [Github Issues](https://github.com/symplely/c-coroutine/issues) for bugs and new features and comment on the ones you are interested in.

## License

The MIT License (MIT). Please see [License File](LICENSE.md) for more information.
