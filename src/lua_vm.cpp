#include "lua_vm.h"

#include "logger.h"

#include <cstdint>
#include <string>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace backend {

LuaVm::LuaVm(const std::string& script_path,
             MpscQueue<GenericTask>* to_io,
             MpscQueue<GenericTask>* to_disk,
             MpscQueue<LogTask>* to_log,
             int worker_index)
    : script_path_(script_path),
      state_(nullptr),
      to_io_(to_io),
      to_disk_(to_disk),
      to_log_(to_log),
      worker_index_(worker_index) {
}

LuaVm::~LuaVm() {
  if (state_) {
    lua_close(state_);
    state_ = nullptr;
  }
}

bool LuaVm::Init() {
  auto logger = GetLogger();
  state_ = luaL_newstate();
  if (!state_) {
    logger->error("failed to create lua state");
    return false;
  }
  luaL_openlibs(state_);

  lua_pushlightuserdata(state_, this);
  lua_pushcclosure(state_, Lua_SendTcp, 1);
  lua_setglobal(state_, "cpp_send_tcp");

  lua_pushlightuserdata(state_, this);
  lua_pushcclosure(state_, Lua_SendUdp, 1);
  lua_setglobal(state_, "cpp_send_udp");

  lua_pushlightuserdata(state_, this);
  lua_pushcclosure(state_, Lua_PostDiskTask, 1);
  lua_setglobal(state_, "cpp_post_disk_task");

  lua_pushlightuserdata(state_, this);
  lua_pushcclosure(state_, Lua_CallExternalService, 1);
  lua_setglobal(state_, "cpp_call_external_service");

  lua_pushlightuserdata(state_, this);
  lua_pushcclosure(state_, Lua_Log, 1);
  lua_setglobal(state_, "cpp_log");

  if (luaL_dofile(state_, script_path_.c_str()) != LUA_OK) {
    const char* message = lua_tostring(state_, -1);
    std::string error_message = message ? message : "";
    logger->error("failed to load lua script {}: {}", script_path_, error_message);
    lua_pop(state_, 1);
    return false;
  }
  return true;
}

void LuaVm::HandleEvent(const Event& event) {
  if (!state_) {
    return;
  }
  const char* handler = nullptr;
  switch (event.protocol) {
    case ProtocolType::Tcp:
      handler = "lua_on_tcp_message";
      break;
    case ProtocolType::Udp:
      handler = "lua_on_udp_signal";
      break;
    case ProtocolType::Unknown:
      handler = "lua_on_timer";
      break;
    default:
      return;
  }
  CallHandler(handler, event);
}

void LuaVm::CallHandler(const char* handler_name, const Event& event) {
  auto logger = GetLogger();
  lua_getglobal(state_, handler_name);
  if (!lua_isfunction(state_, -1)) {
    lua_pop(state_, 1);
    return;
  }
  PushEvent(state_, event);
  if (lua_pcall(state_, 1, 0, 0) != LUA_OK) {
    const char* message = lua_tostring(state_, -1);
    std::string error_message = message ? message : "";
    logger->error("lua handler {} error: {}", handler_name, error_message);
    lua_pop(state_, 1);
  }
}

void LuaVm::PushEvent(lua_State* state, const Event& event) {
  lua_newtable(state);

  lua_pushstring(state, "protocol");
  lua_pushinteger(state, static_cast<lua_Integer>(static_cast<int>(event.protocol)));
  lua_settable(state, -3);

  lua_pushstring(state, "session_id");
  lua_pushinteger(state, static_cast<lua_Integer>(event.session_id));
  lua_settable(state, -3);

  lua_pushstring(state, "timestamp_ms");
  lua_pushinteger(state, static_cast<lua_Integer>(event.context.timestamp_ms));
  lua_settable(state, -3);

  lua_pushstring(state, "remote_ip");
  lua_pushlstring(state, event.context.remote_ip.data(),
                  event.context.remote_ip.size());
  lua_settable(state, -3);

  lua_pushstring(state, "remote_port");
  lua_pushinteger(state, static_cast<lua_Integer>(event.context.remote_port));
  lua_settable(state, -3);

  lua_pushstring(state, "payload");
  lua_pushlstring(state, event.payload.data(), event.payload.size());
  lua_settable(state, -3);
}

