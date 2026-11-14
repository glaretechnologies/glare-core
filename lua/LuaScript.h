/*=====================================================================
LuaScript.h
-----------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "LuaVM.h" // For LuaCFunction
#include <utils/Exception.h>
#include <Luau/Location.h>
#include <string>
#include <vector>
#include <limits>
#include <unordered_set>
class LuaScript;


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

	// Name the script is known by in error messages, e.g. the path of the file it came from.  Luau puts it in front of the line
	// number of a runtime error, and messageWithLocations() puts it in front of a compile error.  "script" if left empty.
	std::string chunkname;

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

	std::string chunkname; // See LuaScriptOptions::chunkname.  Left out of the message when empty.
	std::vector<LuaScriptParseError> errors;
};


/*=====================================================================
LuaScript
---------
A single Lua script.
We can have multiple Lua scripts running on one Lua VM.
=====================================================================*/
class LuaScript
{
public:
	LuaScript(LuaVM* lua_vm, const LuaScriptOptions& options, const std::string& script_src);
	~LuaScript();

	void exec(); // Execute top-level Lua code.

	// An exception is thrown if num_interrupts >= options.max_num_interrupts.  
	// This function can be called before executing some Lua code in this script. (e.g. before calling a Lua function)
	void resetExecutionTimeCounter() { num_interrupts = 0; }
	void addExecutionTimeToVMAndResetCounter()
	{
		lua_vm->total_num_interrupts += num_interrupts;
		num_interrupts = 0;
	}


	struct LuaFuncRefAndPtr
	{
		LuaVM::LuaValueRef ref;
		const void* func_ptr;
	};
	LuaFuncRefAndPtr getRefToFunction(const char* func_name); // Get a reference to a global function looked up by name

	LuaVM::LuaValueRef getRefToFunctionOnStack(lua_State* calling_state, int stack_index);

	// If no table field with the given key found, return LuaVM::LuaValueRef() (i.e. LUA_NOREF)
	LuaVM::LuaValueRef getOptionalRefToFunctionInTable(lua_State* calling_state, int table_index, const char* key);


	void freeRefToFunction(LuaVM::LuaValueRef ref);



	LuaVM* lua_vm;

	lua_State* thread_state;
	LuaVM::LuaValueRef thread_ref;

	size_t num_interrupts;
	LuaScriptOptions options;

	LuaScriptOutputHandler* script_output_handler;

	void* userdata;

	LuaVM::LuaValueRef Vec3dMetaTable_ref;
	LuaVM::LuaValueRef Vec2MetaTable_ref;

private:
	std::unordered_set<LuaVM::LuaValueRef, LuaVM::LuaValueRefHasher> lua_func_references;
};
