#include "../include/coroutine.h"

void handleClient(uv_stream_t *socket) {
    string data = stream_read(socket);
    printf("Coroutine named: %s, received following request: %s\n", co_get_name(), data);
    printf("Coroutine named: %s, sent status: %d, byte size: %zu\n\n", co_get_name(), stream_write(socket, data), strlen(data));
}

int co_main(int argc, char *argv[]) {
    uv_stream_t *socket = stream_bind(
        (argc > 0 && !is_empty(argv[1]) && is_str_eq(argv[1], "-s"))
        ? "https://127.0.0.1:9010"
        : "http://127.0.0.1:9010",
        0);

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
