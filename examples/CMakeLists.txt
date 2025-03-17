cmake_minimum_required(VERSION 2.8...3.14)

add_subdirectory(echo-server)

set(TARGET_LIST child co_uv_fs co_uv_stream co_uv_listen co_uv_connect co_uv_spawn go_readfile)
foreach (TARGET ${TARGET_LIST})
    add_executable(${TARGET} ${TARGET}.c)
    target_link_libraries(${TARGET} uv_coro)
endforeach()
