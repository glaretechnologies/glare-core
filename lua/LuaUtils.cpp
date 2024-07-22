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
#include <../src/lstate.h>


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


std::string LuaUtils::getCallStackAsString(lua_State* state)
{
	const int depth = lua_stackdepth(state);

	std::string s = "----------- Call stack -----------\n";
	for(int level=0; level < depth; ++level)
	{
		lua_Debug ar;
		ar.name = NULL;
		ar.what = NULL;
		lua_getinfo(state, level, "snl", &ar); // s = what, n = funcname, l = currentline

		const std::string line_string = (ar.currentline == -1) ? "[unknown]" : toString(ar.currentline);
		s += std::string(ar.what ? ar.what : "") + ", func: " + std::string(ar.name ? ar.name : "[top level code]") + ", line: " + line_string + "\n";
	}
	s += "----------------------------------";
	return s;
}


int LuaUtils::freeStackCapacity(lua_State* state)
{
	return (int)(state->ci->top - state->top);
}


bool LuaUtils::isFunctionDefined(lua_State* state, const char* func_name)
{
	lua_getglobal(state, func_name); // Push function onto stack (or nil if it isn't defined)
	const bool is_defined = lua_isfunction(state, -1);
	lua_pop(state, 1); // Pop function off stack
	return is_defined;
}


LuaUtils::LuaFuncRefAndPtr LuaUtils::getRefToFunction(lua_State* state, const char* func_name)
{
	lua_getglobal(state, func_name); // Push function onto stack (or nil if it isn't defined)
	
	LuaFuncRefAndPtr res;
	res.ref = LUA_NOREF;
	res.func_ptr = nullptr;

	if(lua_isfunction(state, -1)) // If function was defined
	{
		res.ref = lua_ref(state, /*index=*/-1); // Get reference to func (does not pop)
		res.func_ptr = lua_topointer(state, /*index=*/-1); // Get pointer to func (does not pop)
	}
	
	lua_pop(state, 1);
	return res;
}


static std::string errorContextString(lua_State* state)
{
	return "\n" + LuaUtils::getCallStackAsString(state);
}


static std::string luaTypeString(lua_State* state, int type)
{
	// Check type enum value is in bounds of luaT_typenames
	assert(type >= 0 && type < 11);
	if(type < 0 || type >= 11)
		throw glare::Exception("Internal error, invalid type");
	return std::string(lua_typename(state, type));
}


void LuaUtils::setCFunctionAsTableField(lua_State* state, lua_CFunction fn, const char* debugname, const char* field_key)
{
	assert(lua_istable(state, -1));

	lua_pushcfunction(state, fn, debugname);
	lua_rawsetfield(state, /*table_index=*/-2, field_key); // pops value (function) from stack
}


// Check if the value on the stack is actually a table, otherwise we will hit an assert in lua_rawgetfield.
// Throws Exception if not a table.
void LuaUtils::checkValueIsTable(lua_State* state, int index)
{
	if(!lua_istable(state, index))
		throw glare::Exception("Value was not a table (had type " + luaTypeString(state, lua_type(state, index)) + ")" + errorContextString(state));
}


void LuaUtils::checkArgIsFunction(lua_State* state, int table_index)
{
	if(lua_type(state, /*index=*/table_index) != LUA_TFUNCTION)
		throw glare::Exception("Argument " + toString(table_index) + " must be a function" + errorContextString(state));
}


void* LuaUtils::getTableLightUserDataField(lua_State* state, int table_index, const char* key)
{
	checkValueIsTable(state, table_index);

	lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	void* val = lua_tolightuserdata(state, -1);
	lua_pop(state, 1);
	return val;
}


double LuaUtils::getTableNumberField(lua_State* state, int table_index, const char* key)
{
	checkValueIsTable(state, table_index);

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	// lua_tonumber will update in-place a non-number value (such as a string) on the stack, which we don't want to do.
	// So just throw an error if the value is not a number.
	if(value_type == LUA_TNUMBER)
	{
		const double val = lua_tonumber(state, -1);
		lua_pop(state, 1);
		return val;
	}
	else if(value_type == LUA_TNIL)
	{
		lua_pop(state, 1);
		throw glare::Exception("Table field '" + std::string(key) + "' not found" + errorContextString(state));
	}
	else
	{
		lua_pop(state, 1);
		throw glare::Exception("Value for field '" + std::string(key) + "' is not a number (value has type " + luaTypeString(state, value_type) + ")" + errorContextString(state));
	}
}


