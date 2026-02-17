#pragma once

#include <string>

namespace backend {

enum class TaskType {
  Tcp,
  Udp,
  Timer,
  Disk,
  Log
};

struct GenericTask {
  TaskType type;
};

struct LogTask {
  std::string payload;
};

}  // namespace backend

