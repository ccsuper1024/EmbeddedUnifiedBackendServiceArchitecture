#include "app_config.h"
#include "logger.h"
#include "runtime.h"

#include <exception>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
  std::string config_path = "config/app_config.cfg";
  if (argc > 1) {
    config_path = argv[1];
  }
  try {
    backend::AppConfig config = backend::AppConfig::LoadFromFile(config_path);
    backend::InitLogger(config.log_level);
    auto logger = backend::GetLogger();
    logger->info("backend starting");
    logger->info("node_name={}", config.node_name);
    logger->info("tcp_port={}", static_cast<int>(config.tcp_port));
    backend::Runtime runtime(config);
    runtime.Start();
    logger->info("runtime started");
    runtime.Stop();
    runtime.Join();
    logger->info("runtime stopped");
  } catch (const std::exception& ex) {
    std::cerr << "startup failed: " << ex.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "startup failed: unknown error" << std::endl;
    return 1;
  }
  return 0;
}
