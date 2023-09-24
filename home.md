# Manual

**c-coroutine** is a cooperative multithreading library written in C89. Designed to be simple as possible in usage, but powerfully enough in execution, easily modifiable to any need. It incorporates most asynchronous patterns from various languages. They all the same behaviorally, just syntax layout differences.

This library was initially a rework/refactor and merge of [libco](https://github.com/higan-emu/libco) with [minicoro](https://github.com/edubart/minicoro). These two differ among many [coru](https://github.com/geky/coru), [libdill](https://github.com/sustrik/libdill), [libmill](https://github.com/sustrik/libmill), [libwire](https://github.com/baruch/libwire), [libcoro](https://github.com/semistrict/libcoro), [libcsp](https://github.com/shiyanhui/libcsp), [dyco-coroutine](https://github.com/piaodazhu/dyco-coroutine), in that Windows is supported, and not using **ucontext**. That was until I came across [libtask](https://swtch.com/libtask), where the design is the underpinning of GoLang, and made it Windows compatible in an fork [symplely/libtask](https://github.com/symplely/libtask). **Libtask** has it's channel design origins from [Richard Beton's libcsp](https://libcsp.sourceforge.net/)

_This library is currently being build as fully `C` representation of GoLang `Go` **routine**. **PR** are welcome._

To be clear, this is a programming paradigm on structuring your code. Which can be implemented in whatever language of choice. So this is also the `C` representation of my purely PHP [coroutine](https://symplely.github.io/coroutine/) library by way of `yield`. The same way **Python** usage evolved, see [A Journey to Python Async](https://dev.to/uponthesky/python-a-journey-to-python-async-1-intro-4mgj).

> "The role of the language, is to take care of the mechanics of the async pattern and provide a natural bridge to a language-specific implementation." -[Microsoft](https://learn.microsoft.com/en-us/archive/msdn-magazine/2018/june/c-effective-async-with-coroutines-and-c-winrt).

You can read [Fibers, Oh My!](https://graphitemaster.github.io/fibers/) for a breakdown on how the actual context switch here is achieved by assembly. This library incorporates [libuv](http://docs.libuv.org) in a way that make providing callbacks unnecessary, same as in [Using C++ Resumable Functions with Libuv](https://devblogs.microsoft.com/cppblog/using-ibuv-with-c-resumable-functions/). **Libuv** is handling any hardware or multi-threading CPU access. This not necessary for library usage, the setup can be replaced with some other Event Loop library, or just disabled. There is a unmaintained [libasync](https://github.com/btrask/libasync) package tried combining **libco**, with **libuv** too, Linux only.

Two videos covering things to keep in mind about concurrency, [Building Scalable Deployments with Multiple Goroutines](https://www.youtube.com/watch?v=LNNaxHYFhw8) and [Detecting and Fixing Unbound Concurrency Problems](https://www.youtube.com/watch?v=gggi4GIvgrg).

## Table of Contents

* [Introduction](#introduction)
* [Synopsis](#synopsis)
* [Usage](#usage)
* [Installation](#installation)
* [Contributing](#contributing)
* [License](#license)

## Introduction

## Synopsis

```c
/* Write this function instead of main, this library provides its own main, the scheduler,
which call this function as an coroutine! */
int co_main(int, char **);

/* Calls fn (with args as arguments) in separated thread, returning without waiting
for the execution of fn to complete. The value returned by fn can be accessed through
 the future object returned (by calling `co_async_get()`). */
C_API future *co_async(callable_t, void_t);

/* Returns the value of a promise, a future thread's shared object, If not ready this
function blocks the calling thread and waits until it is ready. */
C_API value_t co_async_get(future *);

/* Waits for the future thread's state to change. this function pauses current coroutine
and execute others until future is ready, thread execution has ended. */
C_API void co_async_wait(future *);

/* Creates/initialize the next series/collection of coroutine's created to be part of wait group,
same behavior of Go's waitGroups, but without passing struct or indicating when done.

All coroutines here behaves like regular functions, meaning they return values, and indicate
a terminated/finish status.

The initialization ends when `co_wait()` is called, as such current coroutine will pause, and
execution will begin for the group of coroutines, and wait for all to finished. */
C_API wait_group_t *co_wait_group(void);

/* Pauses current coroutine, and begin execution for given coroutine wait group object, will
wait for all to finished. Returns hast table of results, accessible by coroutine id. */
C_API wait_result_t co_wait(wait_group_t *);

/* Returns results of the given completed coroutine id, value in union value_t storage format. */
C_API value_t co_group_get_result(wait_result_t *, int);

/* Creates an unbuffered channel, similar to golang channels. */
C_API channel_t *channel(void);

/* Creates an buffered channel of given element count,
similar to golang channels. */
C_API channel_t *channel_buf(int);

/* Send data to the channel. */
C_API int co_send(channel_t *, void_t);

/* Receive data from the channel. */
C_API value_t *co_recv(channel_t *);

/* The `for_select {` macro sets up a coroutine to wait on multiple channel operations.
Must be closed out with `} select_end;`, and if no `select_case(channel)`, `select_case_if(channel)`,
`select_break` provided, an infinite loop is created.

This behaves same as GoLang `select {}` statement.
*/
for_select {
    select_case(channel) {
        co_send(channel, void_t data);
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
C_API int co_go(callable_t, void_t);

/* Creates an coroutine of given function with argument, and immediately execute. */
C_API void co_execute(co_call_t, void_t);

/* Explicitly give up the CPU for at least ms milliseconds.
Other tasks continue to run during this time. */
C_API unsigned int co_sleep(unsigned int ms);

/* Call `CO_MALLOC` to allocate memory of given size in current coroutine,
will auto free `LIFO` on function exit/return, do not free! */
C_API void_t co_new(size_t);

/* Call `CO_CALLOC` to allocate memory array of given count and size in current coroutine,
will auto free `LIFO` on function exit/return, do not free! */
C_API void_t co_new_by(int count, size_t size);

/* Defer execution `LIFO` of given function with argument,
to when current coroutine exits/returns. */
C_API void co_defer(func_t, void_t);

/* An macro that stops the ordinary flow of control and begins panicking,
throws an exception of given message. */
co_panic(message);

/* Same as `defer` but allows recover from an Error condition throw/panic,
you must call `co_recover` to retrieve error message and mark Error condition handled. */
C_API void co_defer_recover(recover_func, void_t);

/* Generic simple union storage types. */
typedef union
{
    int integer;
    unsigned int u_int;
    signed long s_long;
    unsigned long u_long;
    long long long_long;
    size_t max_size;
    float point;
    double precision;
    bool boolean;
    signed short s_short;
    unsigned short u_short;
    signed char schar;
    unsigned char uchar;
    unsigned char *uchar_ptr;
    char *char_ptr;
    char **array;
    void_t object;
    callable_t func;
    const char str[512];
} value_t;

typedef struct co_value
{
    value_t value;
    value_types type;
} co_value_t;

/* Return an value in union type storage. */
C_API value_t co_value(void_t);
```

The above is the **main** and most likely functions to be used, see [coroutine.h](https://github.com/symplely/c-coroutine/blob/main/include/coroutine.h) for additional.

> Note: None of the functions above require passing/handling the underlying `co_routine_t` object/structure.

## Usage

Original **Go** example from <https://www.golinuxcloud.com/goroutines-golang/>

<table>
<tr>
<th>GoLang</th>
<th>C89</th>
</tr>
<tr>
<td>
<pre><code>
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
</code></pre>
</td>
<td>
<pre><code>
# include "../include/coroutine.h"

void_t greetings(void_t arg)
{
    const char *name = c_const_char(arg);
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
</code></pre>
</td>
</tr>
</table>

<details>
<summary>DEBUG run output</summary>

<pre>
Thread #7f11090f11c0 running coroutine id: 1 () status: 3
Start of main Goroutine
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 2 () status: 3
0 ==> John
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 3 () status: 3
0 ==> Mary
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 () status: 3
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 2
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 2 () status: 1
1 ==> John
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 3 () status: 1
1 ==> Mary
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 2
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 2 () status: 1
2 ==> John
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 3 () status: 1
2 ==> Mary
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 2
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 2 () status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 3 () status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f11090f11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
...
...
...
...
Thread #7f6ac8aa11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f6ac8aa11c0 running coroutine id: 4 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f6ac8aa11c0 running coroutine id: 1 (co_main) status: 1
End of main Goroutine
Back at coroutine scheduling
Coroutine scheduler exited
</pre>
</details>

Original **Go** example from <https://www.programiz.com/golang/channel>

<table>
<tr>
<th>GoLang</th>
<th>C89</th>
</tr>
<tr>
<td>
<pre><code>
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
</code></pre>
</td>
<td>
<pre><code>
#include "../include/coroutine.h"

void_t sendData(void_t arg) {
    channel_t *ch = (channel_t *)arg;

    // data sent to the channel
    co_send(ch, "Received. Send Operation Successful");
    puts("No receiver! Send Operation Blocked");

    return 0;
}

int co_main(int argc, char **argv) {
    // create channel
    channel_t *ch = channel();

    // function call with goroutine
    co_go(sendData, ch);
    // receive channel data
    printf("%s\n", co_recv(ch)->value.str);

    return 0;
}
</code></pre>
</td>
</tr>
</table>

<details>
<summary>DEBUG run output</summary>

<pre>
Thread #7f87171711c0 running coroutine id: 1 () status: 3
processed
 r:0x7fffb878dca0
Back at coroutine scheduling
Thread #7f87171711c0 running coroutine id: 2 () status: 3
processed
 s:0x7fffb878dca0*
 => s:0x7fffb878dca0
No receiver! Send Operation Blocked
Back at coroutine scheduling
Thread #7f87171711c0 running coroutine id: 1 (co_main) status: 1
Received. Send Operation Successful
Back at coroutine scheduling
Coroutine scheduler exited
</pre>
</details>

Original **Go** example from <https://go.dev/tour/concurrency/5>

<table>
<tr>
<th>GoLang</th>
<th>C89</th>
</tr>
<tr>
<td>
<pre><code>
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
</code></pre>
</td>
<td>
<pre><code>
#include "../include/coroutine.h"

int fibonacci(channel_t *c, channel_t *quit) {
    int x = 0;
    int y = 1;
    for_select {
        select_case(c) {
            co_send(c, &x);
            unsigned long tmp = x + y;
            x = y;
            y = tmp;
        } select_case_if(quit) {
            co_recv(quit);
            puts("quit");
            return 0;
        } select_break;
    } select_end;
}

void_t func(void_t args) {
    channel_t *c = ((channel_t **)args)[0];
    channel_t *quit = ((channel_t **)args)[1];

    for (int i = 0; i < 10; i++) {
        printf("%d\n", co_recv(c).integer);
    }
    co_send(quit, 0);

    return 0;
}

int co_main(int argc, char **argv) {
    channel_t *args[2];
    channel_t *c = channel();
    channel_t *quit = channel();

    args[0] = c;
    args[1] = quit;
    co_go(func, args);
    return fibonacci(c, quit);
}
</code></pre>
</td>
</tr>
</table>

<details>
<summary>DEBUG run output</summary>

<pre>
Thread #7f2dadbb11c0 running coroutine id: 1 () status: 3
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 3
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
0
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
1
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
1
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
2
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
3
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
5
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
8
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
13
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
21
processed
 r:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 s:0x7fffc1f35ca0*
 => s:0x7fffc1f35ca0
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
34
processed
 s:0x7fffc1f35f10
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 1 (co_main) status: 1
processed
 r:0x7fffc1f35f10*
 => r:0x7fffc1f35f10
quit
Back at coroutine scheduling
Thread #7f2dadbb11c0 running coroutine id: 2 () status: 1
Back at coroutine scheduling
Coroutine scheduler exited

</pre>
</details>

Original **Go** example from <https://www.developer.com/languages/go-error-handling-with-panic-recovery-and-defer/>

<table>
<tr>
<th>GoLang</th>
<th>C89</th>
</tr>
<tr>
<td>
<pre><code>
package main

import (
 "fmt"
 "log"
)

func main() {
 divByZero()
 fmt.Println("Although panicked. We recovered. We call mul() func")
 fmt.Println("mul func result: ", mul(5, 10))
}

func div(x, y int) int {
 return x / y
}

func mul(x, y int) int {
 return x * y
}

func divByZero() {
 defer func() {
  if err := recover(); err != nil {
   log.Println("panic occurred:", err)
  }
 }()
 fmt.Println(div(1, 0))
}
</code></pre>
</td>
<td>
<pre><code>
#include "../include/coroutine.h"

int div_err(int x, int y) {
    return x / y;
}

int mul(int x, int y) {
    return x * y;
}

void func(void_t arg) {
    const char *err = co_recover();
    if (NULL != err)
        printf("panic occurred: %s\n", err);
}

void divByZero(void_t arg) {
    co_defer_recover(func, arg);
    printf("%d", div_err(1, 0));
}

int co_main(int argc, char **argv) {
    co_execute(divByZero, NULL);
    printf("Although panicked. We recovered. We call mul() func\n");
    printf("mul func result: %d\n", mul(5, 10));
    return 0;
}
</code></pre>
</td>
</tr>
</table>

<details>
<summary>DEBUG run output</summary>

<pre>
Thread #7fd29c4011c0 running coroutine id: 1 () status: 3
Back at coroutine scheduling
Thread #7fd29c4011c0 running coroutine id: 2 () status: 3
panic occurred: sig_ill
Back at coroutine scheduling
Thread #7fd29c4011c0 running coroutine id: 1 (co_main) status: 1
Although panicked. We recovered. We call mul() func
mul func result: 50
Back at coroutine scheduling
Coroutine scheduler exited
</pre>
</details>

Original **Go** example from <https://gobyexample.com/waitgroups>

<table>
<tr>
<th>GoLang</th>
<th>C89</th>
</tr>
<tr>
<td>
<pre><code>
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
</code></pre>
</td>
<td>
<pre><code>
#include "../include/coroutine.h"

void_t worker(void_t arg) {
    // int id = c_int(arg);
    int id = co_id();
    printf("Worker %d starting\n", id);

    co_sleep(1000);
    printf("Worker %d done\n", id);
    if (id == 4)
        return (void_t)32;
    else if (id == 3)
        return (void_t)"hello world\0";

    return 0;
}

int co_main(int argc, char **argv)
{
    int cid[5];
    wait_group_t *wg = co_wait_group();
    for (int i = 1; i <= 5; i++) {
       cid[i-1] = co_go(worker, &i);
    }
    wait_result_t *wgr = co_wait(wg);

    printf("\nWorker # %d returned: %d\n",
           cid[2],
           co_group_get_result(wgr, cid[2]).integer);
    printf("\nWorker # %d returned: %s\n",
           cid[1],
           co_group_get_result(wgr, cid[1]).string);
    return 0;
}
</code></pre>
</td>
</tr>
</table>

<details>
<summary>DEBUG run output</summary>

<pre>
Thread #7f48f6c211c0 running coroutine id: 1 () status: 3
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 2 () status: 3
Worker 2 starting
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 3 () status: 3
Worker 3 starting
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 4 () status: 3
Worker 4 starting
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 5 () status: 3
Worker 5 starting
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 6 () status: 3
Worker 6 starting
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 7 () status: 3
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 6 () status: 1
Worker 6 done
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 7 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 5 () status: 1
Worker 5 done
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 7 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 4 () status: 1
Worker 4 done
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 7 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 3 () status: 1
Worker 3 done
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 7 (coroutine_wait) status: 1
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 2 () status: 1
Worker 2 done
Back at coroutine scheduling
Thread #7f48f6c211c0 running coroutine id: 1 (co_main) status: 1

Worker # 4 returned: 32

Worker # 3 returned: hello world
Back at coroutine scheduling
Coroutine scheduler exited
</pre>
</details>

The `C++ 20` concurrency **thread** model by way of **future/promise** implemented with same like _semantics_.

Original **C++ 20** example from <https://cplusplus.com/reference/future/future/wait/>

<table>
<tr>
<th>C++ 20</th>
<th>C89</th>
</tr>
<tr>
<td>
<pre><code>
// future::wait
#include <"iostream">       // std::cout
#include <"future">         // std::async, std::future
#include <"chrono">         // std::chrono::milliseconds

// a non-optimized way of checking for prime numbers:
bool is_prime (int x) {
  for (int i=2; i<x; ++i) if (x%i==0) return false;
  return true;
}

int main ()
{
  // call function asynchronously:
  std::future<"bool"> fut = std::async (is_prime,194232491);

  std::cout << "checking...\n";
  fut.wait();

  std::cout << "\n194232491 ";
  // guaranteed to be ready (and not block) after wait returns
  if (fut.get())
    std::cout << "is prime.\n";
  else
    std::cout << "is not prime.\n";

  return 0;
}
</code></pre>
</td>
<td>
<pre><code>
#include "../include/coroutine.h"

// a non-optimized way of checking for prime numbers:
void_t is_prime(void_t arg) {
    int x = c_int(arg);
    for (int i = 2; i < x; ++i)
        if (x % i == 0) return (void_t)false;
    return (void_t)true;
}

int co_main(int argc, char **argv) {
    int prime = 194232491;
    // call function asynchronously:
    future *fut = co_async(is_prime, &prime);

    printf("checking...\n");
    // Pause and run other coroutines
    // until thread state changes.
    co_async_wait(fut);

    printf("\n194232491 ");
    // guaranteed to be ready (and not block)
    // after wait returns
    if (co_async_get(fut).boolean)
        printf("is prime.\n");
    else
        printf("is not prime.\n");

    return 0;
}
</code></pre>
</td>
</tr>
</table>

<details>
<summary>DEBUG run output</summary>

<pre>
Thread #7fc61b3d11c0 running coroutine id: 1 () status: 3
promise id(706099430) created in thread #7fc61b3d11c0
thread #7fc61b3d11c0 created thread #7fc61b2b0700 with status(0) future id(706099430)
checking...
Back at coroutine scheduling
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 2
Back at coroutine scheduling
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
...
...
...
...
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
promise id(706099430) set LOCK in thread #7fc61b2b0700
promise id(706099430) set UNLOCK in thread #7fc61b2b0700
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1
Back at coroutine scheduling
Thread #7fc61b3d11c0 running coroutine id: 1 (co_main) status: 1

194232491 promise id(706099430) get LOCK in thread #7fc61b3d11c0
promise id(706099430) get UNLOCK in thread #7fc61b3d11c0
is prime!
Back at coroutine scheduling
Coroutine scheduler exited
</pre>
</details>

### See [examples](https://github.com/symplely/c-coroutine/tree/main/examples) folder for more

## Installation

## Contributing

Contributions are encouraged and welcome; I am always happy to get feedback or pull requests on Github :) Create [Github Issues](https://github.com/symplely/c-coroutine/issues) for bugs and new features and comment on the ones you are interested in.

## License

The MIT License (MIT). Please see [License File](LICENSE.md) for more information.
