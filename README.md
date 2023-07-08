# c-coroutine

**c-coroutine** is a cooperative multithreading library written in C89. This library was initially a rework/refactor and merge of [libco](https://github.com/higan-emu/libco) with [minicoro](https://github.com/edubart/minicoro). These two differ among many [coru](https://github.com/geky/coru), [libdill](https://github.com/sustrik/libdill), [libmill](https://github.com/sustrik/libmill), [libwire](https://github.com/baruch/libwire), [libcoro](https://github.com/semistrict/libcoro), [libcsp](https://github.com/shiyanhui/libcsp), [dyco-coroutine](https://github.com/piaodazhu/dyco-coroutine), in that Windows is supported, and not using **ucontext**. That was until I came across [libtask](https://swtch.com/libtask), where the design is the underpinning of GoLang, and made it Windows compatible in an fork [symplely/libtask](https://github.com/symplely/libtask). **Libtask** has it's channel design origins from [Richard Beton's libcsp](https://libcsp.sourceforge.net/)

_This library is currently being build as fully `C` representation of GoLang `Go` **routine**. **PR** are welcome._

To be clear, this is a programming paradigm on structuring your code. Which can be implemented in whatever language of choice. So this is also the `C` representation of my purely PHP [symplely/coroutine](https://github.com/symplely/coroutine) library by way of `yield`.

> "The role of the language, is to take care of the mechanics of the async pattern and provide a natural bridge to a language-specific implementation." -[Microsoft](https://learn.microsoft.com/en-us/archive/msdn-magazine/2018/june/c-effective-async-with-coroutines-and-c-winrt).

You can read [Fibers, Oh My!](https://graphitemaster.github.io/fibers/) for a breakdown on how the actual context switch here is achieved by assembly. This library incorporates [libuv](http://docs.libuv.org) in a way that make providing callbacks unnecessary, same as in [Using C++ Resumable Functions with Libuv](https://devblogs.microsoft.com/cppblog/using-ibuv-with-c-resumable-functions/). **Libuv** is handling any hardware or multi-threading CPU access. This not necessary for library usage, the setup can be replaced with some other Event Loop library, or just disabled.

Two videos covering things to keep in mind about concurrency, [Building Scalable Deployments with Multiple Goroutines](https://www.youtube.com/watch?v=LNNaxHYFhw8) and [Detecting and Fixing Unbound Concurrency Problems](https://www.youtube.com/watch?v=gggi4GIvgrg).

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

<details>
<summary>The `Overhead` cost of switching coroutines, as compared to an ordinary `C` function call.</summary>

### Target - x86

* **Overhead:** ~5x
* **Supported processor(s):** 32-bit x86
* **Supported compiler(s):** any
* **Supported operating system(s):**
  * Windows
  * Mac OS X
  * Linux
  * BSD

### Target - amd64

* **Overhead:** ~10x (Windows), ~6x (all other platforms)
* **Supported processor(s):** 64-bit amd64
* **Supported compiler(s):** any
* **Supported operating system(s):**
  * Windows
  * Mac OS X
  * Linux
  * BSD

### Target - ppc

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

### Target - fiber

This uses Windows' "fibers" API.

* **Overhead:** ~15x
* **Supported processor(s):** Processor independent
* **Supported compiler(s):** any
* **Supported operating system(s):**
  * Windows

### Target - sjlj

This uses the C standard library's `setjump`/`longjmp` APIs.

* **Overhead:** ~30x
* **Supported processor(s):** Processor independent
* **Supported compiler(s):** any
* **Supported operating system(s):**
  * Mac OS X
  * Linux
  * BSD
  * Solaris

### Target - ucontext

This uses the POSIX "ucontext" API.

* **Overhead:** **~300x**
* **Supported processor(s):** Processor independent
* **Supported compiler(s):** any
* **Supported operating system(s):**
  * Linux
  * BSD

</details>

## Synopsis

```c
/* Write this function instead of main, this library provides its own main, the scheduler,
which will call this function as an coroutine! */
int co_main(int, char **);

/* Creates/initialize the next series/collection of coroutine's created to be part of wait group, same behavior of Go's waitGroups, but without passing struct or indicating when done.

All coroutines here behaves like regular functions, meaning they return values, and indicate a terminated/finish status.

The initialization ends when `co_wait()` is called, as such current coroutine will pause, and execution will begin for the group of coroutines, and wait for all to finished. */
C_API co_ht_group_t *co_wait_group(void);

/* Pauses current coroutine, and begin execution for given coroutine wait group object, will wait for all to finished.
Returns hast table of results, accessible by coroutine id. */
C_API co_ht_result_t co_wait(co_ht_group_t *);

/* Returns results of the given completed coroutine id, value in union value_t storage format. */
C_API value_t co_group_get_result(co_ht_result_t *, int);

/* Creates an unbuffered channel, similar to golang channels. */
C_API channel_t *co_make(void);

/* Creates an buffered channel of given element count,
similar to golang channels. */
C_API channel_t *co_make_buf(int);

/* Send data to the channel. */
C_API int co_send(channel_t *, void *);

/* Receive data from the channel. */
C_API value_t *co_recv(channel_t *);

/* The `for_select {` macro sets up a coroutine to wait on multiple channel operations.
Must be closed out with `} select_end;`, and if no `select_case(channel)`, `select_case_if(channel)`,
`select_break` provided, an infinite loop is created.

This behaves same as GoLang `select {}` statement.
*/
for_select {
    select_case(channel) {
        co_send(channel, void *data);
        // Or
        value_t *r = co_recv(channel);
    // Or
    } select_case_if(channel) {
        // co_send(channel); || co_recv(channel);

    /* The `select_default` is run if no other case is ready.
    Must also closed out with `select_break;`. */
    } select_default {
        // ...
    } select_break;
} select_end;

/* Creates an coroutine of given function with argument,
and add to schedular, same behavior as Go in golang. */
C_API int co_go(co_callable_t, void *);

/* Explicitly give up the CPU for at least ms milliseconds.
Other tasks continue to run during this time. */
C_API unsigned int co_sleep(unsigned int ms);

/* Call `CO_MALLOC` to allocate memory of given size in current coroutine,
will auto free `LIFO` on function exit/return, do not free! */
C_API void *co_new(size_t);

/* Call `CO_CALLOC` to allocate memory array of given count and size in current coroutine,
will auto free `LIFO` on function exit/return, do not free! */
C_API void *co_new_by(int count, size_t size);

/* Defer execution `LIFO` of given function with argument,
to when current coroutine exits/returns. */
C_API void co_defer(defer_func, void *);

/* Generic simple union storage types. */
typedef union
{
    int integer;
    signed int s_integer;
    long big_int;
    long long long_int;
    unsigned long long max_int;
    float point;
    double precision;
    bool boolean;
    unsigned char uchar;
    char *string;
    const char chars[512];
    char **array;
    void *object;
    co_callable_t function;
} value_t;

typedef struct co_value
{
    value_t value;
    enum value_types type;
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
#include "../include/coroutine.h"

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
#include "../include/coroutine.h"

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
#include "../include/coroutine.h"

int fibonacci(channel_t *c, channel_t *quit)
{
    int x = 0;
    int y = 1;
    select_if()
        select_case(c)
            co_send(c, &x);
            x = y;
            y = x + y;
        select_case_if(quit)
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

Another **Go** example from <https://gobyexample.com/waitgroups>

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

func worker(id int) {
    fmt.Printf("Worker %d starting\n", id)

    time.Sleep(time.Second)
    fmt.Printf("Worker %d done\n", id)
}

func main() {

    var wg sync.WaitGroup

    for i := 1; i <= 5; i++ {
        wg.Add(1)

        i := i

        go func() {
            defer wg.Done()
            worker(i)
        }()
    }

    wg.Wait()

}
```

This **C** library version of it, but with returning results.

```c
#include "../include/coroutine.h"

void *worker(void *arg)
{
    // int id = co_value(arg).integer;
    int id = co_id();
    printf("Worker %d starting\n", id);

    co_sleep(1000);
    printf("Worker %d done\n", id);
    if (id == 4)
        return (void *)32;
    else if (id == 3)
        return (void *)"hello world\0";

    return 0;
}

int co_main(int argc, char **argv)
{
    int cid[5];
    co_ht_group_t *wg = co_wait_group();
    for (int i = 1; i <= 5; i++)
    {
       cid[i-1] = co_go(worker, &i);
    }
    co_ht_result_t *wgr = co_wait(wg);

    printf("\nWorker # %d returned: %d\n", cid[2], co_group_get_result(wgr, cid[2]).integer);
    printf("\nWorker # %d returned: %s\n", cid[1], co_group_get_result(wgr, cid[1]).string);
    return 0;
}
```

<details>
<summary>The above will output, and the same goes for all compile builds in `Debug` mode.</summary>

```text
Running coroutine id: 1 () status: 3
Back at coroutine scheduling
Running coroutine id: 2 () status: 3
Worker 2 starting
Back at coroutine scheduling
Running coroutine id: 3 () status: 3
Worker 3 starting
Back at coroutine scheduling
Running coroutine id: 4 () status: 3
Worker 4 starting
Back at coroutine scheduling
Running coroutine id: 5 () status: 3
Worker 5 starting
Back at coroutine scheduling
Running coroutine id: 6 () status: 3
Worker 6 starting
Back at coroutine scheduling
Running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Running coroutine id: 7 () status: 3
Back at coroutine scheduling
Running coroutine id: 6 () status: 1
Worker 6 done
Back at coroutine scheduling
Running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Running coroutine id: 7 (coroutine_wait) status: 1
Back at coroutine scheduling
Running coroutine id: 5 () status: 1
Worker 5 done
Back at coroutine scheduling
Running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Running coroutine id: 7 (coroutine_wait) status: 1
Back at coroutine scheduling
Running coroutine id: 4 () status: 1
Worker 4 done
Back at coroutine scheduling
Running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Running coroutine id: 7 (coroutine_wait) status: 1
Back at coroutine scheduling
Running coroutine id: 3 () status: 1
Worker 3 done
Back at coroutine scheduling
Running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Running coroutine id: 7 (coroutine_wait) status: 1
Back at coroutine scheduling
Running coroutine id: 2 () status: 1
Worker 2 done
Back at coroutine scheduling
Running coroutine id: 1 (co_main) status: 1
Worker # 4 returned: 32

Worker # 3 returned: hello world
Back at coroutine scheduling
Coroutine scheduler exited
```

</details>

see [examples](https://github.com/symplely/c-coroutine/tree/main/examples) folder.

## Installation

## Todo

## Contributing

Contributions are encouraged and welcome; I am always happy to get feedback or pull requests on Github :) Create [Github Issues](https://github.com/symplely/c-coroutine/issues) for bugs and new features and comment on the ones you are interested in.

## License

The MIT License (MIT). Please see [License File](LICENSE.md) for more information.
