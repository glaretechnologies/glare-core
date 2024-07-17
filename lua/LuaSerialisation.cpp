/*=====================================================================
LuaSerialisation.cpp
--------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "LuaSerialisation.h"


#include <utils/Exception.h>
#include <utils/StringUtils.h>
#include <lualib.h>


static const uint8 GLARE_LUA_STRING		= 0;
static const uint8 GLARE_LUA_BOOL		= 1;
static const uint8 GLARE_LUA_NUMBER		= 2;
static const uint8 GLARE_LUA_TABLE		= 3;
static const uint8 GLARE_LUA_VECTOR		= 4;
static const uint8 GLARE_LUA_NIL		= 5;


static void serialiseValue(lua_State* state, int stack_index, int depth, const LuaSerialisation::SerialisationOptions& options, BufferOutStream& serialised);
static void deserialiseValue(lua_State* state, HashMap<uint32, int>& metatable_uid_to_ref_map, BufferViewInStream& serialised);


static void serialiseString(lua_State* state, int stack_index, const LuaSerialisation::SerialisationOptions& options, BufferOutStream& serialised)
{
	size_t len = 0;
	const char* data = lua_tolstring(state, stack_index, &len);

	if(serialised.getWriteIndex() + sizeof(uint8) + sizeof(uint32) + len > options.max_serialised_size_B)
		throw glare::Exception("Serialised data exceeded max_serialised_size_B");

	serialised.writeUInt8(GLARE_LUA_STRING);
	serialised.writeUInt32((uint32)len);
	serialised.writeData(data, len);
}


static void serialiseNil(lua_State* state, int stack_index, const LuaSerialisation::SerialisationOptions& options, BufferOutStream& serialised)
{
	serialised.writeUInt8(GLARE_LUA_NIL);
}


static void serialiseBool(lua_State* state, int stack_index, const LuaSerialisation::SerialisationOptions& options, BufferOutStream& serialised)
{
	if(serialised.getWriteIndex() + sizeof(uint8)*2 > options.max_serialised_size_B)
		throw glare::Exception("Serialised data exceeded max_serialised_size_B");

	serialised.writeUInt8(GLARE_LUA_BOOL);
	serialised.writeUInt8((uint8)lua_toboolean(state, stack_index));
}


static void serialiseNumber(lua_State* state, int stack_index, const LuaSerialisation::SerialisationOptions& options, BufferOutStream& serialised)
{
	if(serialised.getWriteIndex() + sizeof(uint8) + sizeof(double) > options.max_serialised_size_B)
		throw glare::Exception("Serialised data exceeded max_serialised_size_B");

	serialised.writeUInt8(GLARE_LUA_NUMBER);
	const double x = lua_tonumber(state, stack_index);
	serialised.writeData(&x, sizeof(double));
}


static void serialiseVector(lua_State* state, int stack_index, const LuaSerialisation::SerialisationOptions& options, BufferOutStream& serialised)
{
	if(serialised.getWriteIndex() + sizeof(uint8) + sizeof(float)*3 > options.max_serialised_size_B)
		throw glare::Exception("Serialised data exceeded max_serialised_size_B");

	serialised.writeUInt8(GLARE_LUA_VECTOR);
	const float* v = lua_tovector(state, stack_index);
	serialised.writeData(v, sizeof(float) * 3);
}


static void serialiseTable(lua_State* state, int table_stack_index, int depth, const LuaSerialisation::SerialisationOptions& options, BufferOutStream& serialised)
{
	if(serialised.getWriteIndex() + sizeof(uint8) + sizeof(uint32) * 2 > options.max_serialised_size_B)
		throw glare::Exception("Serialised data exceeded max_serialised_size_B");

	serialised.writeUInt8(GLARE_LUA_TABLE);

	table_stack_index = lua_absindex(state, table_stack_index); // Convert to positive index so index is still valid after pushing stuff onto stack.

	// Write metatable UID as uint32, or std::numeric_limits<uint32>::max() if no metatable.
	const int have_metatable = lua_getmetatable(state, table_stack_index); // Pushes onto stack if there.
	if(have_metatable)
	{
		const int uid_type = lua_rawgetfield(state, /*index=*/-1, "uid"); // Get metatable uid
		if(uid_type == LUA_TNUMBER)
		{
			const double uid_val = lua_tonumber(state, -1);
			serialised.writeUInt32((uint32)uid_val); // Write metatable UID
			lua_pop(state, 1); // pop UID
		}
		else
			throw glare::Exception("metatable uid was not a number");

		lua_pop(state, 1); // pop metatable off stack
	}
	else
		serialised.writeUInt32(std::numeric_limits<uint32>::max()); // Write 'no-metatable' metatable UID.


	if(!lua_checkstack(state, /*size=*/2)) // Make sure there is space for a key-value pair
		throw glare::Exception("Failed to alloc lua stack space");

	// Do a pass to get number of elements in table
	uint32 num_elems = 0;
	lua_pushnil(state); // Push first key onto stack
	while(1)
	{
		int notdone = lua_next(state, table_stack_index); // pops a key from the stack, and pushes a key-value pair from the table at the given index
		if(notdone == 0)
			break;

		num_elems++;
		lua_pop(state, 1); // Remove value, keep key on stack for next lua_next call
	}

	serialised.writeUInt32(num_elems); // Write number of elements

	// Now write each element
	lua_pushnil(state); // Push first key onto stack
	while(1)
	{
		int notdone = lua_next(state, table_stack_index); // pops a key from the stack, and pushes a key-value pair from the table at the given index
		if(notdone == 0)
			break;

		// Key is at -2, value is at -1
		serialiseValue(state, -2, depth + 1, options, serialised); // Serialise key
		serialiseValue(state, -1, depth + 1, options, serialised); // Serialise value
		
		lua_pop(state, 1); // Remove value, keep key on stack for next lua_next call
	}
}


