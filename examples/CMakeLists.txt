cmake_minimum_required(VERSION 2.8...3.14)

add_subdirectory(echo-server)

set(TARGET_LIST benchmark)
foreach (TARGET ${TARGET_LIST})
    add_executable(${TARGET} ${TARGET}.c)
    target_link_libraries(${TARGET} coroutine)
endforeach()
