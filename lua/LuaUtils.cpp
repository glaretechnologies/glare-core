/*=====================================================================
LuaUtils.cpp
------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "LuaUtils.h"


#include "LuaScript.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../maths/mathstypes.h"
#include <lualib.h>
#include <Luau/Common.h>
#include <limits>


static std::string valueShortString(lua_State* state, int index)
{
	const int t = lua_type(state, index);
	switch (t)
	{
		case LUA_TSTRING:
			return "\"" + std::string(lua_tostring(state, index)) + "\"";
		case LUA_TBOOLEAN:
			return lua_toboolean(state, index) ? "true" : "false";
		case LUA_TNUMBER:
			return doubleToString(lua_tonumber(state, index));
		case LUA_TTABLE:
			return "Table";
		case LUA_TFUNCTION:
			return "Function";
		default:
			return "other type: " + std::string(lua_typename(state, t)) + "]";
	}
}


void LuaUtils::printTable(lua_State* state, int table_index, int spaces)
{
	assert(table_index >= 1);

	lua_pushnil(state); // Push first key onto stack
	while(1)
	{
		int notdone = lua_next(state, table_index); // pops a key from the stack, and pushes a key-value pair from the table at the given index
		if(notdone == 0)
			break;

		conPrint(std::string(spaces, ' ') + valueShortString(state, -2) + " : " + valueShortString(state, -1));

		lua_pop(state, 1); // Remove value, keep key on stack for next lua_next call
	}

	const int have_metatable = lua_getmetatable(state, table_index); // Pushes onto stack if there.
	if(have_metatable)
	{
		conPrint(std::string(spaces, ' ') + "metatable:");
		printTable(state, lua_gettop(state), spaces + 4);

		lua_pop(state, 1); // pop metatable off stack
	}
}


// Adapted from https://www.lua.org/pil/24.2.3.html
void LuaUtils::printStack(lua_State* L)
{
	conPrint("---------------- Lua stack ----------------");
	const int top = lua_gettop(L);
	for(int i = 1; i <= top; i++) // repeat for each level
	{  
		conPrintStr(toString(i) + ": ");
		int t = lua_type(L, i);
		switch (t) {

		case LUA_TSTRING:
			conPrint("string: " + std::string(lua_tostring(L, i)));
			break;

		case LUA_TBOOLEAN:
			conPrint(lua_toboolean(L, i) ? "true" : "false");
			break;

		case LUA_TNUMBER:
			conPrint(doubleToString(lua_tonumber(L, i)));
			break;

		case LUA_TTABLE:
			conPrint("Table");
			printTable(L, i);
			break;

		case LUA_TFUNCTION:
			conPrint("Function");
			break;

		default:  /* other values */
			conPrint("other type: " + std::string(lua_typename(L, t)));
			break;
		}
	}
	conPrint("-------------------------------------------");
}


void LuaUtils::setCFunctionAsTableField(lua_State* state, lua_CFunction fn, const char* debugname, int table_index, const char* field_key)
{
	lua_pushcfunction(state, fn, debugname);
	lua_rawsetfield(state, table_index, field_key); // pops value (function) from stack
}


double LuaUtils::getTableNumberField(lua_State* state, int table_index, const char* key)
{
	// Check if the value on the stack is actually a table, otherwise hit assert in lua_rawgetfield.
	if(!lua_istable(state, table_index))
		throw glare::Exception("Value was not a table");

	lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	const double val = lua_tonumber(state, -1);
	lua_pop(state, 1);
	return val;
}


double LuaUtils::getTableNumberArrayElem(lua_State* state, int table_index, const int elem_index)
{
	// Check if the value on the stack is actually a table, otherwise hit assert in lua_rawgeti.
	if(!lua_istable(state, table_index))
		throw glare::Exception("Value was not a table");

	lua_rawgeti(state, table_index, elem_index); // Push value onto stack
	const double val = lua_tonumber(state, -1);
	lua_pop(state, 1);
	return val;
}


double LuaUtils::getTableNumberFieldWithDefault(lua_State* state, int table_index, const char* key, double default_val)
{
	// Check if the value on the stack is actually a table, otherwise hit assert in lua_rawgetfield.
	if(!lua_istable(state, table_index))
		throw glare::Exception("Value was not a table");

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	if(value_type == LUA_TNIL)
	{
		lua_pop(state, 1);
		return default_val;
	}
	else
	{
		const double val = lua_tonumber(state, -1);
		lua_pop(state, 1);
		return val;
	}
}


bool LuaUtils::tableHasBoolField(lua_State* state, int table_index, const char* key)
{
	// Check if the value on the stack is actually a table, otherwise hit assert in lua_rawgetfield.
	if(!lua_istable(state, table_index))
		throw glare::Exception("Value was not a table");

	lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	const bool val = lua_isboolean(state, -1);
	lua_pop(state, 1);
	return val;
}


