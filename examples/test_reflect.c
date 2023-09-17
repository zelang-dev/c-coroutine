#include "../include/coroutine.h"

#include "test_assert.h"

int co_main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    as_reflect(kind, channel_t, NULL);
    ASSERT_UEQ((size_t)12, reflect_num_fields(kind));
    ASSERT_STR("channel_t", reflect_type_of(kind));
    ASSERT_UEQ(sizeof(channel_t), reflect_type_size(kind));
    size_t packed_size = sizeof(unsigned int) + sizeof(value_types) + sizeof(unsigned char *) + sizeof(unsigned int) + sizeof(unsigned int) + sizeof(unsigned int) + sizeof(unsigned int) + sizeof(bool) + sizeof(char *) + sizeof(msg_queue_t) + sizeof(msg_queue_t) + sizeof(co_value_t *);
    ASSERT_UEQ(packed_size, reflect_packed_size(kind));

    println(1, kind);

    return 0;
}
