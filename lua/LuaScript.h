/*=====================================================================
LuaScript.h
-----------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "LuaVM.h" // For LuaCFunction
#include <utils/Exception.h>
#include <utils/Reference.h>
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
	LuaScriptOptions() : max_num_interrupts(std::numeric_limits<size_t>::max()), script_output_handler(NULL), userdata(NULL), find_calls_to_undefined_globals(true) {}

	size_t max_num_interrupts;

	// Name the script is known by in error messages, e.g. the path of the file it came from.  Luau puts it in front of the line
	// number of a runtime error, and messageWithLocations() puts it in front of a compile error.  "script" if left empty.
	std::string chunkname;

	std::vector<LuaCFunction> c_funcs;

	LuaScriptOutputHandler* script_output_handler;
	
	void* userdata;

	// If true, the script is checked for calls to undefined global functions when loaded.  exec() warns about them, and improveErrorMessage() names them.
	// Set to false to skip the check, e.g. for scripts whose output nobody will see.
	bool find_calls_to_undefined_globals;
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

	// If msg is an "attempt to call a nil value" error on a line that calls an undefined global function, returns msg with a note naming the function.
	// Otherwise returns msg.  Only uses the text of msg, as the Lua state may not be usable after an error.
	std::string improveErrorMessage(const std::string& msg) const;



	Reference<LuaVM> lua_vm;

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

	// Calls to global functions that are neither defined by the script nor globals in the VM when the script is loaded, such as a misspelt function name.
	// Used by improveErrorMessage().
	struct GlobalFuncCall
	{
		std::string name;
		int line; // 1-based
	};
	std::vector<GlobalFuncCall> calls_to_undefined_globals;
};