double LuaUtils::getTableNumberArrayElem(lua_State* state, int table_index, const int elem_index)
{
	checkValueIsTable(state, table_index);

	const int value_type = lua_rawgeti(state, table_index, elem_index); // Push value onto stack
	// lua_tonumber will update in-place a non-number value (such as a string) on the stack, which we don't want to do.
	// So just throw an error if the value is not a number.
	if(value_type == LUA_TNUMBER)
	{
		const double val = lua_tonumber(state, -1);
		lua_pop(state, 1);
		return val;
	}
	else if(value_type == LUA_TNIL)
	{
		lua_pop(state, 1);
		throw glare::Exception("Value at table index " + toString(elem_index) + " was not found" + errorContextString(state));
	}
	else
	{
		lua_pop(state, 1);
		throw glare::Exception("Value at table index " + toString(elem_index) + " is not a number (value has type " + luaTypeString(state, value_type) + ")" + errorContextString(state));
	}
}


double LuaUtils::getTableNumberFieldWithDefault(lua_State* state, int table_index, const char* key, double default_val)
{
	checkValueIsTable(state, table_index);

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	if(value_type == LUA_TNUMBER)
	{
		const double val = lua_tonumber(state, -1);
		lua_pop(state, 1);
		return val;
	}
	else if(value_type == LUA_TNIL)
	{
		lua_pop(state, 1);
		return default_val;
	}
	else
	{
		// lua_tonumber will update in-place a non-number value (such as a string) on the stack, which we don't want to do.
		// So just throw an error.
		lua_pop(state, 1);
		throw glare::Exception("Value for field " + std::string(key) + " is not a number (value has type " + luaTypeString(state, value_type) + ")" + errorContextString(state));
	}
}


bool LuaUtils::tableHasBoolField(lua_State* state, int table_index, const char* key)
{
	checkValueIsTable(state, table_index);

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	const bool is_bool = value_type == LUA_TBOOLEAN;
	lua_pop(state, 1);
	return is_bool;
}


bool LuaUtils::getTableBoolFieldWithDefault(lua_State* state, int table_index, const char* key, bool default_val)
{
	checkValueIsTable(state, table_index);

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	if(value_type == LUA_TBOOLEAN)
	{
		const bool val = lua_toboolean(state, -1);
		lua_pop(state, 1);
		return val;
	}
	else if(value_type == LUA_TNIL)
	{
		lua_pop(state, 1);
		return default_val;
	}
	else
	{
		lua_pop(state, 1);
		throw glare::Exception("Value for field " + std::string(key) + " is not a boolean (value has type " + luaTypeString(state, value_type) + ")" + errorContextString(state));
	}
}


const char* LuaUtils::getStringConstCharPtr(lua_State* state, int index)
{
	if(!lua_isstring(state, index))
		throw glare::Exception("Value was not a string (value had type " + luaTypeString(state, lua_type(state, index)) + ")" + errorContextString(state));

	size_t len;
	const char* s = lua_tolstring(state, index, &len);
	if(s)
		return s;
	else
	{
		assert(0);  // Shouldn't get here
		throw glare::Exception("Value was not a string");
	}
}

const char* LuaUtils::getStringAndAtom(lua_State* state, int index, int& atom_out)
{
	if(!lua_isstring(state, index))
		throw glare::Exception("Value was not a string (value had type " + luaTypeString(state, lua_type(state, index)) + ")" + errorContextString(state));

	const char* s = lua_tostringatom(state, index, &atom_out);
	if(s)
		return s;
	else
	{
		assert(0);  // Shouldn't get here
		throw glare::Exception("Value was not a string");
	}
}

std::string LuaUtils::getString(lua_State* state, int index)
{
	if(!lua_isstring(state, index))
		throw glare::Exception("Value was not a string (value had type " + luaTypeString(state, lua_type(state, index)) + ")" + errorContextString(state));

	size_t len;
	const char* s = lua_tolstring(state, index, &len);
	if(s)
		return std::string(s, len);
	else
		runtimeCheckFailed("Value was not a string");
}


