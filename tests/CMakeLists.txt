cmake_minimum_required(VERSION 3.11...3.14)

set(TARGET_LIST
 test-fs
 test-dns
#fs_test.cc
#stream_test.cc
#tcp_test.cc
#udp_test.cc
#uds_test.cc
)

foreach (TARGET ${TARGET_LIST})
    add_executable(${TARGET} ${TARGET}.c )
    target_link_libraries(${TARGET} uv_coro)
    add_test(NAME ${TARGET} COMMAND ${TARGET} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach()
