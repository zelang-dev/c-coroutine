#include "coroutine.h"

int div_err(int x, int y) {
    return x / y;
}

int mul(int x, int y) {
    return x * y;
}

void func(void *arg) {
    if (co_catch(co_message()))
        printf("panic occurred: %s\n", co_message());
}

void divByZero(void *arg) {
    co_recover(func, arg);
    printf("%d", div_err(1, 0));
}

int co_main(int argc, char **argv) {
    launch(divByZero, NULL);
    printf("Although panicked. We recovered. We call mul() func\n");
    printf("mul func result: %d\n", mul(5, 10));
    return 0;
}
