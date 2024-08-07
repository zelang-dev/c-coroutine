#include <stdio.h>
#include <string.h>

#include <uv.h>

int main(int argc, char *argv[]) {
    fprintf(stderr, "\nThis is stderr\n");
    uv_sleep(25);
    printf("\nThis is stdout\n");
    if (argc > 0 && argv[1] != NULL) {
        if (strcmp(argv[1], "sleep") == 0) {
            printf("\tSleeping...\n");
            uv_sleep(1000);
            printf("\t\tExiting\n");
        } else {
            fprintf(stderr, "\twith \"%s\" passed!\n\n", argv[1]);
        }
    }

    return 0;
}
