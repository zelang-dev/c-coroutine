cmake_minimum_required(VERSION 2.8...3.14)

add_subdirectory(echo-server)

set(TARGET_LIST child co_json co_future_wait test_delay chan_3 primes co_uv_fs co_uv_stream co_uv_listen co_uv_connect co_uv_spawn co_uv_url co_http_request co_http_response co_parse_http go_channel go_sleep go_select go_wait_group go_panic go_readfile go_multi_args test-fs benchmark)
foreach (TARGET ${TARGET_LIST})
    add_executable(${TARGET} ${TARGET}.c)
    target_link_libraries(${TARGET} coroutine)
endforeach()
