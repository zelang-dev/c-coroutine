
#include "coroutine.h"

void *greetings(void *arg) {
    args_t args = args_deferred(arg);
    char *name = args[0].char_ptr;
    int i;
    for (i = 0; i < 3; i++) {
        printf("%d ==> %s\n", i, name);
        sleep_for(1);
    }
    return 0;
}

int co_main(int argc, char **argv) {
    puts("Start of main Goroutine");
    go(greetings, args_ex(1, "John"));
    go(greetings, args_ex(1, "Mary"));
    sleep_for(1000);
    puts("\nEnd of main Goroutine");
    return 0;
}
