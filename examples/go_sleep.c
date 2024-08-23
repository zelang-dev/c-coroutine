
#include "coroutine.h"

void *greetings(void *arg) {
    const char *name = c_const_char(arg);
    for (int i = 0; i < 3; i++) {
        printf("%d ==> %s\n", i, name);
        sleep_for(1);
    }
    return 0;
}

int co_main(int argc, char **argv) {
    puts("Start of main Goroutine");
    go(greetings, "John");
    go(greetings, "Mary");
    sleep_for(1000);
    puts("\nEnd of main Goroutine");
    return 0;
}
