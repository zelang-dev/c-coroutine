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

# if CMake versions earlier than 3.14 need to be supported see https://cmake.org/cmake/help/v3.14/module/FetchContent.html
find_package(raii QUIET)
if(NOT raii_FOUND)
    FetchContent_Declare(raii
        URL https://github.com/zelang-dev/c-raii/archive/refs/tags/1.1.4.zip
        URL_MD5 e3e00ea554b590971c53818f8dbbcdf6
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

add_library(coroutine STATIC ${lib_files})
set_property(TARGET coroutine PROPERTY POSITION_INDEPENDENT_CODE True)
target_link_libraries(coroutine PUBLIC uv_a)

find_package(OpenSSL 0.9.8...<3.0.0)
if(OPENSSL_FOUND AND WIN32)
    target_include_directories(coroutine BEFORE PUBLIC ${OPENSSL_INCLUDE_DIR})
    add_definitions(-DOPENSSL_USE_STATIC_LIBS=TRUE)
    add_definitions(-DOPENSSL_MSVC_STATIC_RT=TRUE)
    add_definitions(-DNO_GETTIMEOFDAY)
    target_link_libraries(coroutine PUBLIC ${OPENSSL_CRYPTO_LIBRARY})
    target_link_libraries(coroutine PUBLIC ${OPENSSL_SSL_LIBRARY})
else()
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
    target_link_libraries(coroutine PUBLIC crypto)
    target_link_libraries(coroutine PUBLIC ssl)
endif()

target_include_directories(coroutine
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

target_include_directories(raii AFTER PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(coroutine PUBLIC raii)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    option(BUILD_EXAMPLES   "whether or not examples should be built" ON)

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
install(TARGETS ${coroutine} DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
