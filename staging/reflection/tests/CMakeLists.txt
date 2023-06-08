include_directories("${CMAKE_SOURCE_DIR}/lib/include")

add_executable(test_get_struct_info test_get_struct_info.c)

add_test(mkcreflect test_get_struct_info)
