#include "assertions.h"

string gai = "dns.google";
string gni = "8.8.8.8";

void_t worker_misc(params_t args) {
    ASSERT_WORKER(($size(args) > 1));
    sleepfor(args[0].u_int);
    ASSERT_WORKER(is_str_in("addrinfo, nameinfo", args[1].char_ptr));
    return args[1].char_ptr;
}

TEST(get_addrinfo) {
    dnsinfo_t *dns;
    rid_t res = go(worker_misc, 2, 200, "addrinfo");
    ASSERT_TRUE(is_type(dns = get_addrinfo(gai, "http", 1, kv(ai_flags, AI_CANONNAME | AI_PASSIVE | AF_INET)), UV_CORO_DNS));
    ASSERT_FALSE(result_is_ready(res));
    while (!result_is_ready(res))
        yielding();

    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "addrinfo");
    ASSERT_STR(gai, dns->ip_name);
    ASSERT_TRUE(is_str_in(dns->ip_addr, "8.8"));
    ASSERT_TRUE((dns->count > 2));

    return 0;
}

TEST(get_nameinfo) {
    nameinfo_t *dns;
    rid_t res = go(worker_misc, 2, 600, "nameinfo");
    ASSERT_TRUE(is_type(dns = get_nameinfo(gni, 443, 0), UV_CORO_NAME));
    ASSERT_FALSE(result_is_ready(res));
    while (!result_is_ready(res)) {
        yielding();
    }

    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "nameinfo");
    ASSERT_STR(gai, dns->host);
    ASSERT_STR("https", dns->service);

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(get_addrinfo);
    EXEC_TEST(get_nameinfo);

    return result;
}

int uv_main(int argc, char **argv) {
    TEST_FUNC(list());
}