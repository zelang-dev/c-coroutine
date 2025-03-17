cmake_minimum_required(VERSION 3.11...3.14)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

project(c-coroutine LANGUAGES C)

set(CMAKE_C_STANDARD 90)

include(CMakeDependentOption)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)
include(CTest)

message("Generated with config types: ${CMAKE_CONFIGURATION_TYPES}")

set(CMAKE_CONFIGURATION_TYPES=Debug;Release)
set(BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/built")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

find_package(raii QUIET)
if(NOT raii_FOUND)
    FetchContent_Declare(raii
        URL https://github.com/zelang-dev/c-raii/archive/refs/heads/main.zip
        URL_MD5 f6f69f4e72680ec1a68f16b8b0b600fb
    )
    FetchContent_MakeAvailable(raii)
endif()

find_package(libuv QUIET)
if(NOT libuv_FOUND)
    FetchContent_Declare(libuv
        URL https://github.com/libuv/libuv/archive/refs/tags/v1.49.2.zip
        URL_MD5 e637c8dd733d75d59a0570b01cef815a
    )
    FetchContent_MakeAvailable(libuv)
endif()

file(GLOB lib_files
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_library(uv_coro STATIC ${lib_files})
set_property(TARGET uv_coro PROPERTY POSITION_INDEPENDENT_CODE True)
target_link_libraries(uv_coro PUBLIC uv_a)
target_include_directories(uv_coro AFTER PUBLIC $<BUILD_INTERFACE:${RAII_INCLUDE_DIR} $<INSTALL_INTERFACE:${RAII_INCLUDE_DIR})
target_link_libraries(uv_coro PUBLIC raii)


find_package(LibreSSL QUIET)
if(NOT LibreSSL_FOUND)
    set(LibreSSL_FOUND 1)
    FetchContent_Declare(libressl
        URL https://github.com/libressl/portable/releases/download/v3.9.2/libressl-3.9.2.tar.gz
        URL_MD5 0c9eb9b9b0f4e76e44f0647b21575391
    )
    FetchContent_MakeAvailable(libressl)
endif()
target_compile_definitions(crypto PUBLIC LIBRESSL_TESTS=OFF LIBRESSL_APPS=OFF)
target_compile_definitions(ssl PUBLIC LIBRESSL_TESTS=OFF LIBRESSL_APPS=OFF)
target_compile_definitions(tls PUBLIC LIBRESSL_TESTS=OFF LIBRESSL_APPS=OFF)
target_include_directories(uv_coro PUBLIC $<BUILD_INTERFACE:${LibreSSL_INCLUDE_DIR} $<INSTALL_INTERFACE:${LibreSSL_INCLUDE_DIR})
target_link_libraries(uv_coro PUBLIC crypto)
target_link_libraries(uv_coro PUBLIC ssl)

target_include_directories(uv_coro
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(UNIX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -rdynamic -D USE_DEBUG -D USE_VALGRIND ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fomit-frame-pointer -Wno-return-type")
endif()

if(WIN32)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D USE_DEBUG ")
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE -DPTW32_STATIC_LIB)
    add_definitions("/wd4244 /wd4267 /wd4033 /wd4715 /wd4311")
endif()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    option(BUILD_EXAMPLES   "whether or not examples should be built" OFF)

    if(BUILD_EXAMPLES)
        enable_testing()
        add_subdirectory(examples)
    endif()
endif()

set(_fmt TGZ)
if(WIN32)
  set(_fmt ZIP)
endif()

set(CPACK_GENERATOR ${_fmt})
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_NUGET_COMPONENT_INSTALL ON)
set(CPACK_WIX_COMPONENT_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)
set(CPACK_VERBATIM_VARIABLES YES)

set(CPACK_PACKAGE_VENDOR "https://github.com/zelang-dev/c-coroutine")
set(CPACK_PACKAGE_VERSION 1.0.0)
include(CPack)

set(CMAKE_INSTALL_CONFIG_NAME ${CMAKE_BUILD_TYPE})
install(TARGETS ${uv_coro} DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
