cmake_minimum_required(VERSION 3.11...3.14)

set(TARGET_LIST
 test-fs
 test-fs_close
 test-fs_open
 test-fs_read
 test-fs_write
#async_work_test.cc
#fs_test.cc
#http10.exe.cc
#loop_test.cc
#multicast.exe.cc
#name_resolution_test.cc
#promise_test.cc
#select_test.cc
#stream_test.cc
#tcp-broadcaster.exe.cc
#tcp_test.cc
#timer_test.cc
#udp_test.cc
#uds_test.cc
)

foreach (TARGET ${TARGET_LIST})
    add_executable(${TARGET} ${TARGET}.c )
    target_link_libraries(${TARGET} uv_coro)
    add_test(NAME ${TARGET} COMMAND ${TARGET} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach()
