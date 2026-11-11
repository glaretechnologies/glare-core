/*=====================================================================
LuaScript.cpp
-------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "LuaScript.h"


#include "LuaVM.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include <lualib.h>
#include <Luau/Compiler.h>
#include "Luau/BytecodeBuilder.h"
#include "Luau/ParseResult.h"
#include <cassert>


LuaScript::LuaScript(LuaVM* lua_vm_, const LuaScriptOptions& options_, const std::string& script_src)
:	lua_vm(lua_vm_),
	thread_state(NULL),
	options(options_),
	num_interrupts(0),
	script_output_handler(options_.script_output_handler),
	userdata(options_.userdata)
{
	try
	{
		if(!lua_vm->init_finished)
			lua_vm->finishInitAndSandbox();

		this->Vec3dMetaTable_ref = lua_vm->Vec3dMetaTable_ref;
		this->Vec2MetaTable_ref  = lua_vm->Vec2MetaTable_ref;

		// conPrint("---------LuaScript::LuaScript()------------");
		thread_state = lua_newthread(lua_vm->state); // Creates a new thread, pushes it on the stack, and returns a pointer to a lua_State that represents this new thread
		if(!thread_state)
			throw glare::Exception("lua_newthread failed");

		thread_ref = lua_vm->createReferenceToLuaValue(lua_vm->state, /*stack index=*/-1); // Get reference to thread to prevent it being garbage collected, see https://github.com/luau-lang/luau/discussions/774 and https://github.com/luau-lang/luau/issues/247
		// lua_ref in Luau doesn't pop item from stack (see https://github.com/luau-lang/luau/issues/247#issuecomment-983042025)

		lua_pop(lua_vm->state, 1); // Pop thread from shared lua_vm stack.

		lua_setthreaddata(thread_state, this);

		luaL_sandboxthread(thread_state);

		Luau::CompileOptions compile_options;
		compile_options.optimizationLevel = 1;
		compile_options.debugLevel = 1;
		compile_options.typeInfoLevel = 0;
		compile_options.coverageLevel = 0;
		compile_options.vectorLib = NULL;
		compile_options.vectorCtor = "Vec3f";
		compile_options.vectorType = "Vec3f";
		compile_options.mutableGlobals = NULL;

		// Use C++ compilation API instead of C API so we can get error locations via the Luau::ParseError/Luau::ParseErrors exceptions.
		Luau::ParseOptions parse_options;
		Luau::BytecodeBuilder bytecode_builder(/*encoder*/NULL);

		Luau::compileOrThrow(bytecode_builder, script_src, compile_options, parse_options);

		const std::string bytecode = bytecode_builder.getBytecode();

		// Luau prints a chunkname starting with '@' as-is, and wraps any other one as [string "..."], so a name we were given gets
		// the prefix in order to read as a plain file:line: location.
		const std::string chunkname = options.chunkname.empty() ? std::string("script") : ("@" + options.chunkname);
		const int result = luau_load(thread_state, chunkname.c_str(), bytecode.c_str(), bytecode.size(), /*env=*/0);
		if(result != 0)
		{
			// Try and get error from top of stack where luau_load puts it.
			size_t stringlen = 0;
			const char* str = lua_tolstring(thread_state, /*index=*/-1, &stringlen); // May return NULL if not a string
			if(str)
				throw glare::Exception("luau_load failed: " + std::string(str, stringlen));
			else
				throw glare::Exception("luau_load failed.");
		}

		// TODO: Add c functions to shared global state instead?
		for(size_t i=0; i<options.c_funcs.size(); ++i)
		{
			lua_pushcfunction(thread_state, options.c_funcs[i].func, /*debugname=*/options.c_funcs[i].func_name.c_str());
			lua_setglobal(thread_state, options.c_funcs[i].func_name.c_str());
		}
	}
	catch(Luau::ParseErrors& e)
	{
		// If we throw an exception in the constructor, the destructor isn't called, so we need to make sure to release the thread reference.
		lua_vm->destroyReferenceToLuaValue(lua_vm->state, thread_ref);

		LuaScriptExcepWithLocation loc_excep(e.what());
		loc_excep.chunkname = options.chunkname;
		for(size_t i=0; i<e.getErrors().size(); ++i)
			loc_excep.errors.push_back(LuaScriptParseError(e.getErrors()[i].getMessage(), e.getErrors()[i].getLocation()));

		throw loc_excep;
	}
	catch(Luau::ParseError& e)
	{
		// If we throw an exception in the constructor, the destructor isn't called, so we need to make sure to release the thread reference.
		lua_vm->destroyReferenceToLuaValue(lua_vm->state, thread_ref);

		LuaScriptExcepWithLocation loc_excep(e.what());
		loc_excep.chunkname = options.chunkname;
		loc_excep.errors.push_back(LuaScriptParseError(e.getMessage(), e.getLocation()));

		throw loc_excep;
	}
	catch(glare::Exception& e)
	{
		// If we throw an exception in the constructor, the destructor isn't called, so we need to make sure to release the thread reference.
		lua_vm->destroyReferenceToLuaValue(lua_vm->state, thread_ref);

		throw e; // Re-throw
	}
	catch(std::exception& e)
	{
		// If we throw an exception in the constructor, the destructor isn't called, so we need to make sure to release the thread reference.
		lua_vm->destroyReferenceToLuaValue(lua_vm->state, thread_ref);

		throw glare::Exception(e.what());
	}
}


