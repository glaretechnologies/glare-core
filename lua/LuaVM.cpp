/*=====================================================================
LuaVM.cpp
---------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "LuaVM.h"


#include "LuaScript.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../maths/mathstypes.h"
#include <lualib.h>
#include <Luau/Common.h>
#include <limits>


static void* glareLuaAlloc(void* user_data, void* ptr, size_t original_size, size_t new_size)
{
	LuaVM* lua_vm = (LuaVM*)user_data;

	if(new_size == 0)
	{
		lua_vm->total_allocated -= (int64)original_size;

		free(ptr);
		return NULL;
	}
	else
	{
		if(new_size != 0 && original_size == 0)
		{
			const int64 new_total_allocated = lua_vm->total_allocated + (int64)new_size;

			if(new_total_allocated > lua_vm->max_total_mem_allowed)
				throw glare::Exception("Tried to allocate more Lua memory than max total allowed amount of " + toString(lua_vm->max_total_mem_allowed) + " B");

			lua_vm->total_allocated = new_total_allocated;
			lua_vm->total_allocated_high_water_mark = myMax(lua_vm->total_allocated_high_water_mark, new_total_allocated);

			return malloc(new_size);
		}
		else
		{
			// Realloc behaviour
			const int64 new_total_allocated = lua_vm->total_allocated + ((int64)new_size - (int64)original_size);

			if(new_total_allocated > lua_vm->max_total_mem_allowed)
				throw glare::Exception("Tried to allocate more Lua memory than max total allowed amount of " + toString(lua_vm->max_total_mem_allowed) + " B");

			lua_vm->total_allocated = new_total_allocated;
			lua_vm->total_allocated_high_water_mark = myMax(lua_vm->total_allocated_high_water_mark, new_total_allocated);

			void* res = realloc(ptr, new_size);
			return res;
		}
	}
}


static void glareLuaInterrupt(lua_State* state, int gc)
{
	LuaScript* script = (LuaScript*)lua_getthreaddata(state);

	if(script) // script can be null if we get an interrupt callback setting up the shared Lua VM or in lua_newthread, before lua_setthreaddata is called.
	{
		script->num_interrupts++;
		if(script->num_interrupts >= script->options.max_num_interrupts)
			throw glare::Exception("Max num interrupts reached, aborting script");
	}
}


// Adapted from luaB_print in C:\programming\luau\0.627\VM\src\lbaselib.cpp
// We will supply our own implementation so print() calls don't output to stdout.
static int glareLuaPrint(lua_State* L)
{
	LuaScript* script = (LuaScript*)lua_getthreaddata(L);
	int n = lua_gettop(L); // number of arguments
	LuaScriptOutputHandler* handler = script->script_output_handler;
	if(handler != NULL)
	{
		for (int i = 1; i <= n; i++)
		{
			size_t l;
			const char* s = luaL_tolstring(L, i, &l); // convert to string using __tostring et al
			if (i > 1)
				handler->printFromLuaScript(script, "\t", 1);
			handler->printFromLuaScript(script, s, l);
			lua_pop(L, 1); // pop result
		}
		//handler->printFromLuaScript(script, "\n", 1);
	}
	return 0; // Return number of results left on stack
}


LuaVM::LuaVM()
:	state(NULL),
	total_allocated(0),
	total_allocated_high_water_mark(0),
	max_total_mem_allowed(std::numeric_limits<int64>::max()),
	init_finished(false)
{
	// Set LuauRecursionLimit to 256.  The default value of 1000 is too high - get a stack overflow in some cases.  See https://github.com/luau-lang/luau/issues/1277
	Luau::FValue<int>* cur = Luau::FValue<int>::list;
	while(cur)
	{
		if(stringEqual(cur->name, "LuauRecursionLimit"))
			cur->value = 256;
		cur = cur->next;
	}

	try
	{
		state = lua_newstate(glareLuaAlloc, /*userdata=*/this);
		if(!state)
			throw glare::Exception("lua_newstate failed.");

		luaL_openlibs(state);

		// Override print.  We will supply our own implementation so print() calls don't output to stdout.
		lua_pushcfunction(state, glareLuaPrint, /*debugname=*/"glareLuaPrint");
		lua_setglobal(state, "print");

		lua_callbacks(state)->interrupt = glareLuaInterrupt;
	}
	catch(std::exception& e)
	{
		throw glare::Exception(e.what());
	}
}


LuaVM::~LuaVM()
{
	if(state)
		lua_close(state);
}


void LuaVM::finishInitAndSandbox()
{
	try
	{
		luaL_sandbox(state);
		init_finished = true;
	}
	catch(std::exception& e)
	{
		throw glare::Exception(e.what());
	}
}


void LuaVM::setCFunctionAsTableField(lua_CFunction fn, const char* debugname, int table_index, const char* field_key)
{
	lua_pushcfunction(state, fn, debugname);
	lua_rawsetfield(state, table_index, field_key); // pops value (function) from stack
}
