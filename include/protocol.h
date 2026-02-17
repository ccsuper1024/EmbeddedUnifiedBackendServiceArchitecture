#pragma once

#include "event.h"

#include <cstddef>
#include <memory>

namespace backend {

class ProtocolHandler {
 public:
  virtual ~ProtocolHandler() = default;

  virtual bool Parse(const char* data, std::size_t length, Event& out_event) = 0;
  virtual bool Encode(const Event& event, std::string& out_bytes) = 0;
};

class ProtocolRegistry {
 public:
  static ProtocolRegistry& Instance();

  void RegisterHandler(ProtocolType type, std::shared_ptr<ProtocolHandler> handler);
  std::shared_ptr<ProtocolHandler> GetHandler(ProtocolType type) const;

 private:
  ProtocolRegistry() = default;
  ProtocolRegistry(const ProtocolRegistry&) = delete;
  ProtocolRegistry& operator=(const ProtocolRegistry&) = delete;
};

}  // namespace backend

