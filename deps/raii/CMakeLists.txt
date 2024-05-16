cmake_minimum_required(VERSION 2.8...3.14)

project(c-raii LANGUAGES C)

set(C_STANDARD 89)

include(CMakeDependentOption)
include(GNUInstallDirs)
message("Generated with config types: ${CMAKE_CONFIGURATION_TYPES}")

set(CMAKE_CONFIGURATION_TYPES=Debug;Release)
set(BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/built")

cmake_dependent_option(BUILD_TESTING
  "Build the unit tests when BUILD_TESTING is enabled and we are the root project" OFF
  "BUILD_TESTING;CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)

option(BUILD_SHARED_LIBS    "Build the library as a shared (dynamically-linked) " OFF)

file(GLOB raii_files
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

if(BUILD_SHARED_LIBS)
    add_library(raii SHARED ${raii_files})
else()
    add_library(raii STATIC ${raii_files})
endif()

set_property(TARGET raii PROPERTY POSITION_INDEPENDENT_CODE True)

target_include_directories(raii PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(UNIX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -D USE_DEBUG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -fomit-frame-pointer -Wno-return-type")
endif()

if(WIN32)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D USE_DEBUG ")
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions("/wd4244 /wd4267 /wd4033 /wd4715")
endif()

find_package(Threads)
target_link_libraries(raii PUBLIC ${CMAKE_THREAD_LIBS_INIT})

if (BUILD_TESTING)
	enable_testing()
	add_subdirectory(tests)
	add_subdirectory(examples)
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

set(CPACK_PACKAGE_VENDOR "https://github.com/zelang-dev/c-raii")
set(CPACK_PACKAGE_VERSION 0.0.0)
include(CPack)

set(CMAKE_INSTALL_CONFIG_NAME ${CMAKE_BUILD_TYPE})
install(TARGETS ${raii} DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
