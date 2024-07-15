/*=====================================================================
LuaSerialisation.h
------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include <utils/BufferOutStream.h>
#include <utils/BufferInStream.h>
#include <utils/BufferViewInStream.h>
#include <utils/HashMap.h>
struct lua_State;


/*=====================================================================
LuaSerialisation
----------------
=====================================================================*/
class LuaSerialisation
{
public:
	// Serialise a Lua object at the given stack index.
	// Leaves stack in same state as before execution.
	struct SerialisationOptions
	{
		SerialisationOptions() : max_depth(16), max_serialised_size_B(1000000) {}
		int max_depth;
		size_t max_serialised_size_B;
	};
	static void serialise(lua_State* state, int stack_index, const SerialisationOptions& options, BufferOutStream& serialised_out);

	// Pushes deserialised lua value onto top of stack
	// metatable_uid_to_ref_map is a map from metatable UID (each metatable will have a UID value stored in it at 'uid'), to metatable reference.
	// This is used for setting the metatable of deserialised tables.
	static void deserialise(lua_State* state, HashMap<uint32, int>& metatable_uid_to_ref_map, BufferViewInStream& serialised_data);
	
	static void test();
};
