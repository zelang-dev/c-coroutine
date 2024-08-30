#include "coroutine.h"

void *sendData(void *arg) {
    channel_t *ch = (channel_t *)arg;

    // data sent to the channel
    chan_send(ch, "Received. Send Operation Successful");
    puts("No receiver! Send Operation Blocked");

    return 0;
}

int co_main(int argc, char **argv) {
    // create channel
    channel_t *ch = channel();

    // function call with goroutine
    go(sendData, ch);
    // receive channel data
    printf("%s\n", chan_recv(ch).char_ptr);

    return 0;
}
