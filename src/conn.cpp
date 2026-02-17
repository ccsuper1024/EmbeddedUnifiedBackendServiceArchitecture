#include "conn.h"

namespace backend {

bool TcpConnTable::Add(const Conn& conn) {
  auto it = conns_.find(conn.fd);
  if (it != conns_.end()) {
    return false;
  }
  conns_.emplace(conn.fd, conn);
  return true;
}

void TcpConnTable::Remove(int fd) {
  conns_.erase(fd);
}

Conn* TcpConnTable::Find(int fd) {
  auto it = conns_.find(fd);
  if (it == conns_.end()) {
    return nullptr;
  }
  return &it->second;
}

UdpSession* UdpSessionTable::FindOrCreate(const std::string& ip,
                                          std::uint16_t port,
                                          ProtocolType protocol,
                                          std::uint64_t now_ms) {
  std::string key = ip + ":" + std::to_string(port);
  auto it = sessions_.find(key);
  if (it != sessions_.end()) {
    it->second.last_active_ms = now_ms;
    return &it->second;
  }
  UdpSession session;
  session.remote_ip = ip;
  session.remote_port = port;
  session.id = sessions_.size() + 1;
  session.protocol = protocol;
  session.last_active_ms = now_ms;
  auto result = sessions_.emplace(key, session);
  return &result.first->second;
}

UdpSession* UdpSessionTable::FindById(std::uint64_t id) {
  for (auto& kv : sessions_) {
    if (kv.second.id == id) {
      return &kv.second;
    }
  }
  return nullptr;
}

RtpSession* RtpSessionTable::FindOrCreate(std::uint32_t ssrc,
                                          std::uint64_t now_ms) {
  auto it = sessions_.find(ssrc);
  if (it != sessions_.end()) {
    it->second.last_active_ms = now_ms;
    return &it->second;
  }
  RtpSession session;
  session.ssrc = ssrc;
  session.id = sessions_.size() + 1;
  session.last_active_ms = now_ms;
  auto result = sessions_.emplace(ssrc, session);
  return &result.first->second;
}

}  // namespace backend
