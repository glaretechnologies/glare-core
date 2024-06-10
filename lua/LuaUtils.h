/*=====================================================================
LuaUtils.h
----------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "../maths/vec3.h"
#include "../maths/Matrix2.h"
struct lua_State;
typedef int (*lua_CFunction)(lua_State* L);


/*=====================================================================
LuaUtils
--------

=====================================================================*/
class LuaUtils
{
public:
	static void printTable(lua_State* state, int table_index, int spaces = 4);
	static void printStack(lua_State* state);

	static void setCFunctionAsTableField(lua_State* state, lua_CFunction fn, const char* debugname, int table_index, const char* field_key);

	static double getTableNumberField(lua_State* state, int table_index, const char* key);
	static double getTableNumberFieldWithDefault(lua_State* state, int table_index, const char* key, double default_val);
	static double getTableNumberArrayElem(lua_State* state, int table_index, const int elem_index);
	static bool tableHasBoolField(lua_State* state, int table_index, const char* key);
	static bool getTableBoolFieldWithDefault(lua_State* state, int table_index, const char* key, bool default_val);
	static std::string getTableStringField(lua_State* state, int table_index, const char* key);
	static Vec3d getTableVec3dField(lua_State* state, int table_index, const char* key);
	static Vec3f getTableVec3fFieldWithDefault(lua_State* state, int table_index, const char* key, const Vec3f& default_val);
	static Matrix2f getTableMatrix2fFieldWithDefault(lua_State* state, int table_index, const char* key, const Matrix2f& default_val);

	static void test();
};