static void serialiseValue(lua_State* state, int stack_index, int depth, const LuaSerialisation::SerialisationOptions& options, BufferOutStream& serialised)
{
	if(depth > options.max_depth)
		throw glare::Exception("Call depth exceeded while serialising Lua value.");

	const int t = lua_type(state, stack_index);
	switch (t)
	{
		case LUA_TNIL:
			serialiseNil(state, stack_index, options, serialised);
			break;
		case LUA_TSTRING:
			serialiseString(state, stack_index, options, serialised);
			break;
		case LUA_TBOOLEAN:
			serialiseBool(state, stack_index, options, serialised);
			break;
		case LUA_TNUMBER:
			serialiseNumber(state, stack_index, options, serialised);
			break;
		case LUA_TTABLE:
			serialiseTable(state, stack_index, depth, options, serialised);
			break;
		case LUA_TVECTOR:
			serialiseVector(state, stack_index, options, serialised);
			break;
		case LUA_TFUNCTION:
			throw glare::Exception("Can't serialise function");
		default:
			throw glare::Exception("Can't serialise type " + std::string(lua_typename(state, t)) + "'");
	}
}


static void deserialiseString(lua_State* state, BufferViewInStream& serialised)
{
	const uint32 len = serialised.readUInt32();

	if(!serialised.canReadNBytes(len))
		throw glare::Exception("Error while deserialising Lua string");

	if(!lua_checkstack(state, /*size=*/1)) // Make sure there is space for the string on the Lua stack
		throw glare::Exception("Failed to alloc lua stack space");

	lua_pushlstring(state, (const char*)serialised.currentReadPtr(), len);

	serialised.advanceReadIndex(len);
}


static void deserialiseBool(lua_State* state, BufferViewInStream& serialised)
{
	const uint8 val = serialised.readUInt8();

	if(!lua_checkstack(state, /*size=*/1)) // Make sure there is space for the bool on the Lua stack
		throw glare::Exception("Failed to alloc lua stack space");

	lua_pushboolean(state, (int)val);
}


static void deserialiseNumber(lua_State* state, BufferViewInStream& serialised)
{
	double x;
	serialised.readData(&x, sizeof(double));

	if(!lua_checkstack(state, /*size=*/1)) // Make sure there is space for the number on the Lua stack
		throw glare::Exception("Failed to alloc lua stack space");

	lua_pushnumber(state, x);
}


static void deserialiseVector(lua_State* state, BufferViewInStream& serialised)
{
	float v[3];
	serialised.readData(v, sizeof(float) * 3);

	if(!lua_checkstack(state, /*size=*/1)) // Make sure there is space for the vector on the Lua stack
		throw glare::Exception("Failed to alloc lua stack space");

	lua_pushvector(state, v[0], v[1], v[2]);
}


