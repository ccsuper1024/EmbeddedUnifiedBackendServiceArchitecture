add_executable(backend_app_config_tests
  test_app_config.cpp
)

target_link_libraries(backend_app_config_tests
  PRIVATE
    backend_core
    gtest_main
)

add_test(
  NAME backend_app_config_tests
  COMMAND backend_app_config_tests
)

add_executable(backend_mpsc_queue_tests
  test_mpsc_queue.cpp
)

target_link_libraries(backend_mpsc_queue_tests
  PRIVATE
    backend_core
    gtest_main
)

add_test(
  NAME backend_mpsc_queue_tests
  COMMAND backend_mpsc_queue_tests
)