int LuaVm::Lua_SendTcp(lua_State* state) {
  int argument_count = lua_gettop(state);
  if (argument_count < 2) {
    lua_pushstring(state, "cpp_send_tcp expects session_id and payload");
    lua_error(state);
    return 0;
  }
  lua_Integer session_id = luaL_checkinteger(state, 1);
  std::size_t length = 0;
  const char* payload = luaL_checklstring(state, 2, &length);
  void* userdata = lua_touserdata(state, lua_upvalueindex(1));
  auto* self = static_cast<LuaVm*>(userdata);
  auto logger = GetLogger();
  logger->info("lua requested tcp send session_id={} size={}",
               static_cast<std::uint64_t>(session_id),
               static_cast<std::size_t>(length));
  if (self && self->to_io_) {
    GenericTask task;
    task.type = TaskType::Tcp;
    task.protocol = ProtocolType::Tcp;
    task.session_id = static_cast<std::uint64_t>(session_id);
    task.payload.assign(payload, length);
    self->to_io_->Push(std::move(task));
  }
  return 0;
}

int LuaVm::Lua_SendUdp(lua_State* state) {
  int argument_count = lua_gettop(state);
  if (argument_count < 2) {
    lua_pushstring(state, "cpp_send_udp expects session_id and payload");
    lua_error(state);
    return 0;
  }
  lua_Integer session_id = luaL_checkinteger(state, 1);
  std::size_t length = 0;
  const char* payload = luaL_checklstring(state, 2, &length);
  void* userdata = lua_touserdata(state, lua_upvalueindex(1));
  auto* self = static_cast<LuaVm*>(userdata);
  auto logger = GetLogger();
  logger->info("lua requested udp send session_id={} size={}",
               static_cast<std::uint64_t>(session_id),
               static_cast<std::size_t>(length));
  if (self && self->to_io_) {
    GenericTask task;
    task.type = TaskType::Udp;
    task.protocol = ProtocolType::Udp;
    task.session_id = static_cast<std::uint64_t>(session_id);
    task.payload.assign(payload, length);
    self->to_io_->Push(std::move(task));
  }
  return 0;
}

int LuaVm::Lua_PostDiskTask(lua_State* state) {
  int argument_count = lua_gettop(state);
  if (argument_count < 1) {
    lua_pushstring(state, "cpp_post_disk_task expects description");
    lua_error(state);
    return 0;
  }
  std::size_t length = 0;
  const char* description = luaL_checklstring(state, 1, &length);
  void* userdata = lua_touserdata(state, lua_upvalueindex(1));
  auto* self = static_cast<LuaVm*>(userdata);
  auto logger = GetLogger();
  logger->info("lua requested disk task description={} size={}",
               std::string(description, length),
               static_cast<std::size_t>(length));
  if (self && self->to_disk_) {
    GenericTask task;
    task.type = TaskType::Disk;
    task.protocol = ProtocolType::Unknown;
    task.session_id = 0;
    task.payload.assign(description, length);
    self->to_disk_->Push(std::move(task));
  }
  return 0;
}

int LuaVm::Lua_CallExternalService(lua_State* state) {
  int argument_count = lua_gettop(state);
  if (argument_count < 1) {
    lua_pushstring(state, "cpp_call_external_service expects description");
    lua_error(state);
    return 0;
  }
  std::size_t length = 0;
  const char* description = luaL_checklstring(state, 1, &length);
  void* userdata = lua_touserdata(state, lua_upvalueindex(1));
  auto* self = static_cast<LuaVm*>(userdata);
  auto logger = GetLogger();
  logger->info("lua requested external service description={} size={}",
               std::string(description, length),
               static_cast<std::size_t>(length));
  if (self && self->to_disk_) {
    GenericTask task;
    task.type = TaskType::Disk;
    task.protocol = ProtocolType::Unknown;
    task.session_id = 0;
    task.payload.assign(description, length);
    self->to_disk_->Push(std::move(task));
  }
  return 0;
}

int LuaVm::Lua_Log(lua_State* state) {
  int argument_count = lua_gettop(state);
  if (argument_count < 2) {
    lua_pushstring(state, "cpp_log expects level and message");
    lua_error(state);
    return 0;
  }
  const char* level = luaL_checkstring(state, 1);
  std::size_t len = 0;
  const char* message = luaL_checklstring(state, 2, &len);
  void* userdata = lua_touserdata(state, lua_upvalueindex(1));
  auto* self = static_cast<LuaVm*>(userdata);
  if (self && self->to_log_) {
    LogTask task;
    std::string lvl(level);
    if (lvl == "trace") task.level = LogTask::Level::Trace;
    else if (lvl == "debug") task.level = LogTask::Level::Debug;
    else if (lvl == "info") task.level = LogTask::Level::Info;
    else if (lvl == "warn") task.level = LogTask::Level::Warn;
    else if (lvl == "error") task.level = LogTask::Level::Error;
    else if (lvl == "critical") task.level = LogTask::Level::Critical;
    else task.level = LogTask::Level::Info;
    task.message.assign(message, len);
    self->to_log_->Push(std::move(task));
  }
  return 0;
}

}  // namespace backend