const char* LuaUtils::getStringPointerAndLen(lua_State* state, int index, size_t& len_out)
{
	if(!lua_isstring(state, index))
		throw glare::Exception("Value was not a string (value had type " + luaTypeString(state, lua_type(state, index)) + ")" + errorContextString(state));

	const char* s = lua_tolstring(state, index, &len_out);
	if(s)
		return s;
	else
		runtimeCheckFailed("Value was not a string");
}


std::string LuaUtils::getStringArg(lua_State* state, int index)
{
	if(!lua_isstring(state, index))
		throw glare::Exception("Argument " + toString(index) + " was not a string (argument had type " + luaTypeString(state, lua_type(state, index)) + ")" + errorContextString(state));

	size_t len;
	const char* s = lua_tolstring(state, index, &len);
	if(s)
		return std::string(s, len);
	else
	{
		assert(0);  // Shouldn't get here
		throw glare::Exception("Value was not a string");
	}
}


std::string LuaUtils::getTableStringField(lua_State* state, int table_index, const char* key)
{
	checkValueIsTable(state, table_index);

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	if(value_type == LUA_TSTRING)
	{
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
			throw glare::Exception("Value was not a string" + errorContextString(state));
		}
	}
	else
	{
		lua_pop(state, 1);
		throw glare::Exception("Value was not a string (value had type " + luaTypeString(state, value_type) + ")" + errorContextString(state));
	}
}


std::string LuaUtils::getTableStringFieldWithEmptyDefault(lua_State* state, int table_index, const char* key)
{
	checkValueIsTable(state, table_index);

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	if(value_type == LUA_TSTRING)
	{
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
	else if(value_type == LUA_TNIL)
	{
		lua_pop(state, 1);
		return std::string();
	}
	else
	{
		lua_pop(state, 1);
		throw glare::Exception("Value was not a string (value had type " + luaTypeString(state, value_type) + ")" + errorContextString(state));
	}
}


bool LuaUtils::getBool(lua_State* state, int index)
{
	if(lua_isboolean(state, index))
		return lua_toboolean(state, index) != 0;
	else
		throw glare::Exception("Value was not a boolean (value had type " + luaTypeString(state, lua_type(state, index)) + ")" + errorContextString(state));
}


float LuaUtils::getFloat(lua_State* state, int index)
{
	if(lua_isnumber(state, index))
		return (float)lua_tonumber(state, index);
	else
		throw glare::Exception("Value was not a number (value had type " + luaTypeString(state, lua_type(state, index)) + ")" + errorContextString(state));
}


double LuaUtils::getDoubleArg(lua_State* state, int index)
{
	if(lua_isnumber(state, index))
		return lua_tonumber(state, index);
	else
		throw glare::Exception("Argument " + toString(index) + " was not a number (argument had type '" + luaTypeString(state, lua_type(state, index)) + "')" + errorContextString(state));
}


Vec3f LuaUtils::getVec3f(lua_State* state, int index)
{
	const float* data = lua_tovector(state, index); // Will return NULL if value does not have vector type.
	if(data)
		return Vec3f(data[0], data[1], data[2]);
	else
		throw glare::Exception("Value was not a Vec3f (value had type " + luaTypeString(state, lua_type(state, index)) + ")" + errorContextString(state));

	/*const float x = (float)getTableNumberField(state, index, "x");
	const float y = (float)getTableNumberField(state, index, "y");
	const float z = (float)getTableNumberField(state, index, "z");
	
	return Vec3f(x, y, z);*/
}

Vec3d LuaUtils::getVec3d(lua_State* state, int index)
{
	const double x = getTableNumberField(state, index, "x");
	const double y = getTableNumberField(state, index, "y");
	const double z = getTableNumberField(state, index, "z");
	
	return Vec3d(x, y, z);
}


Matrix2f LuaUtils::getMatrix2f(lua_State* state, int table_index)
{
	const double x = getTableNumberArrayElem(state, table_index, /*elem index=*/1);
	const double y = getTableNumberArrayElem(state, table_index, /*elem index=*/2);
	const double z = getTableNumberArrayElem(state, table_index, /*elem index=*/3);
	const double w = getTableNumberArrayElem(state, table_index, /*elem index=*/4);

	return Matrix2f((float)x, (float)y, (float)z, (float)w);
}


Vec3d LuaUtils::getTableVec3dField(lua_State* state, int table_index, const char* key)
{
	checkValueIsTable(state, table_index);

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)
	if(value_type == LUA_TTABLE)
	{
		const Vec3d v = getVec3d(state, -1);
		lua_pop(state, 1);
		return v;
	}
	else
	{
		lua_pop(state, 1);
		throw glare::Exception("Could not find table with key '" + std::string(key) + "'" + errorContextString(state));
	}
}


