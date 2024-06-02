/*=====================================================================
LuaScript.cpp
-------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "LuaScript.h"


#include "LuaVM.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include <lualib.h>
#include <luacode.h>


LuaScript::LuaScript(LuaVM* lua_vm_, const LuaScriptOptions& options_, const std::string& script_src)
:	lua_vm(lua_vm_),
	thread_state(NULL),
	options(options_),
	num_interrupts(0),
	script_output_handler(options_.script_output_handler)
{
	try
	{
		thread_state = lua_newthread(lua_vm->state);
		if(!thread_state)
			throw glare::Exception("lua_newthread failed");

		thread_ref = lua_ref(lua_vm->state, /*stack index=*/-1); // Get reference to thread to prevent it being garbage collected, see https://github.com/luau-lang/luau/discussions/774 and https://github.com/luau-lang/luau/issues/247

		lua_setthreaddata(thread_state, this);

		luaL_sandboxthread(thread_state);

		lua_CompileOptions compile_options;
		compile_options.optimizationLevel = 1;
		compile_options.debugLevel = 1;
		compile_options.typeInfoLevel = 0;
		compile_options.coverageLevel = 0;
		compile_options.vectorLib = NULL;
		compile_options.vectorCtor = NULL;
		compile_options.vectorType = NULL;
		compile_options.mutableGlobals = NULL;

		size_t result_size = 0;
		char* bytecode_or_err_msg = luau_compile(script_src.c_str(), script_src.size(), &compile_options, &result_size);

		//conPrint("Result: ");
		//conPrint(std::string(bytecode, result_size));

		const std::string chunkname = "script";
		int result = luau_load(thread_state, chunkname.c_str(), bytecode_or_err_msg, result_size, /*env=*/0);

		free(bytecode_or_err_msg);

		if(result != 0)
		{
			// Try and get error from top of stack where luau_load puts it.
			size_t stringlen = 0;
			const char* str = lua_tolstring(thread_state, /*index=*/-1, &stringlen); // May return NULL if not a string
			if(str)
			{
				std::string error_string(str, stringlen);
				throw glare::Exception("luau_load failed: " + error_string);
			}
			else
				throw glare::Exception("luau_load failed.");
		}

		// TODO: Add c functions to shared global state instead?
		for(size_t i=0; i<options.c_funcs.size(); ++i)
		{
			lua_pushcfunction(thread_state, options.c_funcs[i].func, /*debugname=*/options.c_funcs[i].func_name.c_str());
			lua_setglobal(thread_state, options.c_funcs[i].func_name.c_str());
		}

		// Execute script main code
		lua_call(thread_state, /*nargs=*/0, LUA_MULTRET);
	}
	catch(std::exception& e)
	{
		throw glare::Exception(e.what());
	}
}


LuaScript::~LuaScript()
{
	if(thread_state)
		lua_unref(lua_vm->state, thread_ref);
}
