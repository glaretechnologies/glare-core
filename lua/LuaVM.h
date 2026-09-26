/*=====================================================================
LuaVM.h
-------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
#include <string>
#include <vector>
struct lua_State;
typedef int (*lua_CFunction)(lua_State* L);


struct LuaCFunction
{
	LuaCFunction() {}
	LuaCFunction(lua_CFunction func_, const std::string& func_name_) : func(func_), func_name(func_name_) {}

	lua_CFunction func;
	std::string func_name;
};


struct LuaVMOptions
{
	// Functions to set as globals on the VM, in addition to the built-in ones.  They are set after the built-ins and before the VM
	// is sandboxed, so unlike LuaScriptOptions::c_funcs they can replace a built-in such as print.
	std::vector<LuaCFunction> c_funcs;
};


/*=====================================================================
LuaVM
-----
A wrapper around lua_State.
=====================================================================*/
class LuaVM
{
public:
	LuaVM(const LuaVMOptions& options = LuaVMOptions());
	~LuaVM();

	static void staticInit();

	// Call once you have finished adding global functions
	void finishInitAndSandbox();

	// Assumes table is on top of stack.
	void setCFunctionAsTableField(lua_CFunction fn, const char* debugname, const char* field_key);

	lua_State* state;

	LuaVMOptions options; // Kept because Luau stores the debugname a function was registered with, without copying it.

	int64 total_allocated;
	int64 total_allocated_high_water_mark;
	int64 max_total_mem_allowed;

	bool init_finished;

	int Vec3dMetaTable_ref;

	static bool static_init_called;
};
