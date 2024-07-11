/*=====================================================================
LuaScript.h
-----------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include <utils/Exception.h>
#include <Luau/Location.h>
#include <string>
#include <vector>
#include <limits>
class LuaVM;
class LuaScript;
struct lua_State;
typedef int (*lua_CFunction)(lua_State* L);


struct LuaCFunction
{
	LuaCFunction() {}
	LuaCFunction(lua_CFunction func_, const std::string& func_name_) : func(func_), func_name(func_name_) {}

	lua_CFunction func;
	std::string func_name;
};


class LuaScriptOutputHandler
{
public:
	virtual void printFromLuaScript(LuaScript* script, const char* s, size_t len) {}

	virtual void errorOccurredFromLuaScript(LuaScript* script, const std::string& msg) {}
};


struct LuaScriptOptions
{
	LuaScriptOptions() : max_num_interrupts(std::numeric_limits<size_t>::max()), script_output_handler(NULL), userdata(NULL) {}

	size_t max_num_interrupts;

	std::vector<LuaCFunction> c_funcs;

	LuaScriptOutputHandler* script_output_handler;
	
	void* userdata;
};


class LuaScriptParseError
{
public:
	LuaScriptParseError(const std::string& msg_, Luau::Location location_) : msg(msg_), location(location_) {}

	std::string msg;
	Luau::Location location;
};


class LuaScriptExcepWithLocation : public glare::Exception
{
public:
	LuaScriptExcepWithLocation(const std::string& msg_) : glare::Exception(msg_) {}

	std::string messageWithLocations();
	
	std::vector<LuaScriptParseError> errors;
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

	void exec(); // Excecute top-level Lua code.

	// An exception is thrown if num_interrupts >= options.max_num_interrupts.  
	// This function can be called before executing some Lua code in this script. (e.g. before calling a Lua function)
	void resetExecutionTimeCounter() { num_interrupts = 0; }

	LuaVM* lua_vm;

	lua_State* thread_state;
	int thread_ref;

	size_t num_interrupts;
	LuaScriptOptions options;

	LuaScriptOutputHandler* script_output_handler;

	void* userdata;

	int Vec3dMetaTable_ref;
};