Vec3f LuaUtils::getTableVec3fFieldWithDefault(lua_State* state, int table_index, const char* key, const Vec3f& default_val)
{
	checkValueIsTable(state, table_index);

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call).  Returns the type of the pushed value.
	
	Vec3f res;
	/*if(value_type == LUA_TTABLE)
	{
		const double x = getTableNumberField(state, -1, "x");
		const double y = getTableNumberField(state, -1, "y");
		const double z = getTableNumberField(state, -1, "z");

		res = Vec3f((float)x, (float)y, (float)z);
	}*/
	if(value_type == LUA_TVECTOR)
	{
		/*res = getVec3f(state, 
		const double x = getTableNumberField(state, -1, "x");
		const double y = getTableNumberField(state, -1, "y");
		const double z = getTableNumberField(state, -1, "z");

		res = Vec3f((float)x, (float)y, (float)z);*/
		const float* data = lua_tovector(state, -1); // Will return NULL if value does not have vector type.
		if(data)
		{
			lua_pop(state, 1); // Pop field
			return Vec3f(data[0], data[1], data[2]);
		}
		else
		{
			lua_pop(state, 1); // Pop field
			throw glare::Exception("Value was not a vector" + errorContextString(state));
		}
	}
	else if(value_type == LUA_TNIL)
	{
		lua_pop(state, 1); // Pop field
		return default_val;
	}
	else
	{
		lua_pop(state, 1); // Pop field
		throw glare::Exception("Value was not a Vec3f (value had type " + luaTypeString(state, value_type) + ")" + errorContextString(state));
	}
}


Matrix2f LuaUtils::getTableMatrix2fFieldWithDefault(lua_State* state, int table_index, const char* key, const Matrix2f& default_val)
{
	checkValueIsTable(state, table_index);

	const int value_type = lua_rawgetfield(state, table_index, key); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call).  Returns the type of the pushed value.
	if(value_type == LUA_TTABLE)
	{
		const double x = getTableNumberArrayElem(state, -1, /*elem index=*/1);
		const double y = getTableNumberArrayElem(state, -1, /*elem index=*/2);
		const double z = getTableNumberArrayElem(state, -1, /*elem index=*/3);
		const double w = getTableNumberArrayElem(state, -1, /*elem index=*/4);

		lua_pop(state, 1); // Pop field
		return Matrix2f((float)x, (float)y, (float)z, (float)w);
	}
	else
	{
		lua_pop(state, 1); // Pop field
		return default_val;
	}
}


void LuaUtils::pushVec3f(lua_State* state, const Vec3f& v)
{
	//lua_createtable(state, /*num array elems=*/0, /*num non-array elems=*/3);
	//setNumberAsTableField(state, "x", v.x);
	//setNumberAsTableField(state, "y", v.y);
	//setNumberAsTableField(state, "z", v.z);

	lua_pushvector(state, v.x, v.y, v.z);
}


void LuaUtils::pushVec3d(lua_State* state, const Vec3d& v)
{
	lua_createtable(state, /*num array elems=*/0, /*num non-array elems=*/3);

	setNumberAsTableField(state, "x", v.x);
	setNumberAsTableField(state, "y", v.y);
	setNumberAsTableField(state, "z", v.z);

	LuaScript* lua_script = static_cast<LuaScript*>(lua_getthreaddata(state));

	// Assign Vec3d metatable to the table
	lua_getref(state, lua_script->Vec3dMetaTable_ref); // Push Vec3dMetaTable onto stack
	lua_setmetatable(state, -2); // "Pops a table from the stack and sets it as the new metatable for the value at the given acceptable index."
}


