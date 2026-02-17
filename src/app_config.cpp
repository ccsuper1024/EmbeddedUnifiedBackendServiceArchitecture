#include "app_config.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace backend {

namespace {

std::unordered_map<std::string, std::string> ParseKeyValues(const std::string& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    throw std::runtime_error("failed to open config file");
  }
  std::unordered_map<std::string, std::string> result;
  std::string line;
  while (std::getline(input, line)) {
    std::size_t pos = line.find('=');
    if (pos == std::string::npos) {
      continue;
    }
    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos + 1);
    std::size_t first = key.find_first_not_of(" \t\r\n");
    std::size_t last = key.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || last == std::string::npos) {
      continue;
    }
    key = key.substr(first, last - first + 1);
    first = value.find_first_not_of(" \t\r\n");
    last = value.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || last == std::string::npos) {
      value.clear();
    } else {
      value = value.substr(first, last - first + 1);
    }
    if (!key.empty()) {
      result[key] = value;
    }
  }
  return result;
}

std::uint16_t ToPort(const std::string& value, std::uint16_t fallback) {
  if (value.empty()) {
    return fallback;
  }
  std::uint64_t port = 0;
  std::istringstream iss(value);
  iss >> port;
  if (!iss || port == 0 || port > 65535) {
    return fallback;
  }
  return static_cast<std::uint16_t>(port);
}

int ToInt(const std::string& value, int fallback) {
  if (value.empty()) {
    return fallback;
  }
  int result = 0;
  std::istringstream iss(value);
  iss >> result;
  if (!iss) {
    return fallback;
  }
  if (result <= 0) {
    return fallback;
  }
  return result;
}

std::size_t ToSize(const std::string& value, std::size_t fallback) {
  if (value.empty()) {
    return fallback;
  }
  std::size_t result = 0;
  std::istringstream iss(value);
  iss >> result;
  if (!iss) {
    return fallback;
  }
  if (result == 0) {
    return fallback;
  }
  return result;
}

}  // namespace

AppConfig AppConfig::LoadFromFile(const std::string& path) {
  auto values = ParseKeyValues(path);
  AppConfig config;
  auto node_iter = values.find("node_name");
  if (node_iter != values.end()) {
    config.node_name = node_iter->second;
  } else {
    config.node_name = "embedded-node";
  }
  auto level_iter = values.find("log_level");
  if (level_iter != values.end()) {
    config.log_level = level_iter->second;
  } else {
    config.log_level = "info";
  }
  auto port_iter = values.find("tcp_port");
  if (port_iter != values.end()) {
    config.tcp_port = ToPort(port_iter->second, 9000);
  } else {
    config.tcp_port = 9000;
  }
  config.tcp_io_threads = ToInt(values["tcp_io_threads"], 4);
  config.udp_io_threads = ToInt(values["udp_io_threads"], 2);
  config.worker_threads = ToInt(values["worker_threads"], 8);
  config.disk_threads = ToInt(values["disk_threads"], 3);
  config.log_threads = ToInt(values["log_threads"], 1);
  config.timer_threads = ToInt(values["timer_threads"], 1);
  config.queue_size_io_to_worker = ToSize(values["queue_size_io_to_worker"], 65536);
  config.queue_size_worker_to_io = ToSize(values["queue_size_worker_to_io"], 65536);
  config.queue_size_worker_to_disk = ToSize(values["queue_size_worker_to_disk"], 16384);
  config.queue_size_worker_to_log = ToSize(values["queue_size_worker_to_log"], 16384);
  auto lua_script_iter = values.find("lua_main_script");
  if (lua_script_iter != values.end()) {
    config.lua_main_script = lua_script_iter->second;
  } else {
    config.lua_main_script = "scripts/main.lua";
  }
  return config;
}

}  // namespace backend
