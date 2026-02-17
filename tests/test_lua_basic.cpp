#include <gtest/gtest.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

TEST(LuaBasic, CreateStateAndExecuteChunk) {
  lua_State* state = luaL_newstate();
  ASSERT_NE(state, nullptr);
  luaL_openlibs(state);
  int result = luaL_dostring(state, "return 1 + 1");
  ASSERT_EQ(result, LUA_OK);
  int value = static_cast<int>(lua_tointeger(state, -1));
  ASSERT_EQ(value, 2);
  lua_pop(state, 1);
  lua_close(state);
}

