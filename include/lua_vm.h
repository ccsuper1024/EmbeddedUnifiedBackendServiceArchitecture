#pragma once

#include "event.h"
#include "mpsc_queue.h"
#include "tasks.h"

#include <string>

struct lua_State;

namespace backend {

class LuaVm {
 public:
  LuaVm(const std::string& script_path,
        MpscQueue<GenericTask>* to_io,
        MpscQueue<DiskTask>* to_disk,
        MpscQueue<LogTask>* to_log,
        int worker_index);
  ~LuaVm();

  bool Init();
  void HandleEvent(const Event& event);
  void RestoreState(const std::string& name, const std::string& data);

 private:
  void CallHandler(const char* handler_name, const Event& event);
  static void PushEvent(lua_State* state, const Event& event);
  static int Lua_SendTcp(lua_State* state);
  static int Lua_SendUdp(lua_State* state);
  static int Lua_PostDiskTask(lua_State* state);
  static int Lua_CallExternalService(lua_State* state);
  static int Lua_Log(lua_State* state);
  static int Lua_PersistState(lua_State* state);

  std::string script_path_;
  lua_State* state_;
  MpscQueue<GenericTask>* to_io_;
  MpscQueue<DiskTask>* to_disk_;
  MpscQueue<LogTask>* to_log_;
  int worker_index_;
};

}  // namespace backend
