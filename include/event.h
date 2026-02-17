#pragma once

#include <cstdint>
#include <string>

namespace backend {

enum class ProtocolType {
  Unknown = 0,
  Tcp = 1,
  Udp = 2,
  Rtp = 3
};

struct EventContext {
  std::uint64_t timestamp_ms;
  std::string remote_ip;
  std::uint16_t remote_port;
};

struct Event {
  ProtocolType protocol;
  std::uint64_t session_id;
  EventContext context;
  std::string payload;
};

}  // namespace backend

