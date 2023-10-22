#include "../include/coroutine.h"

void handleClient(uv_stream_t *socket) {
    string data = stream_read(socket);

    printf("Coroutine named: %s, received following request: %s\n\n", co_get_name(), data);

    if (strcmp(data, "exit") == 0) {
        // exit command will cause this script to quit out
        puts("exit command received");
        exit(0);
    } else if (strcmp(data, "hi") == 0) {
        // hi command
        // write back to the client a response.
        int status = stream_write(socket, "Hello, This is our command run!");
        printf("hi command received: status %d\n", status);
    } else {
    }
}

int co_main(int argc, char *argv[]) {
    uv_stream_t *socket = stream_bind("http://127.0.0.1:9010", 0);

    while (true) {
        uv_stream_t *connectedSocket = stream_listen(socket, 1024);
        if (connectedSocket == NULL) {
            printf("Invalid handle, got error code: %d\n", co_err_code());
            break;
        }

        stream_handler(handleClient, connectedSocket);
    }

    return 0;
}