static void deserialiseNil(lua_State* state, BufferViewInStream& serialised)
{
	if(!lua_checkstack(state, /*size=*/1)) // Make sure there is space for the nil on the Lua stack
		throw glare::Exception("Failed to alloc lua stack space");

	lua_pushnil(state);
}


static void deserialiseTable(lua_State* state, HashMap<uint32, int>& metatable_uid_to_ref_map, BufferViewInStream& serialised)
{
	const uint32 metatable_uid = serialised.readUInt32();
	int metatable_ref = -1;
	if(metatable_uid != std::numeric_limits<uint32>::max())
	{
		// Table had a metatable
		auto res = metatable_uid_to_ref_map.find(metatable_uid);
		if(res == metatable_uid_to_ref_map.end())
			throw glare::Exception("Did not find info for metatable with uid " + toString(metatable_uid) + " in metatable_uid_to_ref_map.");

		metatable_ref = res->second;
	}

	// Read number of elements
	const uint32 num_elems = serialised.readUInt32();

	if(!lua_checkstack(state, /*size=*/2)) // Make sure there is space for the table and metatable
		throw glare::Exception("Failed to alloc lua stack space");

	lua_createtable(state, /*num array elems hint=*/num_elems, /*num other elems hint=*/num_elems);
	for(uint32 i=0; i<num_elems; ++i)
	{
		// Deserialise key to top of lua stack
		deserialiseValue(state, metatable_uid_to_ref_map, serialised);

		// Deserialise value to top of lua stack
		deserialiseValue(state, metatable_uid_to_ref_map, serialised);

		// Set as a table (key, value) pair
		// key is at -2, value is at -1, so table is at -3.
		lua_settable(state, /*table index=*/-3); // pops both the key and the value from the stack
	}

	// Set table metatable if there should be one
	if(metatable_uid != std::numeric_limits<uint32>::max())
	{
		lua_getref(state, metatable_ref); // Pushes metatable onto the stack.
		lua_setmetatable(state, -2); // "Pops a table from the stack and sets it as the new metatable for the value at the given acceptable index."
	}
}


// Pushes the deserialised Lua value onto the top of the Lua stack
static void deserialiseValue(lua_State* state, HashMap<uint32, int>& metatable_uid_to_ref_map, BufferViewInStream& serialised)
{
	const int t = serialised.readUInt8();
	switch (t)
	{
		case GLARE_LUA_STRING:
			deserialiseString(state, serialised);
			break;
		case GLARE_LUA_BOOL:
			deserialiseBool(state, serialised);
			break;
		case GLARE_LUA_NUMBER:
			deserialiseNumber(state, serialised);
			break;
		case GLARE_LUA_TABLE:
			deserialiseTable(state, metatable_uid_to_ref_map, serialised);
			break;		
		case GLARE_LUA_VECTOR:
			deserialiseVector(state, serialised);
			break;
		case GLARE_LUA_NIL:
			deserialiseNil(state, serialised);
			break;
		default:
			throw glare::Exception("Error while deserialising, invalid type " + toString(t));
	}
}


void LuaSerialisation::serialise(lua_State* state, int stack_index, const SerialisationOptions& options, BufferOutStream& serialised)
{
	serialised.clear();

	// Write version
	serialised.writeUInt32(1);

	serialiseValue(state, stack_index, /*depth=*/0, options, serialised);
}


void LuaSerialisation::deserialise(lua_State* state, HashMap<uint32, int>& metatable_uid_to_ref_map, BufferViewInStream& serialised)
{
	// Read version
	/*const uint32 version =*/ serialised.readUInt32();

	deserialiseValue(state, metatable_uid_to_ref_map, serialised);
}


#if BUILD_TESTS


#include "LuaVM.h"
#include "LuaScript.h"
#include "LuaUtils.h"
#include "../utils/TestUtils.h"
#include "../utils/TestExceptionUtils.h"


