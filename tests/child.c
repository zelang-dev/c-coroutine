#include <stdio.h>
#include <string.h>

#include <uv.h>

int main(int argc, char *argv[]) {
    fprintf(stderr, "\nThis is stderr\n");
    uv_sleep(25);
    printf("This is stdout\n");
    uv_sleep(25);
    printf("\tSleeping...\n");
    uv_sleep(500);
    printf("`%s` argument received\n", argv[1]);
    uv_sleep(25);
    fprintf(stderr, "Exiting\n");

    return 0;
}
