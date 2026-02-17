add_library(backend_core
  app_config.cpp
  logger.cpp
  runtime.cpp
  protocol.cpp
  conn.cpp
)

target_include_directories(backend_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(backend_core
  PUBLIC
    spdlog::spdlog
    pthread
)

add_executable(backend_app
  main.cpp
)

target_link_libraries(backend_app
  PRIVATE
    backend_core
)
