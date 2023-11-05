cmake_minimum_required (VERSION 2.8.11)
project(uv-tls-examples)

add_executable(test_tls
  test_tls.c
)

add_executable(test_tls_client
  test_tls_client.c
)

target_link_libraries(test_tls
  uv_a
  uv-tls
  ssl
  crypto
)

target_link_libraries(test_tls_client
  uv_a
  uv-tls
  ssl
  crypto
)
