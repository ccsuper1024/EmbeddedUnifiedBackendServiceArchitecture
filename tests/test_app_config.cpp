#include "app_config.h"

#include <fstream>
#include <string>

#include <gtest/gtest.h>

namespace {

std::string WriteTempConfig() {
  std::string path = "test_temp_app_config.cfg";
  std::ofstream output(path);
  output << "node_name=test-node" << '\n';
  output << "log_level=debug" << '\n';
  output << "tcp_port=12345" << '\n';
  output.close();
  return path;
}

}  // namespace

TEST(AppConfigTest, LoadFromFileParsesValues) {
  std::string path = WriteTempConfig();
  backend::AppConfig config = backend::AppConfig::LoadFromFile(path);
  EXPECT_EQ(config.node_name, "test-node");
  EXPECT_EQ(config.log_level, "debug");
  EXPECT_EQ(config.tcp_port, 12345);
  EXPECT_GT(config.tcp_io_threads, 0);
  EXPECT_GT(config.udp_io_threads, 0);
  EXPECT_GT(config.worker_threads, 0);
  EXPECT_GT(config.disk_threads, 0);
  EXPECT_GT(config.log_threads, 0);
  EXPECT_GT(config.timer_threads, 0);
  EXPECT_GT(config.queue_size_io_to_worker, 0u);
  EXPECT_GT(config.queue_size_worker_to_io, 0u);
  EXPECT_GT(config.queue_size_worker_to_disk, 0u);
  EXPECT_GT(config.queue_size_worker_to_log, 0u);
}
