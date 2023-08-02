cmake_minimum_required(VERSION 3.14)

project(c-coroutine LANGUAGES C)

set(C_STANDARD 89)

include(CMakeDependentOption)
cmake_dependent_option(CO_BUILD_TESTS
  "Build the unit tests when BUILD_TESTING is enabled and we are the root project" ON
  "BUILD_TESTING;CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

message("Generated with config types: ${CMAKE_CONFIGURATION_TYPES}")

set(BUILD_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BUILD_DIR}/lib)

file(GLOB lib_files
    ${CMAKE_SOURCE_DIR}/src/*.c
)

add_library(coroutine STATIC ${lib_files})

if(UNIX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -D CO_DEBUG ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fomit-frame-pointer ")
    find_package(Threads)
    target_link_libraries(coroutine ${CMAKE_THREAD_LIBS_INIT} )
endif()

if(WIN32)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D CO_DEBUG")
    add_definitions(-D__PTW32_STATIC_LIB)
    add_subdirectory(pthreads4w)
    target_link_libraries(coroutine PUBLIC libpthreadVSE3)
endif()

option(LIBUV_BUILD_SHARED "Build shared lib" OFF)

add_subdirectory(libuv)
target_link_libraries(coroutine PUBLIC uv_a)

if(CO_BUILD_TESTS)
    set(TARGET_LIST co_cpp_future_wait co_cpp_future test_delay chan_3 primes co_call_uv go_channel go_sleep go_select go_wait_group go_panic)
    foreach (TARGET ${TARGET_LIST})
        add_executable(${TARGET} examples/${TARGET}.c)
        target_link_libraries(${TARGET} coroutine)
    endforeach()
endif()