void LuaUtils::pushMatrix2f(lua_State* state, const Matrix2f& m)
{
	lua_createtable(state, /*num array elems=*/4, /*num non-array elems=*/0);

	for(int i=0; i<4; ++i)
	{
		lua_pushnumber(state, m.e[i]);
		lua_rawseti(state, /*table index=*/-2, i);
	}
}


void LuaUtils::setStringAsTableField(lua_State* state, const char* field_key, const std::string& s)
{
	assert(lua_istable(state, -1));

	lua_pushlstring(state, s.c_str(), s.size());
	lua_rawsetfield(state, /*table index=*/-2, field_key);
}


#if BUILD_TESTS


#include "LuaVM.h"
#include "LuaScript.h"
#include "../utils/TestUtils.h"
#include "../utils/TestExceptionUtils.h"


void LuaUtils::test()
{
	conPrint("LuaUtils::test()");

	{
		try
		{
			LuaVM vm;
	
			const std::string src = "t = { x = 123.0, b=true, s = 'abc', v1d = {x=7, y=8, z=9}, v1f = Vec3f(7, 8, 9), m1 = {10, 11, 12, 13} }  " 
				"t2 = { 100, 200, 300, 'a' } v3 = Vec3f(1, 2, 3)  v3d = {x=101, y=102, z=103}  s2 = \"abc\" n = 567.0   m2 = {10, 11, 12, 13} b1 = true   b2 = false";
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();
			

			{ // Scope for pushing t
			lua_getglobal(script.thread_state, "t"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			const int initial_stack_size = lua_gettop(script.thread_state);

			//----------------------------------- Test getTableNumberField -----------------------------------
			testAssert(getTableNumberField(script.thread_state, /*table index=*/-1, "x") == 123.0);
			//testAssert(getTableNumberField(script.thread_state, /*table index=*/-1, "xdfgfdg") == 0.0);

			// Test on a table field that is not present
			testExceptionExpected([&]() { getTableNumberField(script.thread_state, /*table index=*/-1, "xdfgfdg"); });

			// Test on a table field that is not a number
			testExceptionExpected([&]() { getTableNumberField(script.thread_state, /*table index=*/-1, "s"); });

			// Test getTableNumberField on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableNumberField(script.thread_state, /*table index=*/-1, "xfsdfd"); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size


			//----------------------------------- Test getTableNumberFieldWithDefault -----------------------------------
			testAssert(getTableNumberFieldWithDefault(script.thread_state, /*table index=*/-1, "x", /*default val=*/-1.0) == 123.0);

			// Test on a table field that is not present
			testAssert(getTableNumberFieldWithDefault(script.thread_state, /*table index=*/-1, "xsdfdsf", /*default val=*/-1.0) == -1.0);

			// Test on a table field that is not a number
			testExceptionExpected([&]() { getTableNumberFieldWithDefault(script.thread_state, /*table index=*/-1, "s", /*default val=*/-1.0); });

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

			// Test with a field that is not a boolean
			testExceptionExpected([&]() { getTableBoolFieldWithDefault(script.thread_state, /*table index=*/-1, "x", /*default val=*/false); });
			testExceptionExpected([&]() { getTableBoolFieldWithDefault(script.thread_state, /*table index=*/-1, "x", /*default val=*/true); });
			testAssert(getTableBoolFieldWithDefault(script.thread_state, /*table index=*/-1, "xsdfsdf", /*default val=*/false) == false);
			testAssert(getTableBoolFieldWithDefault(script.thread_state, /*table index=*/-1, "xsdfsdf", /*default val=*/true) == true);

			// Test getTableBoolFieldWithDefault on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableBoolFieldWithDefault(script.thread_state, /*table index=*/-1, "x", /*default val=*/true); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size


			//----------------------------------- Test getTableStringField -----------------------------------
			testAssert(getTableStringField(script.thread_state, /*table index=*/-1, "s") == "abc");
			testExceptionExpected([&]() { getTableStringField(script.thread_state, /*table index=*/-1, "sxxsdfgdsfg"); });
			//testAssert(getTableStringField(script.thread_state, /*table index=*/-1, "x") == "123"); // Type conversion
			testExceptionExpected([&]() { getTableStringField(script.thread_state, /*table index=*/-1, "x"); }); // Test on value that is not a string

			// Test getTableStringField on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableStringField(script.thread_state, /*table index=*/-1, "x"); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size

			//----------------------------------- Test getTableStringFieldWithEmptyDefault -----------------------------------
			testAssert(getTableStringFieldWithEmptyDefault(script.thread_state, /*table index=*/-1, "s") == "abc");
			testAssert(getTableStringFieldWithEmptyDefault(script.thread_state, /*table index=*/-1, "sxxsdfgdsfg") == "");
			testExceptionExpected([&]() { getTableStringFieldWithEmptyDefault(script.thread_state, /*table index=*/-1, "x"); }); // Test on value that is not a string

			// Test getTableStringFieldWithEmptyDefault on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableStringFieldWithEmptyDefault(script.thread_state, /*table index=*/-1, "x"); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size


			//----------------------------------- Test getTableVec3dField -----------------------------------
			testAssert(getTableVec3dField(script.thread_state, /*table index=*/-1, "v1d") == Vec3d(7.0, 8.0, 9.0));
			testExceptionExpected([&]() { getTableVec3dField(script.thread_state, /*table index=*/-1, "vdfgdfg"); });

			// Test getTableVec3dField on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableVec3dField(script.thread_state, /*table index=*/-1, "x"); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size


			


			//----------------------------------- Test getTableVec3fFieldWithDefault -----------------------------------
			testAssert(getTableVec3fFieldWithDefault(script.thread_state, /*table index=*/-1, "v1f", /*default val=*/Vec3f(-1.f)) == Vec3f(7.0f, 8.0f, 9.0f));
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
			} // End scope for pushing t


			const int initial_stack_size = lua_gettop(script.thread_state);

			//----------------------------------- Test getBool -----------------------------------
			lua_getglobal(script.thread_state, "b1"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testAssert(getBool(script.thread_state, /*table index=*/-1) == true);
			lua_pop(script.thread_state, 1); // Pop b1

			lua_getglobal(script.thread_state, "b2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testAssert(getBool(script.thread_state, /*table index=*/-1) == false);
			lua_pop(script.thread_state, 1); // Pop b2

			// Test getBool on a value that is not a bool
			lua_getglobal(script.thread_state, "t2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testExceptionExpected([&]() { getBool(script.thread_state, /*table index=*/-1); });
			lua_pop(script.thread_state, 1); // Pop t2

			// Test getBool on a non-existent value
			lua_getglobal(script.thread_state, "dklfgjhdfkjgh"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testExceptionExpected([&]() { getBool(script.thread_state, /*table index=*/-1); });
			lua_pop(script.thread_state, 1); // Pop nil


			//----------------------------------- Test getFloat -----------------------------------
			lua_getglobal(script.thread_state, "n"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testAssert(getFloat(script.thread_state, /*table index=*/-1) == 567.f);
			lua_pop(script.thread_state, 1); // Pop n

			// Test getFloat on a value that is not a number
			lua_getglobal(script.thread_state, "t2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testExceptionExpected([&]() { getFloat(script.thread_state, /*table index=*/-1); });
			lua_pop(script.thread_state, 1); // Pop t2

			// Test getFloat on a non-existent value
			lua_getglobal(script.thread_state, "dklfgjhdfkjgh"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testExceptionExpected([&]() { getFloat(script.thread_state, /*table index=*/-1); });
			lua_pop(script.thread_state, 1); // Pop nil


			//----------------------------------- Test getDoubleArg -----------------------------------
			lua_getglobal(script.thread_state, "n"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testAssert(getDoubleArg(script.thread_state, /*table index=*/-1) == 567.f);
			lua_pop(script.thread_state, 1); // Pop n

			// Test getDoubleArg on a value that is not a number
			lua_getglobal(script.thread_state, "t2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testExceptionExpected([&]() { getDoubleArg(script.thread_state, /*table index=*/-1); });
			lua_pop(script.thread_state, 1); // Pop t2


			//----------------------------------- Test getVec3f -----------------------------------
			lua_getglobal(script.thread_state, "v3"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testAssert(getVec3f(script.thread_state, /*table index=*/-1) == Vec3f(1.0f, 2.0f, 3.0f));
			lua_pop(script.thread_state, 1); // Pop v3

			// Test getVec3f on a value that is not a Vec3f
			lua_getglobal(script.thread_state, "t2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testExceptionExpected([&]() { getVec3f(script.thread_state, /*table index=*/-1); });
			lua_pop(script.thread_state, 1); // Pop t2


			//----------------------------------- Test getVec3d -----------------------------------
			lua_getglobal(script.thread_state, "v3d"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testAssert(getVec3d(script.thread_state, /*table index=*/-1) == Vec3d(101, 102, 103));
			lua_pop(script.thread_state, 1); // Pop v3d

			// Test getVec3d on a value that is not a Vec3d
			lua_getglobal(script.thread_state, "t2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testExceptionExpected([&]() { getVec3d(script.thread_state, /*table index=*/-1); });
			lua_pop(script.thread_state, 1); // Pop t2


			//----------------------------------- Test getMatrix2f -----------------------------------
			lua_getglobal(script.thread_state, "m2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testAssert(getMatrix2f(script.thread_state, /*table index=*/-1) == Matrix2f(10.f, 11.f, 12.f, 13.f));
			lua_pop(script.thread_state, 1); // Pop m2

			// Test getMatrix2f on a value that is not a Matrix2f
			lua_getglobal(script.thread_state, "t2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testExceptionExpected([&]() { getMatrix2f(script.thread_state, /*table index=*/-1); });
			lua_pop(script.thread_state, 1); // Pop t2


			//----------------------------------- Test getString -----------------------------------
			lua_getglobal(script.thread_state, "s2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testAssert(getString(script.thread_state, /*table index=*/-1) == "abc");
			lua_pop(script.thread_state, 1); // Pop s2

			// Test getString on a value that is not a string
			lua_getglobal(script.thread_state, "t2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testExceptionExpected([&]() { getString(script.thread_state, /*table index=*/-1); });
			lua_pop(script.thread_state, 1); // Pop t2

			// Test getString on a non-existent value
			lua_getglobal(script.thread_state, "dklfgjhdfkjgh"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testExceptionExpected([&]() { getString(script.thread_state, /*table index=*/-1); });
			lua_pop(script.thread_state, 1); // Pop nil


			//----------------------------------- Test getStringConstCharPtr -----------------------------------
			lua_getglobal(script.thread_state, "s2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testAssert(::stringEqual(getStringConstCharPtr(script.thread_state, /*table index=*/-1), "abc"));
			lua_pop(script.thread_state, 1); // Pop s2

			// Test getStringConstCharPtr on a value that is not a string
			lua_getglobal(script.thread_state, "t2"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testExceptionExpected([&]() { getStringConstCharPtr(script.thread_state, /*table index=*/-1); });
			lua_pop(script.thread_state, 1); // Pop t2

			// Test getStringConstCharPtr on a non-existent value
			lua_getglobal(script.thread_state, "dklfgjhdfkjgh"); // Pushes onto the stack the value of the global name. Returns the type of that value.
			testExceptionExpected([&]() { getStringConstCharPtr(script.thread_state, /*table index=*/-1); });
			lua_pop(script.thread_state, 1); // Pop nil


			testAssert(lua_gettop(script.thread_state) == initial_stack_size); // Check stack has been returned to initial size



			lua_getglobal(script.thread_state, "t2"); // Pushes onto the stack the value of the global name. Returns the type of that value.


			//----------------------------------- Test getTableNumberArrayElem -----------------------------------
			testAssert(getTableNumberArrayElem(script.thread_state, /*table index=*/-1, 1) == 100.0);
			testAssert(getTableNumberArrayElem(script.thread_state, /*table index=*/-1, 2) == 200.0);
			testAssert(getTableNumberArrayElem(script.thread_state, /*table index=*/-1, 3) == 300.0);

			testExceptionExpected([&]() { getTableNumberArrayElem(script.thread_state, /*table index=*/-1, 4); }); // Test value not a number

			testExceptionExpected([&]() { getTableNumberArrayElem(script.thread_state, /*table index=*/-1, 100); }); // Test index out of bounds

			// Test getTableNumberArrayElem on a value that is not a table.
			lua_pushnumber(script.thread_state, 123.0);
			testExceptionExpected([&]() { getTableNumberArrayElem(script.thread_state, /*table index=*/-1, 1); });
			lua_pop(script.thread_state, 1); // Pop 123.0

			lua_pop(script.thread_state, 1); // Pop t2

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
