#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace backend {

struct AppConfig {
  std::string node_name;
  std::string log_level;
  std::uint16_t tcp_port;
  int tcp_io_threads;
  int udp_io_threads;
  int worker_threads;
  int disk_threads;
  int log_threads;
  int timer_threads;
  std::size_t queue_size_io_to_worker;
  std::size_t queue_size_worker_to_io;
  std::size_t queue_size_worker_to_disk;
  std::size_t queue_size_worker_to_log;

  static AppConfig LoadFromFile(const std::string& path);
};

}  // namespace backend
