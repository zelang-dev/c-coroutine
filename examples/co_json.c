#include "coroutine.h"

int co_main(int argc, char **argv) {
    json_t *encoded = json_encode("si.s.v",
                                  kv("name", "John Smith"),
                                  kv("age", 25),
                                  kv("address.city", "Cupertino"),
                                  kv("contact.emails", json_decode(
                                      json_for("ss", "email@example.com", "email2@example.com"), false)
                                  ));

    if (is_json(encoded))
        puts(json_serialize(encoded, true));

    return 0;
}
