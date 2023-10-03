cmake_minimum_required(VERSION 2.8...3.14)

project(c-coroutine LANGUAGES C)

set(C_STANDARD 89)

include(CMakeDependentOption)
cmake_dependent_option(CO_BUILD_TESTS
  "Build the unit tests when BUILD_TESTING is enabled and we are the root project" ON
  "BUILD_TESTING;CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

message("Generated with config types: ${CMAKE_CONFIGURATION_TYPES}")

set(BUILD_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/coroutine-built")

option(LIBUV_BUILD_SHARED "Build shared lib" OFF)
add_subdirectory(libuv)

file(GLOB lib_files
    ${CMAKE_SOURCE_DIR}/src/*.c
)

add_definitions(-DCO_STACK_SIZE=10240)
add_definitions(-DCO_MAIN_STACK=11264)
add_definitions(-DCO_SCRAPE_SIZE=64)
add_definitions(-DHASH_INIT_CAPACITY=512)
add_library(coroutine STATIC ${lib_files})
target_link_libraries(coroutine PUBLIC uv_a)

if(UNIX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -D CO_DEBUG -D CO_USE_VALGRIND ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fomit-frame-pointer ")
    find_package(Threads)
    target_link_libraries(coroutine PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()

if(WIN32)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D CO_DEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D__PTW32_STATIC_LIB")
    add_definitions(-D__PTW32_STATIC_LIB)
    add_subdirectory(pthreads4w)
    target_link_libraries(coroutine PUBLIC libpthreadVSE3)
endif()

if(CO_BUILD_TESTS)
    set(TARGET_LIST co_cpp_future_wait co_cpp_future test_delay chan_3 primes co_call_uv go_channel go_sleep go_select go_wait_group go_panic go_readfile)
    foreach (TARGET ${TARGET_LIST})
        add_executable(${TARGET} examples/${TARGET}.c)
        target_link_libraries(${TARGET} coroutine)
    endforeach()
endif()
