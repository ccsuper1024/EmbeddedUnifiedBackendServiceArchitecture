#pragma once

#include "event.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace backend {

enum class ConnState {
  Connecting = 0,
  Established = 1,
  Closing = 2,
  Closed = 3
};

struct Conn {
  int fd;
  ConnState state;
  int worker_index;
  ProtocolType protocol;
  std::string recv_buffer;
  std::string send_buffer;
  std::string remote_ip;
  std::uint16_t remote_port;
  std::uint64_t last_active_ms;
};

class TcpConnTable {
 public:
  bool Add(const Conn& conn);
  void Remove(int fd);
  Conn* Find(int fd);

 private:
  std::unordered_map<int, Conn> conns_;
};

struct UdpSession {
  std::string remote_ip;
  std::uint16_t remote_port;
  std::uint64_t id;
  ProtocolType protocol;
  std::uint64_t last_active_ms;
};

class UdpSessionTable {
 public:
  UdpSession* FindOrCreate(const std::string& ip,
                           std::uint16_t port,
                           ProtocolType protocol,
                           std::uint64_t now_ms);
  UdpSession* FindById(std::uint64_t id);

 private:
  std::unordered_map<std::string, UdpSession> sessions_;
};

struct RtpSession {
  std::uint32_t ssrc;
  std::uint64_t id;
  std::uint64_t last_active_ms;
};

class RtpSessionTable {
 public:
  RtpSession* FindOrCreate(std::uint32_t ssrc, std::uint64_t now_ms);

 private:
  std::unordered_map<std::uint32_t, RtpSession> sessions_;
};

}  // namespace backend
