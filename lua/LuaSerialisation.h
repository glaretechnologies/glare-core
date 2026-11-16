/*=====================================================================
LuaSerialisation.h
------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "LuaVM.h"
#include <utils/BufferOutStream.h>
#include <utils/BufferInStream.h>
#include <utils/BufferViewInStream.h>
#include <utils/HashMap.h>


/*=====================================================================
LuaSerialisation
----------------
=====================================================================*/
class LuaSerialisation
{
public:
	static constexpr size_t DEFAULT_MAX_SERIALISED_SIZE_B = 1000000;

	// Serialise a Lua object at the given stack index.
	// Leaves stack in same state as before execution.
	struct SerialisationOptions
	{
		SerialisationOptions() : max_depth(16), max_serialised_size_B(DEFAULT_MAX_SERIALISED_SIZE_B) {}
		int max_depth;
		size_t max_serialised_size_B;
	};
	static void serialise(lua_State* state, int stack_index, const SerialisationOptions& options, BufferOutStream& serialised_out);



	// Pushes deserialised lua value onto top of stack
	// metatable_uid_to_ref_map is a map from metatable UID (each metatable will have a UID value stored in it at 'uid'), to metatable reference.
	// This is used for setting the metatable of deserialised tables.
	struct DeserialisationOptions
	{
		DeserialisationOptions() : max_depth(16), max_table_entries(DEFAULT_MAX_SERIALISED_SIZE_B / 2) {}
		int max_depth; // Root value is at depth 0, as in serialise().
		size_t max_table_entries; // Each key/value pair requires at least two type tags.
	};
	static void deserialise(lua_State* state, HashMap<uint32, LuaVM::LuaValueRef>& metatable_uid_to_ref_map, BufferViewInStream& serialised_data,
		const DeserialisationOptions& options = DeserialisationOptions());
	
	static void test();
};
