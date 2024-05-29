/*=====================================================================
LuaScript.h
-----------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include <string>
#include <vector>
#include <limits>
class LuaVM;
struct lua_State;
typedef int (*lua_CFunction)(lua_State* L);

struct LuaCFunction
{
	LuaCFunction() {}
	LuaCFunction(lua_CFunction func_, const std::string& func_name_) : func(func_), func_name(func_name_) {}

	lua_CFunction func;
	std::string func_name;
};

struct LuaScriptOptions
{
	LuaScriptOptions() : max_num_interrupts(std::numeric_limits<size_t>::max()) {}

	size_t max_num_interrupts;

	std::vector<LuaCFunction> c_funcs;
};


/*=====================================================================
LuaScript
---------

=====================================================================*/
class LuaScript
{
public:
	LuaScript(LuaVM* lua_vm, const LuaScriptOptions& options, const std::string& script_src);
	~LuaScript();

	LuaVM* lua_vm;

	lua_State* thread_state;
	int thread_ref;

	size_t num_interrupts;
	LuaScriptOptions options;
};