bool LuaUtils::getTableBoolFieldWithDefault(lua_State* state, int table_index, const char* key, bool default_val)
{
	// Check if the value on the stack is actually a table, otherwise hit assert in lua_rawgetfield.
	if(!lua_istable(state, table_index))
		throw glare::Exception("Value was not a table");

	lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	if(lua_isboolean(state, -1))
	{
		const bool val = lua_toboolean(state, -1);
		lua_pop(state, 1);
		return val;
	}
	else
	{
		lua_pop(state, 1);
		return default_val;
	}
}


std::string LuaUtils::getTableStringField(lua_State* state, int table_index, const char* key)
{
	// Check if the value on the stack is actually a table, otherwise hit assert in lua_rawgetfield.
	if(!lua_istable(state, table_index))
		throw glare::Exception("Value was not a table");

	lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	size_t len;
	const char* s = lua_tolstring(state, -1, &len);
	if(s)
	{
		const std::string res(s, len);
		lua_pop(state, 1);
		return res;
	}
	else
	{
		lua_pop(state, 1);
		return std::string();
	}
}


Vec3d LuaUtils::getTableVec3dField(lua_State* state, int table_index, const char* key)
{
	// Check if the value on the stack is actually a table, otherwise hit assert in lua_rawgetfield.
	if(!lua_istable(state, table_index))
		throw glare::Exception("Value was not a table");

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	if(value_type == LUA_TTABLE)
	{
		const double x = getTableNumberField(state, -1, "x");
		const double y = getTableNumberField(state, -1, "y");
		const double z = getTableNumberField(state, -1, "z");

		lua_pop(state, 1);

		return Vec3d(x, y, z);
	}
	else
	{
		lua_pop(state, 1);
		throw glare::Exception("Could not find table with key '" + std::string(key) + "'");
	}
}


Vec3f LuaUtils::getTableVec3fFieldWithDefault(lua_State* state, int table_index, const char* key, const Vec3f& default_val)
{
	// Check if the value on the stack is actually a table, otherwise hit assert in lua_rawgetfield.
	if(!lua_istable(state, table_index))
		throw glare::Exception("Value was not a table");

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call).  Returns the type of the pushed value.
	
	Vec3f res;
	if(value_type == LUA_TTABLE)
	{
		const double x = getTableNumberField(state, -1, "x");
		const double y = getTableNumberField(state, -1, "y");
		const double z = getTableNumberField(state, -1, "z");

		res = Vec3f((float)x, (float)y, (float)z);
	}
	else
		res = default_val;

	lua_pop(state, 1); // Pop field

	return res;
}


Matrix2f LuaUtils::getTableMatrix2fFieldWithDefault(lua_State* state, int table_index, const char* key, const Matrix2f& default_val)
{
	// Check if the value on the stack is actually a table, otherwise hit assert in lua_rawgetfield.
	if(!lua_istable(state, table_index))
		throw glare::Exception("Value was not a table");

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call).  Returns the type of the pushed value.
	
	Matrix2f res;
	if(value_type == LUA_TTABLE)
	{
		const double x = getTableNumberArrayElem(state, -1, /*elem index=*/1);
		const double y = getTableNumberArrayElem(state, -1, /*elem index=*/2);
		const double z = getTableNumberArrayElem(state, -1, /*elem index=*/3);
		const double w = getTableNumberArrayElem(state, -1, /*elem index=*/4);

		res = Matrix2f((float)x, (float)y, (float)z, (float)w);
	}
	else
		res = default_val;

	lua_pop(state, 1); // Pop field

	return res;
}


#if BUILD_TESTS


#include "LuaVM.h"
#include "LuaScript.h"
#include "../utils/TestUtils.h"
#include <functional>


static void testExceptionExpected(std::function<void()> test_func)
{
	try
	{
		test_func(); // Execute the test code.
		failTest("Excep expected");
	}
	catch(glare::Exception& e)
	{
		conPrint("Caught expected excep: " + e.what());
	}
}