LuaScript::~LuaScript()
{
	// Remove all 'leaked' (unfreed) function references from the LuaVM.
	for(auto it = lua_func_references.begin(); it != lua_func_references.end(); ++it)
		lua_vm->destroyReferenceToLuaValue(lua_vm->state, *it);

	lua_vm->destroyReferenceToLuaValue(lua_vm->state, thread_ref);
}


void LuaScript::exec()
{
	try
	{
		// Execute script main code
		lua_call(thread_state, /*nargs=*/0, LUA_MULTRET);
	}
	catch(std::exception& e)
	{
		throw glare::Exception(e.what());
	}
}


LuaScript::LuaFuncRefAndPtr LuaScript::getRefToFunction(const char* func_name)
{
	lua_getglobal(thread_state, func_name); // Push function onto stack (or nil if it isn't defined)

	LuaFuncRefAndPtr res;
	assert(res.ref.ref == LUA_NOREF);
	res.func_ptr = nullptr;

	if(lua_isfunction(thread_state, -1)) // If function was defined
	{
		res.ref = lua_vm->createReferenceToLuaValue(thread_state, /*index=*/-1); // Get reference to func (does not pop)
		res.func_ptr = lua_topointer(thread_state, /*index=*/-1); // Get pointer to func (does not pop)

		// Add to tracking set for this LuaScript
		if(res.ref.ref > LUA_REFNIL)
			lua_func_references.insert(res.ref);
	}

	lua_pop(thread_state, 1);
	return res;
}


LuaVM::LuaValueRef LuaScript::getRefToFunctionOnStack(lua_State* calling_state, int stack_index)
{
	LuaVM::LuaValueRef res = lua_vm->createReferenceToLuaValue(calling_state, stack_index);

	// Add to tracking set for this LuaScript
	if(res.ref > LUA_REFNIL)
		lua_func_references.insert(res);

	return res;
}


void LuaScript::freeRefToFunction(LuaVM::LuaValueRef ref)
{
	if(ref.ref <= LUA_REFNIL)
		return;

	// Check and remove from tracking set for this LuaScript
	#ifndef NDEBUG
	if(lua_func_references.count(ref) == 0)
	{
		conPrint("============================ Error: LuaScript: tried to destroy a LuaValueRef that was not in lua_func_references (destroyed twice?) ============================");
		#if defined(_WIN32)
		__debugbreak();
		#else
		assert(false);
		#endif
	}
	#endif

	lua_func_references.erase(ref);


	lua_vm->destroyReferenceToLuaValue(thread_state, ref);
}


std::string LuaScriptExcepWithLocation::messageWithLocations()
{
	std::string msg;// = what();

	for(size_t i=0; i<errors.size(); ++i)
	{
		const std::string line = toString(errors[i].location.begin.line + 1);
		const std::string col  = toString(errors[i].location.begin.column + 1);

		// Name the script when we know what it's called, in the file:line:col form editors and tools expect.
		if(chunkname.empty())
			msg += "Line " + line + ", col " + col + ": " + errors[i].msg + "\n";
		else
			msg += chunkname + ":" + line + ":" + col + ": " + errors[i].msg + "\n";
	}
	return msg;
}
