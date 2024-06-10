/*=====================================================================
LuaVM.h
-------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
struct lua_State;
typedef int (*lua_CFunction)(lua_State* L);


/*=====================================================================
LuaVM
-----

=====================================================================*/
class LuaVM
{
public:
	LuaVM();
	~LuaVM();

	// Call once you have finished adding global functions
	void finishInitAndSandbox();

	
	void setCFunctionAsTableField(lua_CFunction fn, const char* debugname, int table_index, const char* field_key);

	lua_State* state;

	int64 total_allocated;
	int64 total_allocated_high_water_mark;
	int64 max_total_mem_allowed;

	bool init_finished;
};
