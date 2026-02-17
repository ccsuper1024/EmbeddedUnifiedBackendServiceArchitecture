#pragma once

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace backend {

void InitLogger(const std::string& level);
std::shared_ptr<spdlog::logger> GetLogger();

}  // namespace backend