//========================== Fuzzing ==========================
#if 1
// Command line:
// C:\fuzz_corpus\lua_serialisation C:\code\glare-core\testfiles\lua\serialisation_fuzz_seeds

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		LuaVM vm;
		vm.max_total_mem_allowed = 16 * 1024 * 1024;

		LuaScriptOptions script_options;
		script_options.max_num_interrupts = 1000;

		const std::string src((const char*)data, size);
		LuaScript script(&vm, script_options, src);
		script.exec();

		lua_getglobal(script.thread_state, "a");

		//LuaUtils::printStack(script.thread_state);

		if(lua_gettop(script.thread_state) >= 1)
		{
			LuaSerialisation::SerialisationOptions options;
			options.max_depth = 16;
			options.max_serialised_size_B = 64 * 1024;
		
			BufferOutStream serialised;
			LuaSerialisation::serialise(script.thread_state, /*stack index=*/-1, options, serialised);

			// Deserialise
			BufferViewInStream buffer_in_stream(ArrayRef<uint8>(serialised.buf.data(), serialised.buf.size()));
			HashMap<uint32, int> metatable_uid_to_ref_map(/*empty key=*/std::numeric_limits<uint32>::max());
			LuaSerialisation::deserialise(script.thread_state, metatable_uid_to_ref_map, buffer_in_stream);

			testAssert(buffer_in_stream.endOfStream());
		}
	}
	catch(glare::Exception& /*e*/)
	{
		// conPrint("Excep: " + e.what());
	}

	return 0; // Non-zero return values are reserved for future use.
}
#endif
//========================== End fuzzing ==========================


class PrinterLuaSerialisationOutputHandler : public LuaScriptOutputHandler
{
public:
	virtual void printFromLuaScript(LuaScript* lua_script, const char* s, size_t len) override
	{
		conPrint(std::string(s, len));
		buf += std::string(s, len);
	}
	std::string buf;
};


