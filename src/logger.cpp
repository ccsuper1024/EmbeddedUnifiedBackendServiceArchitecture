#include "logger.h"

#include <memory>
#include <stdexcept>
#include <string>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace backend {

namespace {

std::shared_ptr<spdlog::logger> global_logger;

spdlog::level::level_enum ParseLevel(const std::string& level) {
  if (level == "trace") {
    return spdlog::level::trace;
  }
  if (level == "debug") {
    return spdlog::level::debug;
  }
  if (level == "info") {
    return spdlog::level::info;
  }
  if (level == "warn") {
    return spdlog::level::warn;
  }
  if (level == "error") {
    return spdlog::level::err;
  }
  if (level == "critical") {
    return spdlog::level::critical;
  }
  return spdlog::level::info;
}

}  // namespace

void InitLogger(const std::string& level) {
  if (!global_logger) {
    global_logger = spdlog::stdout_color_mt("backend");
  }
  global_logger->set_level(ParseLevel(level));
  global_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
}

std::shared_ptr<spdlog::logger> GetLogger() {
  if (!global_logger) {
    throw std::runtime_error("logger not initialized");
  }
  return global_logger;
}

}  // namespace backend

