/*=====================================================================
LuaVM.h
-------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
struct lua_State;


/*=====================================================================
LuaVM
-----

=====================================================================*/
class LuaVM
{
public:
	LuaVM();
	~LuaVM();

	lua_State* state;

	int64 total_allocated;
	int64 total_allocated_high_water_mark;
	int64 max_total_mem_allowed;
};
