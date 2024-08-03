/*
This benchmark based off **GoLang** version at https://github.com/pkolaczk/async-runtimes-benchmarks/blob/main/go/main.go

```go
package main

import (
    "fmt"
    "os"
    "strconv"
    "sync"
    "time"
)

func main() {
    numRoutines := 100000
    if len(os.Args) > 1 {
        n, err := strconv.Atoi(os.Args[1])
        if err == nil {
            numRoutines = n
        }
    }

    var wg sync.WaitGroup
    for i := 0; i < numRoutines; i++ {
    wg.Add(1)
    go func() {
        defer wg.Done()
        time.Sleep(10 * time.Second)
    }()
    }
    wg.Wait()
    fmt.Println("All goroutines finished.")
}
```
*/
#include "coroutine.h"

void *func(void *arg) {
    co_sleep(10 * time_Second);
    return 0;
}

int co_main(int argc, char **argv) {
    int numRoutines = 100000, i;
    if (argc > 1)
        numRoutines = atoi(argv[1]);

    wait_group_t *wg = co_wait_group();
    for (i = 1; i <= numRoutines; i++) {
        co_go(func, NULL);
    }
    co_wait(wg);

    printf("\nAll coroutines finished.\n");
    return 0;
}
