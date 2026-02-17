#pragma once

#include "event.h"

#include <string>

struct lua_State;

namespace backend {

class LuaVm {
 public:
  explicit LuaVm(const std::string& script_path);
  ~LuaVm();

  bool Init();
  void HandleEvent(const Event& event);

 private:
  void CallHandler(const char* handler_name, const Event& event);
  static void PushEvent(lua_State* state, const Event& event);
  static int Lua_SendTcp(lua_State* state);
  static int Lua_SendUdp(lua_State* state);
  static int Lua_PostDiskTask(lua_State* state);
  static int Lua_CallExternalService(lua_State* state);

  std::string script_path_;
  lua_State* state_;
};

}  // namespace backend

