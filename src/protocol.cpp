#include "protocol.h"

#include <memory>
#include <unordered_map>

namespace backend {

namespace {

struct RegistryStorage {
  std::unordered_map<ProtocolType, std::shared_ptr<ProtocolHandler>> handlers;
};

RegistryStorage& GetStorage() {
  static RegistryStorage storage;
  return storage;
}

}  // namespace

ProtocolRegistry& ProtocolRegistry::Instance() {
  static ProtocolRegistry instance;
  return instance;
}

void ProtocolRegistry::RegisterHandler(ProtocolType type,
                                       std::shared_ptr<ProtocolHandler> handler) {
  auto& storage = GetStorage();
  storage.handlers[type] = std::move(handler);
}

std::shared_ptr<ProtocolHandler> ProtocolRegistry::GetHandler(ProtocolType type) const {
  const auto& storage = GetStorage();
  auto it = storage.handlers.find(type);
  if (it == storage.handlers.end()) {
    return nullptr;
  }
  return it->second;
}

}  // namespace backend

