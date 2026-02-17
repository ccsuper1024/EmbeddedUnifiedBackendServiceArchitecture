#pragma once

#include <string>
#include <cstdint>

#include "event.h"

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
  ProtocolType protocol;
  std::uint64_t session_id;
  std::string payload;
};

struct LogTask {
  enum class Level {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical
  };
  Level level;
  std::string message;
};

}  // namespace backend
