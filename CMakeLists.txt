cmake_minimum_required(VERSION 2.8...3.14)

project(c-coroutine LANGUAGES C)

set(C_STANDARD 89)

include(CMakeDependentOption)
cmake_dependent_option(CO_BUILD_TESTS
  "Build the unit tests when BUILD_TESTING is enabled and we are the root project" ON
  "BUILD_TESTING;CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

message("Generated with config types: ${CMAKE_CONFIGURATION_TYPES}")

set(CMAKE_CONFIGURATION_TYPES=Debug;Release)
set(BUILD_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/coroutine-built")

option(LIBUV_BUILD_SHARED "Build shared lib" OFF)
add_subdirectory(deps/libuv)

file(GLOB lib_files
    ${CMAKE_SOURCE_DIR}/src/*.c
)

add_definitions(-DCO_SCRAPE_SIZE=64)
add_definitions(-DHASH_INIT_CAPACITY=512)
add_library(coroutine STATIC ${lib_files})
target_link_libraries(coroutine PUBLIC uv_a)

add_subdirectory(deps/yyjson)
target_link_libraries(coroutine PUBLIC yyjson)

find_package(OpenSSL)
if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions(-DOPENSSL_USE_STATIC_LIBS=TRUE)
    add_definitions(-DOPENSSL_MSVC_STATIC_RT=TRUE)
    target_link_libraries(coroutine PUBLIC crypto)
    target_link_libraries(coroutine PUBLIC ssl)
else()
    add_definitions(-DLIBRESSL_APPS=OFF)
    add_definitions(-DLIBRESSL_TESTS=OFF)
    add_definitions(-DLIBRESSL_DISABLE_OVERRIDE_WINCRYPT_DEFINES_WARNING)
    include_directories(${CMAKE_SOURCE_DIR}/include)
    add_subdirectory(deps/libressl)
    target_link_libraries(coroutine PUBLIC crypto)
    target_link_libraries(coroutine PUBLIC ssl)
endif()

if(UNIX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -D CO_DEBUG -D CO_USE_VALGRIND ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fomit-frame-pointer -Wno-return-type")
    find_package(Threads)
    target_link_libraries(coroutine PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()

if(WIN32)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D CO_DEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /D__PTW32_STATIC_LIB")
    add_definitions(-D__PTW32_STATIC_LIB -D_CRT_SECURE_NO_DEPRECATE)
    add_subdirectory(deps/pthreads4w)
    target_link_libraries(coroutine PUBLIC libpthreadVSE3)
endif()

if(CO_BUILD_TESTS)
    add_subdirectory(examples)
endif()
