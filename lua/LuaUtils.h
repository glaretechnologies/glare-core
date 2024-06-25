/*=====================================================================
LuaUtils.h
----------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "../maths/vec3.h"
#include "../maths/Matrix2.h"
#include <lualib.h>


/*=====================================================================
LuaUtils
--------

=====================================================================*/
class LuaUtils
{
public:
	static void printTable(lua_State* state, int table_index, int spaces = 4);
	static void printStack(lua_State* state);

	static std::string getCallStackAsString(lua_State* state);

	static bool isFunctionDefined(lua_State* state, const char* func_name);
	static int getRefToFunction(lua_State* state, const char* func_name); // Returns LUA_NOREF if function was not defined.

	static void setCFunctionAsTableField(lua_State* state, lua_CFunction fn, const char* debugname, int table_index, const char* field_key);

	// Assumes table is on top of stack
	static inline void setLightUserDataAsTableField(lua_State* state, const char* field_key, void* val);
	// Assumes table is on top of stack
	static inline void setNumberAsTableField(lua_State* state, const char* field_key, double x);
	
	static void* getTableLightUserDataField(lua_State* state, int table_index, const char* key);

	static double getTableNumberField(lua_State* state, int table_index, const char* key);
	static double getTableNumberFieldWithDefault(lua_State* state, int table_index, const char* key, double default_val);
	static double getTableNumberArrayElem(lua_State* state, int table_index, const int elem_index);
	static bool tableHasBoolField(lua_State* state, int table_index, const char* key);
	static bool getTableBoolFieldWithDefault(lua_State* state, int table_index, const char* key, bool default_val);
	
	// Get a const char* pointer to a string value on the Lua stack.  Only valid for the lifetime of the stack object.
	// Throws exception if value on stack is not a string.
	static const char* getStringConstCharPtr(lua_State* state, int index);
	static const char* getStringAndAtom(lua_State* state, int index, int& atom_out);
	// Convert a Lua string on the stack at index into a std::string
	static std::string getString(lua_State* state, int index);
	static std::string getTableStringField(lua_State* state, int table_index, const char* key);
	static std::string getTableStringFieldWithEmptyDefault(lua_State* state, int table_index, const char* key);

	static bool getBool(lua_State* state, int index);
	static float getFloat(lua_State* state, int index);
	static double getDouble(lua_State* state, int index);
	// Convert a Vec3d on the Lua stack at the given index to a Vec3d.  Does not alter Lua stack.
	static Vec3f getVec3f(lua_State* state, int index);
	static Vec3d getVec3d(lua_State* state, int index);
	static Matrix2f getMatrix2f(lua_State* state, int index);
	static Vec3d getTableVec3dField(lua_State* state, int table_index, const char* key);
	static Vec3f getTableVec3fFieldWithDefault(lua_State* state, int table_index, const char* key, const Vec3f& default_val);
	static Matrix2f getTableMatrix2fFieldWithDefault(lua_State* state, int table_index, const char* key, const Matrix2f& default_val);

	static void pushVec3f(lua_State* state, const Vec3f& v);
	static void pushVec3d(lua_State* state, const Vec3d& v);
	static void pushMatrix2f(lua_State* state, const Matrix2f& m);
	static inline void pushString(lua_State* state, const std::string& s);
	

	static void test();
};



// Assumes table is on top of stack.
inline void LuaUtils::setLightUserDataAsTableField(lua_State* state, const char* field_key, void* val)
{
	lua_pushlightuserdata(state, val);
	lua_rawsetfield(state, /*table index=*/-2, field_key);
}


// Assumes table is on top of stack.
void LuaUtils::setNumberAsTableField(lua_State* state, const char* field_key, double x)
{
	lua_pushnumber(state, x);
	lua_rawsetfield(state, /*table index=*/-2, field_key);
}


void LuaUtils::pushString(lua_State* state, const std::string& s)
{
	lua_pushlstring(state, s.c_str(), s.size());
}