void LuaSerialisation::test()
{
	conPrint("LuaSerialisation::test()");

	try
	{
		LuaVM vm;
	
		const std::string src = "t = { x = 123.0, b=true, s = 'abc', v1d = {x=7, y=8, z=9}, v1f = Vec3f(7, 8, 9), m1 = {10, 11, 12, 13} }  " 
			"t2 = { 100, 200, 300, 'a' }          "
			"v3 = Vec3f(1, 2, 3)               "
			"v3d = Vec3d(101, 102, 103)  "
			"s2 = \"abc\"                      "
			"n = 567.0                  "
			"m2 = {10, 11, 12, 13}    "
			"b1 = true                "
			"b2 = false              "
			"t1 = { a = \"b\" }    "
			"t3 = { a = t1 }    " // nested table
			"circ_table = { a = nil }      " // table with circular reference
			"circ_table.a = circ_table     " // make the circular reference
			"deep_table = { a = { b = { c = { d = { e = { f = { g = { a = { b = { c = { d = { e = { f = { g = { a = { b = { c = { d = { e = { f = { g = 1.0} } } } } } } }}}}}}}}}}}}}}    \n"
			"function someFunc() return 1.0 end   \n"
			"nilval = nil"
			"";

		PrinterLuaSerialisationOutputHandler handler;
		LuaScriptOptions script_options;
		script_options.script_output_handler = &handler;
		LuaScript script(&vm, script_options, src);
		script.exec();

		
		// Serialise and deserialise a number
		{
			lua_getglobal(script.thread_state, "n"); // push onto stack

			const int initial_stack_size = lua_gettop(script.thread_state);

			BufferOutStream serialised;
			serialise(script.thread_state, /*stack index=*/-1, SerialisationOptions(), serialised);

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size

			lua_pop(script.thread_state, 1);

			// Deserialise it
			HashMap<uint32, int> metatable_uid_to_ref_map(/*empty key=*/std::numeric_limits<uint32>::max());

			BufferViewInStream instream(ArrayRef<uint8>(serialised.buf.data(), serialised.buf.size()));
			deserialise(script.thread_state, metatable_uid_to_ref_map, instream);
			testAssert(instream.endOfStream());

			// The number should be on the stack now
			testAssert(lua_type(script.thread_state, -1) == LUA_TNUMBER);
			testAssert(lua_tonumber(script.thread_state, -1) == 567.0);

			lua_pop(script.thread_state, 1); // pop the number
		}

		// Serialise and deserialise a boolean
		{
			lua_getglobal(script.thread_state, "b1"); // push onto stack

			const int initial_stack_size = lua_gettop(script.thread_state);

			BufferOutStream serialised;
			serialise(script.thread_state, /*stack index=*/-1, SerialisationOptions(), serialised);

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size

			lua_pop(script.thread_state, 1);

			// Deserialise it
			HashMap<uint32, int> metatable_uid_to_ref_map(/*empty key=*/std::numeric_limits<uint32>::max());

			BufferViewInStream instream(ArrayRef<uint8>(serialised.buf.data(), serialised.buf.size()));
			deserialise(script.thread_state, metatable_uid_to_ref_map, instream);
			testAssert(instream.endOfStream());

			// The bool should be on the stack now
			testAssert(lua_type(script.thread_state, -1) == LUA_TBOOLEAN);
			testAssert(lua_toboolean(script.thread_state, -1) == 1);

			lua_pop(script.thread_state, 1); // pop the bool
		}

		// Serialise and deserialise a string
		{
			lua_getglobal(script.thread_state, "s2"); // push onto stack

			const int initial_stack_size = lua_gettop(script.thread_state);

			BufferOutStream serialised;
			serialise(script.thread_state, /*stack index=*/-1, SerialisationOptions(), serialised);

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size

			lua_pop(script.thread_state, 1);

			// Deserialise it
			HashMap<uint32, int> metatable_uid_to_ref_map(/*empty key=*/std::numeric_limits<uint32>::max());

			BufferViewInStream instream(ArrayRef<uint8>(serialised.buf.data(), serialised.buf.size()));
			deserialise(script.thread_state, metatable_uid_to_ref_map, instream);
			testAssert(instream.endOfStream());

			// The string should be on the stack now
			testAssert(lua_type(script.thread_state, -1) == LUA_TSTRING);
			testAssert(LuaUtils::getString(script.thread_state, -1) == "abc");

			lua_pop(script.thread_state, 1); // pop the string
		}

		// Serialise and deserialise a table
		{
			lua_getglobal(script.thread_state, "t1"); // push onto stack

			const int initial_stack_size = lua_gettop(script.thread_state);

			BufferOutStream serialised;
			serialise(script.thread_state, /*stack index=*/-1, SerialisationOptions(), serialised);

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size

			lua_pop(script.thread_state, 1);

			// Deserialise it
			HashMap<uint32, int> metatable_uid_to_ref_map(/*empty key=*/std::numeric_limits<uint32>::max());

			BufferViewInStream instream(ArrayRef<uint8>(serialised.buf.data(), serialised.buf.size()));
			deserialise(script.thread_state, metatable_uid_to_ref_map, instream);
			testAssert(instream.endOfStream());

			// The table should be on the stack now
			testAssert(lua_type(script.thread_state, -1) == LUA_TTABLE);
			testAssert(LuaUtils::getTableStringField(script.thread_state, -1, "a") == "b");

			lua_pop(script.thread_state, 1); // pop the table
		}

		// Serialise and deserialise an array table (m2 = {10, 11, 12, 13} )
		{
			lua_getglobal(script.thread_state, "m2"); // push onto stack

			const int initial_stack_size = lua_gettop(script.thread_state);

			BufferOutStream serialised;
			serialise(script.thread_state, /*stack index=*/-1, SerialisationOptions(), serialised);

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size

			lua_pop(script.thread_state, 1);

			// Deserialise it
			HashMap<uint32, int> metatable_uid_to_ref_map(/*empty key=*/std::numeric_limits<uint32>::max());

			BufferViewInStream instream(ArrayRef<uint8>(serialised.buf.data(), serialised.buf.size()));
			deserialise(script.thread_state, metatable_uid_to_ref_map, instream);
			testAssert(instream.endOfStream());

			// The table should be on the stack now
			testAssert(lua_type(script.thread_state, -1) == LUA_TTABLE);
			testAssert(LuaUtils::getTableNumberArrayElem(script.thread_state, -1, /*elem index=*/1) == 10);
			testAssert(LuaUtils::getTableNumberArrayElem(script.thread_state, -1, /*elem index=*/2) == 11);
			testAssert(LuaUtils::getTableNumberArrayElem(script.thread_state, -1, /*elem index=*/3) == 12);
			testAssert(LuaUtils::getTableNumberArrayElem(script.thread_state, -1, /*elem index=*/4) == 13);

			lua_pop(script.thread_state, 1); // pop the table
		}

		// Serialise and deserialise a nested table (t3 = { a = t1 })
		{
			lua_getglobal(script.thread_state, "t3"); // push onto stack

			const int initial_stack_size = lua_gettop(script.thread_state);

			BufferOutStream serialised;
			serialise(script.thread_state, /*stack index=*/-1, SerialisationOptions(), serialised);

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size

			lua_pop(script.thread_state, 1);

			// Deserialise it
			HashMap<uint32, int> metatable_uid_to_ref_map(/*empty key=*/std::numeric_limits<uint32>::max());

			BufferViewInStream instream(ArrayRef<uint8>(serialised.buf.data(), serialised.buf.size()));
			deserialise(script.thread_state, metatable_uid_to_ref_map, instream);
			testAssert(instream.endOfStream());

			// The table should be on the stack now
			testAssert(lua_type(script.thread_state, -1) == LUA_TTABLE);

			lua_getfield(script.thread_state, -1, "a"); // push table t1 onto stack

			testAssert(lua_type(script.thread_state, -1) == LUA_TTABLE);
			testAssert(LuaUtils::getTableStringField(script.thread_state, -1, "a") == "b");

			lua_pop(script.thread_state, 1); // pop table t1

			lua_pop(script.thread_state, 1); // pop table t3
		}

		// Test attempting to serialise a table with a circular reference.
		// Make sure we don't overflow the stack.
		{
			lua_getglobal(script.thread_state, "circ_table"); // push onto stack

			LuaUtils::printStack(script.thread_state);
			const int initial_stack_size = lua_gettop(script.thread_state);

			testThrowsExcepContainingString([&]() {
				BufferOutStream serialised;
				serialise(script.thread_state, /*stack index=*/-1, SerialisationOptions(), serialised);
			}, "depth");

			//testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size
			testAssert(lua_gettop(script.thread_state) >= initial_stack_size);
			lua_settop(script.thread_state, initial_stack_size); // Restore stack

			lua_pop(script.thread_state, 1); // pop table circ_table
		}

		// Test a serialisation where we exceed max_serialised_size_B
		{
			lua_getglobal(script.thread_state, "s2"); // push onto stack

			LuaUtils::printStack(script.thread_state);
			const int initial_stack_size = lua_gettop(script.thread_state);

			SerialisationOptions options;
			options.max_serialised_size_B = 3;

			testThrowsExcepContainingString([&]() {
				BufferOutStream serialised;
				serialise(script.thread_state, /*stack index=*/-1, options, serialised);
			}, "max_serialised_size_B");

			//testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size
			testAssert(lua_gettop(script.thread_state) >= initial_stack_size);
			lua_settop(script.thread_state, initial_stack_size); // Restore stack

			lua_pop(script.thread_state, 1); 
		}

		// Serialise and deserialise a Vec3d (tests metatable uid serialisation)
		{
			lua_getglobal(script.thread_state, "v3d"); // push onto stack

			const int initial_stack_size = lua_gettop(script.thread_state);

			BufferOutStream serialised;
			serialise(script.thread_state, /*stack index=*/-1, SerialisationOptions(), serialised);

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size

			lua_pop(script.thread_state, 1);

			// Deserialise it
			HashMap<uint32, int> metatable_uid_to_ref_map(/*empty key=*/std::numeric_limits<uint32>::max());
			metatable_uid_to_ref_map[1] = vm.Vec3dMetaTable_ref;

			BufferViewInStream instream(ArrayRef<uint8>(serialised.buf.data(), serialised.buf.size()));
			deserialise(script.thread_state, metatable_uid_to_ref_map, instream);
			testAssert(instream.endOfStream());

			// The table should be on the stack now
			testAssert(lua_type(script.thread_state, -1) == LUA_TTABLE);

			const int have_metatable = lua_getmetatable(script.thread_state, -1); // Pushes onto stack if there.
			testAssert(have_metatable);
			testAssert(LuaUtils::getTableNumberField(script.thread_state, /*table index=*/-1, "uid") == 1);
			lua_pop(script.thread_state, 1); // pop metatable

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size
			lua_pop(script.thread_state, 1); // pop v3d
		}

		// Serialise and deserialise a Vec3f (Vec3f is set as the built-in vector type)
		{
			lua_getglobal(script.thread_state, "v3"); // push onto stack
			const int initial_stack_size = lua_gettop(script.thread_state);

			BufferOutStream serialised;
			serialise(script.thread_state, /*stack index=*/-1, SerialisationOptions(), serialised);

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size

			lua_pop(script.thread_state, 1);

			// Deserialise it
			HashMap<uint32, int> metatable_uid_to_ref_map(/*empty key=*/std::numeric_limits<uint32>::max());

			BufferViewInStream instream(ArrayRef<uint8>(serialised.buf.data(), serialised.buf.size()));
			deserialise(script.thread_state, metatable_uid_to_ref_map, instream);
			testAssert(instream.endOfStream());

			// The vector should be on the stack now
			testAssert(lua_type(script.thread_state, -1) == LUA_TVECTOR);

			testAssert(lua_tovector(script.thread_state, -1)[0] == 1.f);
			testAssert(lua_tovector(script.thread_state, -1)[1] == 2.f);
			testAssert(lua_tovector(script.thread_state, -1)[2] == 3.f);

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size
			lua_pop(script.thread_state, 1); // pop v3
		}

		// Serialise and deserialise nil
		{
			lua_getglobal(script.thread_state, "nilval"); // push onto stack
			const int initial_stack_size = lua_gettop(script.thread_state);

			BufferOutStream serialised;
			serialise(script.thread_state, /*stack index=*/-1, SerialisationOptions(), serialised);

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size

			lua_pop(script.thread_state, 1);

			// Deserialise it
			HashMap<uint32, int> metatable_uid_to_ref_map(/*empty key=*/std::numeric_limits<uint32>::max());

			BufferViewInStream instream(ArrayRef<uint8>(serialised.buf.data(), serialised.buf.size()));
			deserialise(script.thread_state, metatable_uid_to_ref_map, instream);
			testAssert(instream.endOfStream());

			// nil should be on the stack now
			testAssert(lua_type(script.thread_state, -1) == LUA_TNIL);

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size
			lua_pop(script.thread_state, 1); // pop nil
		}

		// Test trying to serialise a function fails with an exception thrown.
		{
			lua_getglobal(script.thread_state, "someFunc"); // push onto stack

			LuaUtils::printStack(script.thread_state);
			const int initial_stack_size = lua_gettop(script.thread_state);

			SerialisationOptions options;

			testThrowsExcepContainingString([&]() {
				BufferOutStream serialised;
				serialise(script.thread_state, /*stack index=*/-1, options, serialised);
			}, "function");

			testAssert(lua_gettop(script.thread_state) == initial_stack_size);
			lua_settop(script.thread_state, initial_stack_size); // Restore stack

			lua_pop(script.thread_state, 1);
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
	
	try
	{
		// Serialise and deserialise a deeply nested table
		// Use a different VM for deserialisation so we can test with an initially smaller Lua stack
		BufferOutStream serialised;
		{
			LuaVM vm;
			const std::string src =
				"deep_table = { a = { b = { c = { d = { e = { f = { g = { a = { b = { c = { d = { e = { f = { g = { a = { b = { c = { d = { e = { f = { g = 1.0} } } } } } } }}}}}}}}}}}}}}"
				"";
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();
		
			lua_getglobal(script.thread_state, "deep_table"); // push onto stack
			const int initial_stack_size = lua_gettop(script.thread_state);
			SerialisationOptions options;
			options.max_depth = 100; // Crank up max depth so we can serialise these tables.
			serialise(script.thread_state, /*stack index=*/-1, options, serialised);
			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size
			lua_pop(script.thread_state, 1);
		}
		// Deserialise it
		{
			LuaVM vm;
			const std::string src = "";
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();

			HashMap<uint32, int> metatable_uid_to_ref_map(/*empty key=*/std::numeric_limits<uint32>::max());

			printVar(LuaUtils::freeStackCapacity(script.thread_state));

			BufferViewInStream instream(ArrayRef<uint8>(serialised.buf.data(), serialised.buf.size()));
			deserialise(script.thread_state, metatable_uid_to_ref_map, instream);

			printVar(LuaUtils::freeStackCapacity(script.thread_state));
		}

		//TEMP:
		/*for(int i=0; i<1000; ++i)
		{
			printVar(freeStackCapacity(script.thread_state));
			lua_pushnumber(script.thread_state, 1.0);
		}*/
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	conPrint("LuaSerialisation::test() done");
}


#endif // BUILD_TESTS