void LuaUtils::test()
{
	conPrint("LuaUtils::test()");

	{
		try
		{
			LuaVM vm;
	
			const std::string src = "t = { x = 123.0, b=true, s = 'abc', v1 = {x=7, z=8}, m1 = {10, 11, 12, 13} }   t2 = { 100, 200, 300 }";
			LuaScript script(&vm, LuaScriptOptions(), src);

			

			lua_getglobal(script.thread_state, "t"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			printStack(script.thread_state);
			int initial_stack_size = lua_gettop(script.thread_state);

			//----------------------------------- Test getTableNumberField -----------------------------------
			testAssert(getTableNumberField(script.thread_state, /*table index=*/-1, "x") == 123.0);
			testAssert(getTableNumberField(script.thread_state, /*table index=*/-1, "xdfgfdg") == 0.0);

			// Test getTableNumberField on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableNumberField(script.thread_state, /*table index=*/-1, "xfsdfd"); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size


			//----------------------------------- Test getTableNumberFieldWithDefault -----------------------------------
			testAssert(getTableNumberFieldWithDefault(script.thread_state, /*table index=*/-1, "x", /*default val=*/-1.0) == 123.0);
			testAssert(getTableNumberFieldWithDefault(script.thread_state, /*table index=*/-1, "xsdfdsf", /*default val=*/-1.0) == -1.0);

			// Test getTableNumberFieldWithDefault on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableNumberFieldWithDefault(script.thread_state, /*table index=*/-1, "x", /*default val=*/-1.0); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size


			//----------------------------------- Test tableHasBoolField -----------------------------------
			testAssert(tableHasBoolField(script.thread_state, /*table index=*/-1, "b"));
			testAssert(!tableHasBoolField(script.thread_state, /*table index=*/-1, "x"));
			testAssert(!tableHasBoolField(script.thread_state, /*table index=*/-1, "xsdfsdf"));

			// Test tableHasBoolField on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { tableHasBoolField(script.thread_state, /*table index=*/-1, "x"); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size


			//----------------------------------- Test getTableBoolFieldWithDefault -----------------------------------
			testAssert(getTableBoolFieldWithDefault(script.thread_state, /*table index=*/-1, "b", /*default val=*/false) == true);
			testAssert(getTableBoolFieldWithDefault(script.thread_state, /*table index=*/-1, "x", /*default val=*/false) == false);
			testAssert(getTableBoolFieldWithDefault(script.thread_state, /*table index=*/-1, "x", /*default val=*/true) == true);
			testAssert(getTableBoolFieldWithDefault(script.thread_state, /*table index=*/-1, "xsdfsdf", /*default val=*/false) == false);
			testAssert(getTableBoolFieldWithDefault(script.thread_state, /*table index=*/-1, "xsdfsdf", /*default val=*/true) == true);

			// Test getTableBoolFieldWithDefault on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableBoolFieldWithDefault(script.thread_state, /*table index=*/-1, "x", /*default val=*/true); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size


			//----------------------------------- Test getTableStringField -----------------------------------
			testAssert(getTableStringField(script.thread_state, /*table index=*/-1, "s") == "abc");
			testAssert(getTableStringField(script.thread_state, /*table index=*/-1, "sxxsdfgdsfg") == "");
			testAssert(getTableStringField(script.thread_state, /*table index=*/-1, "x") == "123"); // Type conversion

			// Test getTableStringField on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableStringField(script.thread_state, /*table index=*/-1, "x"); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size


			//----------------------------------- Test getTableVec3dField -----------------------------------
			testAssert(getTableVec3dField(script.thread_state, /*table index=*/-1, "v1") == Vec3d(7.0, 0.0, 8.0));
			//testAssert(getTableVec3dField(script.thread_state, /*table index=*/-1, "vdfgdfg") == Vec3d(0.0));
			testExceptionExpected([&]() { getTableVec3dField(script.thread_state, /*table index=*/-1, "vdfgdfg"); });

			// Test getTableVec3dField on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableVec3dField(script.thread_state, /*table index=*/-1, "x"); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size


			//----------------------------------- Test getTableVec3fFieldWithDefault -----------------------------------
			testAssert(getTableVec3fFieldWithDefault(script.thread_state, /*table index=*/-1, "v1", /*default val=*/Vec3f(-1.f)) == Vec3f(7.0f, 0.0f, 8.0f));
			testAssert(getTableVec3fFieldWithDefault(script.thread_state, /*table index=*/-1, "zz", /*default val=*/Vec3f(-1.f)) == Vec3f(-1.f));

			// Test getTableVec3fFieldWithDefault on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableVec3fFieldWithDefault(script.thread_state, /*table index=*/-1, "x", /*default val=*/Vec3f(-1.f)); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size


			//----------------------------------- Test getTableMatrix2fFieldWithDefault -----------------------------------
			testAssert(getTableMatrix2fFieldWithDefault(script.thread_state, /*table index=*/-1, "m1", /*default val=*/Matrix2f::identity()) == Matrix2f(10.f, 11.f, 12.f, 13.f));
			testAssert(getTableMatrix2fFieldWithDefault(script.thread_state, /*table index=*/-1, "zz", /*default val=*/Matrix2f::identity()) == Matrix2f::identity());

			// Test getTableMatrix2fFieldWithDefault on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableMatrix2fFieldWithDefault(script.thread_state, /*table index=*/-1, "x", /*default val=*/Matrix2f::identity()); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size



			lua_pop(script.thread_state, 1); // Pop t

			lua_getglobal(script.thread_state, "t2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			printStack(script.thread_state);
			initial_stack_size = lua_gettop(script.thread_state);


			//----------------------------------- Test getTableNumberArrayElem -----------------------------------
			testAssert(getTableNumberArrayElem(script.thread_state, /*table index=*/-1, 1) == 100.0);
			testAssert(getTableNumberArrayElem(script.thread_state, /*table index=*/-1, 2) == 200.0);
			testAssert(getTableNumberArrayElem(script.thread_state, /*table index=*/-1, 3) == 300.0);
			testAssert(getTableNumberArrayElem(script.thread_state, /*table index=*/-1, 100) == 0.0); // out of bounds

			// Test getTableNumberArrayElem on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableNumberArrayElem(script.thread_state, /*table index=*/-1, 1); });
			lua_pop(script.thread_state, 1); // Pop 123.0


			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

	}


	conPrint("LuaUtils::test() done");
}


#endif // BUILD_TESTS
