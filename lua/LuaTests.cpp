/*=====================================================================
LuaTests.cpp
------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "LuaTests.h"


#include "LuaVM.h"
#include "LuaScript.h"
#include "LuaUtils.h"
#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/FileUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include <lualib.h>
#include <luacode.h>





// Adds two numbers together
static int testFunc(lua_State* state)
{
	const int a = lua_tointeger(state, 1); // First argument
	const int b = lua_tointeger(state, 2); // Second argument
	const int result = a + b;


	lua_pushinteger(state, result);

	return 1; // Count of returned values
}


static int testCallStackPrintFunc(lua_State* state)
{
	conPrint(LuaUtils::getCallStackAsString(state));

	return 0; // Count of returned values
}


//static int createObjectLuaFunc(lua_State* state)
//{
//	conPrint("----createObjectLuaFunc()----");
//	LuaUtils::printStack(state);
//
//	int val_type = lua_getfield(state, /*index=*/1, /*field key=*/"name");
//	if(val_type == LUA_TSTRING)
//	{
//		size_t stringlen = 0;
//		//const char* str = luaL_checklstring(state, /*index=*/-1, &stringlen);
//		const char* str = lua_tolstring(state, /*index=*/-1, &stringlen); // May return NULL if not a string
//
//		LuaUtils::printStack(state);
//		if(str)
//		{
//			//size_t stringlen = 0;
//			//const char* str = lua_tolstring(state, /*index=*/1, &stringlen);
//
//			conPrint("str: " + std::string(str, stringlen));
//		}
//	}
//	lua_pop(state, 1); // Pop the string off the stack
//	LuaUtils::printStack(state);
//
//	val_type = lua_getfield(state, /*index=*/1, /*field key=*/"mass");
//	if(val_type == LUA_TNUMBER)
//	{
//		const double mass = lua_tonumber(state, /*index=*/-1);
//		printVar(mass);
//	}
//	lua_pop(state, 1); // Pop mass value off stack
//
//	LuaUtils::printStack(state);
//
//	return 0;
//}


// Assume that table is at the top of stack.  From https://www.lua.org/pil/25.1.html
//static void setNumberField(lua_State* state, const char* key, double value)
//{
//	lua_pushstring(state, key);
//	lua_pushnumber(state, value);
//	lua_settable(state, /*table index=*/-3);
//}


static int testMethod(lua_State* state)
{
	// Arg 1: table
	testAssert(lua_istable(state, 1));

	// Arg 2: number
	testAssert(lua_isnumber(state, 2));

	const double x = lua_tonumber(state, 2); // Second argument
	const double result = x * 2;

	lua_pushnumber(state, result);

	return 1; // Count of returned values
}


// C++ implementation of __index.  Used when a table field is read from.
static int glareLuaIndexMetaMethod(lua_State* state)
{
	testAssert(lua_istable(state, 1));

	// Assuming arg 1 is table, get uid field from it
	lua_rawgetfield(state, 1, "uid"); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)

	//int isnum;
	//const double uid = lua_tonumberx(state, -1, &isnum);
	lua_pop(state, 1); // Pop uid from stack

	size_t stringlen = 0;
	const char* key_str = lua_tolstring(state, /*index=*/2, &stringlen); // May return NULL if not a string

	if(key_str)
	{
		if(stringEqual(key_str, "testMethod"))
		{
			lua_pushcfunction(state, testMethod, "testMethod");
			return 1;
		}
		else if(stringEqual(key_str, "testattribute"))
		{
			lua_pushnumber(state, 100.0);
			return 1;
		}
	}

	lua_pushnil(state);
	return 1; // Count of returned values
}


// C++ implementation of __namecall.  Used when a table method is called.
#if 0
static int glareLuaNameCallMetaMethod(lua_State* state)
{
	testAssert(lua_istable(state, 1));

	// Assuming arg 1 is table, get uid field from it
	lua_rawgetfield(state, 1, "uid"); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)

	//int isnum;
	//const double uid = lua_tonumberx(state, -1, &isnum);
	lua_pop(state, 1); // Pop uid from stack

	size_t stringlen = 0;
	const char* key_str = lua_tolstring(state, /*index=*/2, &stringlen); // May return NULL if not a string

	if(key_str)
	if(stringEqual(key_str, "testMethod"))
	{
		lua_pushcfunction(state, testMethod, "testMethod");
		return 1;
	}

	lua_pushnil(state);
	return 1; // Count of returned values
}
#endif


// C++ implementation of __newindex.  Used when a value is assigned to a table field.
static int glareLuaNewIndexMetaMethod(lua_State* state)
{
	testAssert(lua_istable(state, 1));

	// Assuming arg 1 is table, get uid field from it
	lua_rawgetfield(state, 1, "uid"); // Push field value onto stack (use lua_rawgetfield to avoid metamethod call)

	//int isnum;
	//const double uid = lua_tonumberx(state, -1, &isnum);
	lua_pop(state, 1); // Pop uid from stack

	// Read key
	size_t stringlen = 0;
	/*const char* key_str =*/ lua_tolstring(state, /*index=*/2, &stringlen); // May return NULL if not a string

	// Read newly assigned value.
	//const double value = lua_tonumberx(state, 3, &isnum);

	return 0; // Count of returned values
}


//========================== Fuzzing ==========================
#if 0
// Command line:
// C:\fuzz_corpus\lua C:\code\glare-core\testfiles\lua\fuzz_seeds

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	LuaVM::staticInit();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		LuaVMOptions vm_options;
		vm_options.max_total_mem_allowed = 1024 * 1024;
		LuaVM vm(vm_options);
		
		LuaScriptOptions options;
		options.max_num_interrupts = 64000;

		const std::string src((const char*)data, size);
		LuaScript script(&vm, options, src);
		script.exec();
	}
	catch(glare::Exception& )
	{
		//conPrint("Excep: " + e.what());
	}

	return 0; // Non-zero return values are reserved for future use.
}
#endif
//========================== End fuzzing ==========================


class TestLuaScriptOutputHandler : public LuaScriptOutputHandler
{
public:
	virtual void printFromLuaScript(LuaScript* lua_script, const char* s, size_t len) override
	{
		buf += std::string(s, len);
	}

	std::string buf;
};
class PrinterTestLuaScriptOutputHandler : public LuaScriptOutputHandler
{
public:
	virtual void printFromLuaScript(LuaScript* lua_script, const char* s, size_t len) override
	{
		conPrint(std::string(s, len));
		buf += std::string(s, len);
	}
	std::string buf;
};


void LuaTests::test()
{
	conPrint("LuaTests::test()");

	try
	{
		//========================== Test creating and destroying VM ==========================
		try
		{
			LuaVM vm;
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}
	
		//==========================  Test with an empty script ==========================
		try
		{
			LuaVM vm;
	
			const std::string src = "";
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}
	
		//========================== Test getRefToFunctionOnStack ==========================
		{
			LuaVM vm;
			LuaScript script(&vm, LuaScriptOptions(), "function double(x) return x * 2 end");
			script.exec();
			lua_State* state = script.thread_state;
			const int initial_refs = vm.getNumLuaValueReferences();

			lua_getglobal(state, "double"); // Push the double function
			const LuaVM::LuaValueRef ref = script.getRefToFunctionOnStack(state, /*stack_index=*/-1); // Does not pop
			lua_pop(state, 1); // Pop function
			testAssert(ref.ref > LUA_REFNIL && vm.getNumLuaValueReferences() == initial_refs + 1);

			// Check the ref refers to the double function by calling it: double(21) should return 42.
			lua_getref(state, ref.ref); // Push function
			lua_pushnumber(state, 21); // Push arg
			lua_call(state, /*nargs=*/1, /*nresults=*/1); // Pops function and arg, pushes result
			testAssert(lua_tonumber(state, -1) == 42);
			lua_pop(state, 1); // Pop result

			script.freeRefToFunction(ref);
			testAssert(vm.getNumLuaValueReferences() == initial_refs);
		}

		// A function referenced from a child coroutine can be retrieved and released through the script's root state.
		{
			LuaVM vm;
			const std::string src =
				"weak = setmetatable({}, {__mode = 'v'})\n"
				"co = coroutine.create(function()\n"
				"    local captured = {offset = 7}\n"
				"    local fn = function(x) return x + captured.offset end\n"
				"    weak[1] = fn\n"
				"    return fn\n"
				"end)";
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();
			lua_State* root = script.thread_state;
			const int root_top = lua_gettop(root);
			const int initial_refs = vm.getNumLuaValueReferences();

			lua_getglobal(root, "co");
			lua_State* child = lua_tothread(root, -1);
			testAssert(child && child != root);
			lua_pop(root, 1); // The global co keeps the child alive.
			testAssert(lua_resume(child, root, 0) == LUA_OK);
			testAssert(lua_gettop(child) == 1 && lua_isfunction(child, -1));
			const void* function_ptr = lua_topointer(child, -1);

			const LuaVM::LuaValueRef ref = script.getRefToFunctionOnStack(child, -1);
			testAssert(lua_gettop(child) == 1 && lua_gettop(root) == root_top);
			testAssert(lua_topointer(child, -1) == function_ptr);
			testAssert(vm.getNumLuaValueReferences() == initial_refs + 1);

			// Remove the coroutine's results and frames so only the registry reference retains the function.
			lua_resetthread(child);
			const int child_top = lua_gettop(child);
			lua_gc(root, LUA_GCCOLLECT, 0);

			lua_getref(root, ref.ref);
			testAssert(lua_isfunction(root, -1) && lua_topointer(root, -1) == function_ptr);
			lua_pushnumber(root, 35);
			lua_call(root, 1, 1);
			testAssert(lua_tonumber(root, -1) == 42);
			lua_pop(root, 1);
			testAssert(lua_gettop(root) == root_top && lua_gettop(child) == child_top);

			script.freeRefToFunction(ref); // Uses the root state, although the reference was created on child.
			testAssert(vm.getNumLuaValueReferences() == initial_refs);
			testAssert(lua_gettop(root) == root_top && lua_gettop(child) == child_top);

			// Check the release really removed the strong reference, independently of the C++ tracking count.
			lua_gc(root, LUA_GCCOLLECT, 0);
			lua_getglobal(root, "weak");
			lua_rawgeti(root, -1, 1);
			testAssert(lua_isnil(root, -1));
			lua_pop(root, 2);
			testAssert(lua_gettop(root) == root_top && lua_gettop(child) == child_top);
		}

		//========================== Test getOptionalRefToFunctionInTable ==========================
		{
			LuaVM vm;
			const std::string src =
				"opts = {\n"
				"    onChange = function(x) return x * 2 end,\n"
				"    notFunc = 5,\n"
				"    strField = 'abc',\n"
				"    tableField = {},\n"
				"}\n"
				"withMeta = setmetatable({}, {__index = {onChange = function() end}})";
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();
			lua_State* state = script.thread_state;
			const int initial_top = lua_gettop(state);
			const int initial_refs = vm.getNumLuaValueReferences();

			lua_getglobal(state, "opts"); // Push opts table, at index initial_top + 1
			const int opts_index = lua_gettop(state);

			// Function field: returns a valid, tracked ref, and leaves the stack unchanged.
			{
				const LuaVM::LuaValueRef ref = script.getOptionalRefToFunctionInTable(state, opts_index, "onChange");
				testAssert(ref.ref > LUA_REFNIL);
				testAssert(vm.getNumLuaValueReferences() == initial_refs + 1);
				testAssert(lua_gettop(state) == opts_index);

				// Check the ref refers to the onChange function by calling it: onChange(21) should return 42.
				lua_getref(state, ref.ref); // Push function
				testAssert(lua_isfunction(state, -1));
				lua_pushnumber(state, 21); // Push arg
				lua_call(state, /*nargs=*/1, /*nresults=*/1); // Pops function and arg, pushes result
				testAssert(lua_tonumber(state, -1) == 42);
				lua_pop(state, 1); // Pop result

				script.freeRefToFunction(ref);
				testAssert(vm.getNumLuaValueReferences() == initial_refs);
				testAssert(lua_gettop(state) == opts_index);
			}

			// Negative (relative) table index.
			{
				lua_pushnil(state); // Push a dummy value so the table is at -2.
				const LuaVM::LuaValueRef ref = script.getOptionalRefToFunctionInTable(state, /*table_index=*/-2, "onChange");
				testAssert(ref.ref > LUA_REFNIL);
				testAssert(lua_gettop(state) == opts_index + 1); // Stack is unchanged: opts table, then the dummy nil.
				lua_pop(state, 1); // Pop dummy nil

				script.freeRefToFunction(ref);
				testAssert(vm.getNumLuaValueReferences() == initial_refs);
			}

			// Missing field: returns LUA_NOREF and creates no ref.
			{
				const LuaVM::LuaValueRef ref = script.getOptionalRefToFunctionInTable(state, opts_index, "noSuchField");
				testAssert(ref.ref == LUA_NOREF);
				testAssert(vm.getNumLuaValueReferences() == initial_refs);
				testAssert(lua_gettop(state) == opts_index);

				script.freeRefToFunction(ref); // Should do nothing for LUA_NOREF.
				testAssert(vm.getNumLuaValueReferences() == initial_refs);
			}

			// Non-function fields: throw, with the key name in the message, and create no ref.
			const char* bad_keys[] = { "notFunc", "strField", "tableField" };
			for(size_t i=0; i<staticArrayNumElems(bad_keys); ++i)
			{
				try
				{
					script.getOptionalRefToFunctionInTable(state, opts_index, bad_keys[i]);
					failTest("Expected exception for key " + std::string(bad_keys[i]));
				}
				catch(glare::Exception& e)
				{
					testAssert(StringUtils::containsString(e.what(), "'" + std::string(bad_keys[i]) + "' must be a function"));
				}
				testAssert(vm.getNumLuaValueReferences() == initial_refs);
				testAssert(lua_gettop(state) == opts_index); // Stack is unchanged: field value was popped before throwing.
			}

			lua_settop(state, initial_top); // Pop opts table

			// The lookup is raw, so a function only reachable through __index is not found.
			{
				lua_getglobal(state, "withMeta"); // Push withMeta table
				const LuaVM::LuaValueRef ref = script.getOptionalRefToFunctionInTable(state, /*table_index=*/-1, "onChange");
				testAssert(ref.ref == LUA_NOREF);
				testAssert(vm.getNumLuaValueReferences() == initial_refs);
				testAssert(lua_gettop(state) == initial_top + 1); // Stack is unchanged: just the withMeta table.
				lua_pop(state, 1); // Pop withMeta table
			}
		}

		//========================== Test with a simple script ==========================
		try
		{
			LuaVM vm;
	
			const std::string src = "print('hello')";
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

		//========================== Test compilation failure due to a syntax error ==========================
		try
		{
			LuaVM vm;

			const std::string src = "print('hello'";
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();

			failTest("Expected excep.");
		}
		catch(glare::Exception& e)
		{
			// Expected
			conPrint("Got expected excep: " + e.what());
		}

		//========================== Test creating another script after compilation failure ==========================
		try
		{
			LuaVM vm;

			try
			{
				const std::string src = "print('hello'";
				LuaScript script(&vm, LuaScriptOptions(), src);
				script.exec();
				failTest("Expected excep.");
			}
			catch(glare::Exception& e)
			{
				conPrint("Got expected excep: " + e.what());
			}

			{
				const std::string src = "print('hello')";
				LuaScript script(&vm, LuaScriptOptions(), src);
				script.exec();
			}
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

		//========================== Test vector (Vec3f) type ==========================
		try
		{
			LuaVM vm;

			PrinterTestLuaScriptOutputHandler output_handler;
			LuaScriptOptions options;
			options.script_output_handler = &output_handler;

			//const std::string src = "print(v)  \n  print(v.Y)  \n  print(v.Z)";
			//const std::string src = "print(tostring(v.x))  \n  print(tostring(v.y))  \n  print(tostring(v.z))";
			//const std::string src = "local v = vector(1, 2, 3)  \n   local a = v.x    \n    ";
			const std::string src = "function testVecFunc(v : Vec3f) print(tostring(v.y)) end ";
			LuaScript script(&vm, options, src);
			script.exec();
			
			lua_getglobal(script.thread_state, "testVecFunc");  // Push function to be called onto stack
			lua_pushvector(script.thread_state, 10.0, 20.0, 30.0);
			lua_call(script.thread_state, /*nargs=*/1, /*nresults=*/0);

			testAssert(output_handler.buf == "20");
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}
		try
		{
			LuaVM vm;

			PrinterTestLuaScriptOutputHandler output_handler;
			LuaScriptOptions options;
			options.script_output_handler = &output_handler;

			{
			//const std::string src = "local v = Vec3f(10, 20, 30)      print(v.z)";
			const std::string src = "local v1 = Vec3f(10, 20, 30)   local v2 = Vec3f(1, 2, 3)   local v3 = v1 + v2     print(v3.z)";
			LuaScript script(&vm, options, src);
			script.exec();
			testAssert(output_handler.buf == "33");
			}

			// Test other operators
			{
			const std::string src = 
				"local v1 = Vec3f(10, 20, 30)   \n"
				"local v2 = Vec3f(1, 2, 3)          \n"
				"local v3 = v1 + v2     assert(v3.y == 22.0)     \n"
				"local v4 = v1 - v2     assert(v4.y == 18.0)     \n"
				"local v5 = -v1			assert(v5.y == -20.0)    \n" // Check unary minus
				"assert(v1 == Vec3f(10, 20, 30))                 \n" // Check equality operator
				"assert(v1 == v1)                                \n" // Check equality operator
				"assert(v1 + v2 == Vec3f(11, 22, 33))            \n" // Check equality operator
				"";
			LuaScript script(&vm, options, src);
			script.exec();
			}
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

		//========================== Test Vec3d type ==========================
		// Test Vec3d constructor
		try
		{
			LuaVM vm;

			PrinterTestLuaScriptOutputHandler output_handler;
			LuaScriptOptions options;
			options.script_output_handler = &output_handler;

			const std::string src = "local v1 = Vec3d(10, 20, 30) print(v1.z)";
			LuaScript script(&vm, options, src);
			script.exec();
			
			testAssert(output_handler.buf == "30");
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

		// Test addition operator etc.
		try
		{
			LuaVM vm;

			PrinterTestLuaScriptOutputHandler output_handler;
			LuaScriptOptions options;
			options.script_output_handler = &output_handler;

			{
			output_handler.buf.clear();
			const std::string src = "local v1 = Vec3d(10, 20, 30)   local v2 = Vec3d(1, 2, 3)   local v3 = v1 + v2     print(v3.y)";
			LuaScript script(&vm, options, src);
			script.exec();
			testAssert(output_handler.buf == "22");
			}
			{
			const std::string src = 
				"local v1 = Vec3d(10, 20, 30)   \n"
				"local v2 = Vec3d(1, 2, 3)          \n"
				"local v3 = v1 + v2     assert(v3.y == 22.0)     \n" // v3 = (11, 22, 33)
				"local v4 = v1 - v2     assert(v4.y == 18.0)     \n"
				"local v5 = -v1			assert(v5.y == -20.0)    \n" // Check unary minus
				"assert(v1 == Vec3d(10, 20, 30))                 \n" // Check equality operator
				"assert(v1 == v1)                                \n" // Check equality operator
				"assert(v1 + v2 == Vec3d(11, 22, 33))            \n" // Check equality operator
				"local v6 = v3 + v1     assert(v6 == Vec3d(21, 42, 63))     \n" // Check operators work on Vec3d returned from operator.
				"assert(v2 * 2 == Vec3d(2, 4, 6))                \n" // Check scalar multiplication
				"assert(2 * v2 == Vec3d(2, 4, 6))                \n"
				"assert((v1 + v2) * 0.5 == Vec3d(5.5, 11, 16.5)) \n"
				"";
			LuaScript script(&vm, options, src);
			script.exec();
			}
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

		//========================== Test Vec2 type ==========================
		try
		{
			LuaVM vm;

			PrinterTestLuaScriptOutputHandler output_handler;
			LuaScriptOptions options;
			options.script_output_handler = &output_handler;

			{
			const std::string src = "local v1 = Vec2(10, 20) print(v1.y)";
			LuaScript script(&vm, options, src);
			script.exec();
			testAssert(output_handler.buf == "20");
			}
			{
			const std::string src =
				"local v1 = Vec2(10, 20)         \n"
				"local v2 = Vec2(1, 2)           \n"
				"local v3 = v1 + v2     assert(v3.x == 11.0 and v3.y == 22.0)     \n"
				"local v4 = v1 - v2     assert(v4.x == 9.0  and v4.y == 18.0)     \n"
				"local v5 = -v1         assert(v5.x == -10.0 and v5.y == -20.0)   \n" // Check unary minus
				"assert(v1 == Vec2(10, 20))                      \n" // Check equality operator
				"assert(v1 ~= Vec2(10, 21))                      \n"
				"assert(v1 + v2 == Vec2(11, 22))                 \n"
				"local v6 = v3 + v1     assert(v6 == Vec2(21, 42))     \n" // Check operators work on Vec2 returned from operator.
				"assert(v2 * 2 == Vec2(2, 4))                    \n" // Check scalar multiplication
				"assert(2 * v2 == Vec2(2, 4))                    \n"
				"assert((v1 + v2) * 0.5 == Vec2(5.5, 11))        \n"
				"";
			LuaScript script(&vm, options, src);
			script.exec();
			}
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

		//========================== Test accessing lua_state->userdata from a coroutine ==========================
		// Vec2 construction needs the owning LuaScript on the coroutine's own lua_State.
		{
			LuaVM vm;
			const std::string src =
				"local make_vec = coroutine.wrap(function() return Vec2(3, 4) end)\n"
				"local v = make_vec()\n"
				"assert(v.x == 3 and v.y == 4)";
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();
		}

		//========================== Test maximum memory usage hit during initial state alloc ==========================
		// Allow the initial state allocation, then hit the limit while lua_newstate is initialising it.
		{
			size_t initial_state_size = 0;
			const auto measure_initial_allocation = [](void* userdata, void*, size_t, size_t new_size) -> void* {
				*static_cast<size_t*>(userdata) = new_size;
				return nullptr;
			};
			lua_State* probe = lua_newstate(measure_initial_allocation, &initial_state_size);
			testAssert(probe == nullptr && initial_state_size > 0);

			LuaVMOptions vm_options;
			vm_options.max_total_mem_allowed = (int64)initial_state_size;
			try
			{
				LuaVM vm(vm_options);
				failTest("Expected lua_newstate to fail during initialisation.");
			}
			catch(glare::Exception& e)
			{
				testAssert(e.what() == "lua_newstate failed.");
			}
		}

		//========================== Test maximum memory usage ==========================
		try
		{
			LuaVMOptions vm_options;
			vm_options.max_total_mem_allowed = 500000;
			LuaVM vm(vm_options);

			LuaScriptOptions options;

			const std::string src = "my_table = {} \n for i=1, 100000000 do  my_table[i] = 'boo' end ";
			LuaScript script(&vm, options, src);
			script.exec();

			failTest("Expected excep.");
		}
		catch(glare::Exception& e)
		{
			// Expected
			conPrint("Got expected excep: " + e.what());
		}

		//========================== Test maximum memory calling a Lua function ==========================
		try
		{
			LuaVMOptions vm_options;
			vm_options.max_total_mem_allowed = 500000;
			LuaVM vm(vm_options);

			LuaScriptOptions options;

			const std::string src = 
				"function f()  \n"
				"	local my_table = {} \n for i=1, 100000000 do  my_table[i] = 'boo' end\n"
				"end\n";
			LuaScript script(&vm, options, src);
			script.exec();

			lua_getglobal(script.thread_state, "f");
			lua_call(script.thread_state, /*nargs=*/0, /*nresults=*/0); // Pops all arguments and function value

			failTest("Expected excep.");
		}
		catch(std::exception& e)
		{
			conPrint("Got expected excep: " + std::string(e.what()));
		}

		//========================== Test maximum memory calling a Lua function with recovery ==========================
		try
		{
			LuaVMOptions vm_options;
			vm_options.max_total_mem_allowed = 500000;
			LuaVM vm(vm_options);

			LuaScriptOptions options;

			const std::string src = 
				"function f()  \n"
				"	local my_table = {} \n for i=1, 100000000 do  my_table[i] = 'boo' end\n"
				"end\n"
				"function g()  \n"
				"	print('hello') \n"
				"end\n";

			// Create a LuaScript, call f, should throw exception
			{
				LuaScript script(&vm, options, src);
				script.exec();

				try
				{
					lua_getglobal(script.thread_state, "f"); // pushes onto stack
					lua_call(script.thread_state, /*nargs=*/0, /*nresults=*/0); // Pops all arguments and function value

					failTest("Expected excep.");
				}
				catch(std::exception& e)
				{
					conPrint("Got expected excep: " + std::string(e.what()));
				}
			}

			// Create a LuaScript, call g, should work fine.
			{
				LuaScript script(&vm, options, src);
				script.exec();

				try
				{
					lua_getglobal(script.thread_state, "g"); // pushes onto stack
					lua_call(script.thread_state, /*nargs=*/0, /*nresults=*/0); // Pops all arguments and function value
				}
				catch(std::exception& e)
				{
					failTest(std::string(e.what()));
				}
				catch(glare::Exception& e)
				{
					failTest(e.what());
				}
			}
		}
		catch(glare::Exception& e)
		{
			conPrint("Got expected excep: " + e.what());
		}



		//========================== Test maximum execution time ==========================
		try
		{
			LuaVM vm;

			LuaScriptOptions options;
			options.max_num_interrupts = 10000;

			const std::string src = "my_table = {} \n for i=1, 100000000 do  my_table[0] = 'boo' end ";
			LuaScript script(&vm, options, src);
			script.exec();

			failTest("Expected excep.");
		}
		catch(glare::Exception& e)
		{
			// Expected
			conPrint("Got expected excep: " + e.what());
		}

		//========================== Test maximum execution time calling a Lua function ==========================
		try
		{
			LuaVM vm;
			LuaScriptOptions options;
			options.max_num_interrupts = 10000;

			const std::string src = 
				"function f()  \n"
				"	local my_table = {} \n for i=1, 100000000 do  my_table[0] = 'boo' end\n"
				"end\n";
			LuaScript script(&vm, options, src);
			script.exec();

			lua_getglobal(script.thread_state, "f");
			lua_call(script.thread_state, /*nargs=*/0, /*nresults=*/0); // Pops all arguments and function value

			failTest("Expected excep.");
		}
		catch(glare::Exception& e)
		{
			// Expected
			conPrint("Got expected excep: " + e.what());
		}

		//========================== Test maximum execution time calling a Lua function and recovery ==========================
		// Test recovery in the same LuaScript (Lua thread).  Does not work.
		try
		{
			LuaVM vm;
			LuaScriptOptions options;
			options.max_num_interrupts = 10000;

			const std::string src = 
				"function f()  \n"
				"	local my_table = {} \n for i=1, 100000000 do  my_table[0] = 'boo' end\n"
				"end\n"
				"function g()  \n"
				"	print('hello') \n"
				"end\n";
			LuaScript script(&vm, options, src);
			script.exec();

			try
			{
				lua_getglobal(script.thread_state, "f"); // pushes onto stack
				lua_call(script.thread_state, /*nargs=*/0, /*nresults=*/0); // Pops all arguments and function value

				failTest("Expected excep.");
			}
			catch(std::exception& e)
			{
				conPrint("Got expected excep: " + std::string(e.what()));
			}
			catch(glare::Exception& e)
			{
				conPrint("Got expected excep: " + e.what());
			}

			// Now continue and call g
			// This doesn't work, hits assert.  This is fine actually.
			// 
			//lua_getglobal(script.thread_state, "g"); // pushes onto stack
			//lua_call(script.thread_state, /*nargs=*/0, /*nresults=*/0); // Pops all arguments and function value
		}
		catch(glare::Exception& e)
		{
			// Expected
			conPrint("Got expected excep: " + e.what());
		}

		//========================== Test maximum execution time calling a Lua function and recovery ==========================
		// Test recovery in a different LuaScript (Lua thread).
		try
		{
			LuaVM vm;
			LuaScriptOptions options;
			options.max_num_interrupts = 10000;

			const std::string src = 
				"function f()  \n"
				"	local my_table = {} \n for i=1, 100000000 do  my_table[0] = 'boo' end\n"
				"end\n"
				"function g()  \n"
				"	print('hello') \n"
				"end\n";

			// Create a LuaScript, call f, should throw exception
			{
				LuaScript script(&vm, options, src);
				script.exec();

				try
				{
					lua_getglobal(script.thread_state, "f"); // pushes onto stack
					lua_call(script.thread_state, /*nargs=*/0, /*nresults=*/0); // Pops all arguments and function value

					failTest("Expected excep.");
				}
				catch(glare::Exception& e)
				{
					conPrint("Got expected excep: " + e.what());
				}
			}

			// Create a LuaScript, call g, should work fine.
			{
				LuaScript script(&vm, options, src);
				script.exec();

				try
				{
					lua_getglobal(script.thread_state, "g"); // pushes onto stack
					lua_call(script.thread_state, /*nargs=*/0, /*nresults=*/0); // Pops all arguments and function value
				}
				catch(glare::Exception& e)
				{
					failTest(e.what());
				}
			}
		}
		catch(glare::Exception& e)
		{
			// Expected
			conPrint("Got expected excep: " + e.what());
		}

		//========================== Test maximum execution time and resetExecutionTimeCounter() ==========================
		try
		{
			LuaVM vm;
			LuaScriptOptions options;
			options.max_num_interrupts = 10000;

			const std::string src = 
				"function f()  \n"
				"	local my_table = {} my_table[0] = 1 \n"
				"end\n";

			// Try repeatedly executing a lua script function, we should reach max_num_interrupts
			try
			{
				LuaScript script(&vm, options, src);
				script.exec();

				for(int i=0; i<10000; ++i)
				{
					lua_getglobal(script.thread_state, "f");
					lua_call(script.thread_state, /*nargs=*/0, /*nresults=*/0); // Pops all arguments and function value
				}
			}
			catch(glare::Exception& e)
			{
				conPrint("Got expected excep: " + e.what());
			}

			// Try repeatedly executing a lua script function, but with calling resetExecutionTimeCounter() before each call.  We should not reach max_num_interrupts.
			{
				LuaScript script(&vm, options, src);
				script.exec();

				for(int i=0; i<10000; ++i)
				{
					script.resetExecutionTimeCounter();
					lua_getglobal(script.thread_state, "f");
					lua_call(script.thread_state, /*nargs=*/0, /*nresults=*/0); // Pops all arguments and function value
				}
			}
		}
		catch(glare::Exception& e)
		{
			failTest(e.what());
		}

		//========================== Test assert() ==========================
		try
		{
			LuaVM vm;

			const std::string src = "assert(false, 'assert failed')";
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();

			failTest("Expected excep.");
		}
		catch(glare::Exception& e)
		{
			conPrint("Got expected excep: " + e.what()); // Expected
		}

		//========================== Test error() ==========================
		try
		{
			LuaVM vm;

			const std::string src = "error('error called woop woop')";
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();

			failTest("Expected excep.");
		}
		catch(glare::Exception& e)
		{
			conPrint("Got expected excep: " + e.what()); // Expected
		}

		//========================== Test calling a Lua function from lua ==========================
		try
		{
			LuaVM vm;

			const std::string src = "function f(x) return x + 1 end    local y = f(1)  print(tostring(y))";

			TestLuaScriptOutputHandler handler;
			LuaScriptOptions options;
			options.script_output_handler = &handler;

			LuaScript script(&vm, options, src);
			script.exec();

			testAssert(handler.buf == "2");
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

		//========================== Test calling a Lua function from C ==========================
		try
		{
			LuaVM vm;

			const std::string src = "function f(x) return x + 1 end";

			TestLuaScriptOutputHandler handler;
			LuaScriptOptions options;
			LuaScript script(&vm, options, src);
			script.exec();

			Timer timer;
			const int num_iters = 1000;
			for(int i=0; i<num_iters; ++i)
			{
				lua_getglobal(script.thread_state, "f"); // pushes onto stack
				lua_pushnumber(script.thread_state, 10.0);
				lua_call(script.thread_state, /*nargs=*/1, /*nresults=*/1);

				testAssert(lua_isnumber(script.thread_state, -1));
				testAssert(lua_tonumber(script.thread_state, -1) == 11.0);
				lua_pop(script.thread_state, 1);
			}
			conPrint("calling function via name string took " + doubleToStringNSigFigs(timer.elapsed() / num_iters * 1.0e9, 5) + " ns");


			//------------- Try calling using a reference to the function -------------

			// Get reference to function
			lua_getglobal(script.thread_state, "f"); // pushes onto stack
			int f_ref = lua_ref(script.thread_state, /*stack index=*/-1);
			lua_pop(script.thread_state, 1);

			timer.reset();
			for(int i=0; i<num_iters; ++i)
			{
				lua_getref(script.thread_state, f_ref); // Pushes f_ref onto the stack.
				lua_pushnumber(script.thread_state, 10.0);
				lua_call(script.thread_state, /*nargs=*/1, /*nresults=*/1);
				testAssert(lua_isnumber(script.thread_state, -1));
				testAssert(lua_tonumber(script.thread_state, -1) == 11.0);
				lua_pop(script.thread_state, 1);
			}
			conPrint("calling function via ref took " + doubleToStringNSigFigs(timer.elapsed() / num_iters * 1.0e9, 5) + " ns");
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

		//========================== Test getCallStackAsString() ==========================
		try
		{
			LuaVM vm;

			LuaScriptOptions options;
			options.c_funcs.push_back(LuaCFunction(testCallStackPrintFunc, "testCallStackPrintFunc"));

			const std::string src = 
				"function f()   \n"              // line 1
				"   testCallStackPrintFunc() \n" // line 2
				"end            \n"				 // line 3
				"function g()   \n"				 // line 4
				"     f()       \n"				 // line 5
				"end            \n"				 // line 6
				"function h()   \n"				 // line 7
				"    g()       \n"				 // line 8
				"end            \n"				 // line 9
				"h()         \n";				 // line 10
			LuaScript script(&vm, options, src);
			script.exec();
		}
		catch(LuaScriptExcepWithLocation& e)
		{
			failTest("Failed:" + e.messageWithLocations());
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}
		
		//========================== Test LuaScriptOutputHandler ==========================
		try
		{
			LuaVMOptions vm_options;
			vm_options.max_total_mem_allowed = 500000;
			LuaVM vm(vm_options);

			LuaScriptOptions options;
			options.c_funcs.push_back(LuaCFunction(testFunc, "testFunc"));

			TestLuaScriptOutputHandler handler;
			options.script_output_handler = &handler;

			const std::string src = "print('test123')";
			LuaScript script(&vm, options, src);
			script.exec();

			testAssert(handler.buf == "test123");
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}


		//========================== Test calling back into a C function ==========================
		try
		{
			LuaVMOptions vm_options;
			vm_options.max_total_mem_allowed = 500000;
			LuaVM vm(vm_options);

			LuaScriptOptions options;
			options.c_funcs.push_back(LuaCFunction(testFunc, "testFunc"));

			const std::string src = "local z = testFunc(1.0, 2.0)  \n    print(string.format('testFunc result: %i', z))   \n   assert(z == 3.0, 'z == 3.0')";
			LuaScript script(&vm, options, src);
			script.exec();
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

		//========================== Test stack overflow in compiler ==========================
		// See https://github.com/luau-lang/luau/issues/1277, should be fixed now with setting LuauRecursionLimit workaround.
		try
		{
			LuaVMOptions vm_options;
			vm_options.max_total_mem_allowed = 500000;
			LuaVM vm(vm_options);

			LuaScriptOptions options;
			options.c_funcs.push_back(LuaCFunction(testFunc, "testFunc"));

			const std::string src(2000, '(');
			LuaScript script(&vm, options, src);

			failTest("Expected excep.");
		}
		catch(glare::Exception& e)
		{
			conPrint("Got expected excep: " + e.what()); // Expected
		}


		//========================== Test repeated LuaScript creation ==========================
		// Tests issue where thread remained on Lua stack after creation, resulting in stack overflow.
		try
		{
			LuaVMOptions vm_options;
			vm_options.max_total_mem_allowed = 500000;
			LuaVM vm(vm_options);

			for(int i=0; i<1000; ++i)
			{
				LuaScriptOptions options;
				const std::string src = "x = 1.0";
				LuaScript script(&vm, options, src);
				script.exec();
			}
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}


		//========================== Test repeated LuaScript creation with a compile error ==========================
		// This will cause an exception to be thrown from the LuaScript constructor; make sure we are not leaking anything.
		try
		{
			LuaVMOptions vm_options;
			vm_options.max_total_mem_allowed = 500000;
			LuaVM vm(vm_options);

			for(int i=0; i<1000; ++i)
			{
				try
				{
					LuaScriptOptions options;
					const std::string src = "function x";
					LuaScript script(&vm, options, src);
					failTest("Exception expected");
				}
				catch(glare::Exception& )
				{
					// Expected
				}
			}
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}


		//========================== Test creating a table with a metatable and __index and __newindex metamethod ==========================
		try
		{
			LuaVMOptions vm_options;
			vm_options.max_total_mem_allowed = 500000;
			LuaVM vm(vm_options);

			LuaScriptOptions options;
			//options.c_funcs.push_back(LuaCFunction(testFunc, "testFunc"));

			TestLuaScriptOutputHandler handler;
			options.script_output_handler = &handler;

			const std::string src = 
				"function testFuncTakingTable(t)    \n"
				"   t.somefield = 456.0             \n"
				"   return t.hello                  \n"
				"end";
			LuaScript script(&vm, options, src);
			script.exec();


			Timer timer;

			const int num_iters = 1;
			for(int i=0; i<num_iters; ++i)
			{
				lua_getglobal(script.thread_state, "testFuncTakingTable");  // Push function to be called onto stack

				// Create a table ('test_table')
				lua_createtable(script.thread_state, /*num array elems=*/0, /*num non-array elems=*/1); // Create table

				// Set table UID field
				lua_pushnumber(script.thread_state, 123.0);
				lua_rawsetfield(script.thread_state, /*table index=*/-2, "uid"); // pops value (123.0) from stack

				lua_createtable(script.thread_state, /*num array elems=*/0, /*num non-array elems=*/2); // Create metatable
			
				// Set glareLuaIndexMetaMethod as __index metamethod
				lua_pushcfunction(script.thread_state, glareLuaIndexMetaMethod, /*debugname=*/"glareLuaIndexMetaMethod");
				lua_rawsetfield(script.thread_state, /*table index=*/-2, /*key=*/"__index"); // pops value (glareLuaIndexMetaMethod) from stack

				// Set glareLuaNewIndexMetaMethod as __newindex metamethod
				lua_pushcfunction(script.thread_state, glareLuaNewIndexMetaMethod, /*debugname=*/"glareLuaNewIndexMetaMethod");
				lua_rawsetfield(script.thread_state, /*table index=*/-2, /*key=*/"__newindex"); // pops value (glareLuaNewIndexMetaMethod) from stack

				// Assign metatable to test_table
				lua_setmetatable(script.thread_state, -2); // "Pops a table from the stack and sets it as the new metatable for the value at the given acceptable index."

				// Call function
				lua_call(script.thread_state, /*nargs=*/1, /*nresults=*/0);
			}

			conPrint("testFuncTakingTable call took " + doubleToStringNSigFigs(timer.elapsed() / num_iters * 1.0e9, 4) + " ns");
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}


		//========================== Test method call syntax ==========================
		// Test with eager method table approach.
		// The problem with this approach is that I don't think we can handle lazy attributes this way.
		try
		{
			LuaVM vm;
			LuaScriptOptions options;
			TestLuaScriptOutputHandler handler;
			options.script_output_handler = &handler;

			// t:testMethod(x) is syntactic sugar for t.testMethod(t, x)           (See https://stackoverflow.com/a/18809740)
			const std::string src = 
				"function testFuncTakingTable(t)    \n"
				"   assert(t:testMethod(123.0) == 246.0, 't:testMethod(123.0) == 246.0')                  \n"
				//"   assert(t.testattribute == 100.0, 't.testattribute == 100.0')                          \n"
				"end";
			LuaScript script(&vm, options, src);
			script.exec();

			// Create a method table with testMethod as a key.
			// Then create a metatable m, set m.__index = method table
			// We will store a reference to the metatable in the TestClassMetaTable global variable.

			try
			{
				//printStack(script.thread_state);
				lua_createtable(script.thread_state, /*num array elems=*/0, /*num non-array elems=*/2); // Create metatable, push onto stack

				// Create method table
				lua_createtable(script.thread_state, /*num array elems=*/0, /*num non-array elems=*/2); // Create metatable, push onto stack

				// Set testMethod in method table
				lua_pushcfunction(script.thread_state, testMethod, /*debugname=*/"testMethod");
				lua_rawsetfield(script.thread_state, /*table index=*/-2, /*key=*/"testMethod"); // pops value (testMethod) from stack

				// Set glareLuaIndexMetaMethod as __index method
				//lua_pushcfunction(script.thread_state, glareLuaIndexMetaMethod, /*debugname=*/"glareLuaIndexMetaMethod");
				//lua_rawsetfield(script.thread_state, /*table index=*/-2, /*key=*/"__index"); // pops value (testMethod) from stack

				// Set method table as __index value in metatable.
				lua_rawsetfield(script.thread_state, /*table index=*/-2, /*key=*/"__index"); // pops value (method table) from stack

				//lua_setglobal(script.thread_state, "TestClassMetaTable"); // Pops a value from the stack and sets it as the new value of global name
				
				const int TestClassMetaTable_ref = lua_ref(script.thread_state, -1); // Get reference to metatable (does not pop)
				lua_pop(script.thread_state, 1); // Pop metatable from stack

				Timer timer;

				const int num_iters = 1000;
				for(int i=0; i<num_iters; ++i)
				{
					lua_getglobal(script.thread_state, "testFuncTakingTable");  // Push function to be called onto stack

					// Create a table ('test_table')
					lua_createtable(script.thread_state, /*num array elems=*/0, /*num non-array elems=*/1); // Create table

					// Assign metatable to test_table
					//lua_getglobal(script.thread_state, "TestClassMetaTable"); // Pushes onto the stack the value of the global name
					
					lua_getref(script.thread_state, TestClassMetaTable_ref); // Pushes TestClassMetaTable_ref onto the stack.
					lua_setmetatable(script.thread_state, -2); // "Pops a table from the stack and sets it as the new metatable for the value at the given acceptable index."

					// Call function
					lua_call(script.thread_state, /*nargs=*/1, /*nresults=*/0);
				}

				conPrint("testFuncTakingTable with eager method table call took " + doubleToStringNSigFigs(timer.elapsed() / num_iters * 1.0e9, 4) + " ns");
			}
			catch(std::exception& e) // Catch lua_exception
			{
				failTest("Failed: std::exception: " + std::string(e.what()));
			}
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

		//========================== Test method call syntax with __namecall ==========================
#if 0
		//NOTE: only works with 'reflected metadata tables'
		try
		{
			LuaVM vm;
			LuaScriptOptions options;
			TestLuaScriptOutputHandler handler;
			options.script_output_handler = &handler;

			// t:testMethod(x) is syntactic sugar for t.testMethod(t, x)           (See https://stackoverflow.com/a/18809740)
			const std::string src = 
				"function testFuncTakingTable(t)    \n"
				"   assert(t:testMethod(123.0) == 246.0, 't:testMethod(123.0) == 246.0')                  \n"
				"end";
			LuaScript script(&vm, options, src);
			script.exec();

			// Create a method table with testMethod as a key.
			// Then create a metatable m, set m.__index = method table
			// We will store a reference to the metatable in the TestClassMetaTable global variable.

			try
			{
				lua_createtable(script.thread_state, /*num array elems=*/0, /*num non-array elems=*/2); // Create metatable, push onto stack

				// Set glareLuaNameCallMetaMethod as __namecall method on metatable
				lua_pushcfunction(script.thread_state, glareLuaNameCallMetaMethod, /*debugname=*/"glareLuaNameCallMetaMethod");
				lua_rawsetfield(script.thread_state, /*table index=*/-2, /*key=*/"__namecall"); // pops value from stack

				// Set glareLuaIndexMetaMethod as __index method on metatable
				//lua_pushcfunction(script.thread_state, glareLuaIndexMetaMethod, /*debugname=*/"glareLuaIndexMetaMethod");
				//lua_rawsetfield(script.thread_state, /*table index=*/-2, /*key=*/"__index"); // pops value from stack

				lua_setreadonly(script.thread_state, /*index=*/-1, /*enabled=*/1);

				// store a reference to the metatable in the TestClassMetaTable global variable.
				//lua_setglobal(script.thread_state, "TestClassMetaTable"); // Pops a value from the stack and sets it as the new value of global name

				const int TestClassMetaTable_ref = lua_ref(script.thread_state, -1); // Get reference to metatable (does not pop)
				lua_pop(script.thread_state, 1); // Pop metatable from stack

				Timer timer;

				const int num_iters = 1;
				for(int i=0; i<num_iters; ++i)
				{
					lua_getglobal(script.thread_state, "testFuncTakingTable");  // Push function to be called onto stack

					// Create a table ('test_table')
					lua_createtable(script.thread_state, /*num array elems=*/0, /*num non-array elems=*/1); // Create table

					// Assign metatable to test_table
					lua_getref(script.thread_state, TestClassMetaTable_ref); // Pushes TestClassMetaTable_ref onto the stack.
					lua_setmetatable(script.thread_state, -2); // "Pops a table from the stack and sets it as the new metatable for the value at the given acceptable index."

					// Call function
					lua_call(script.thread_state, /*nargs=*/1, /*nresults=*/0);
				}

				conPrint("testFuncTakingTable with eager method table call took " + doubleToStringNSigFigs(timer.elapsed() / num_iters * 1.0e9, 4) + " ns");
			}
			catch(std::exception& e) // Catch lua_exception
			{
				failTest("Failed: std::exception: " + std::string(e.what()));
			}
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}
#endif
		//========================== Test method call syntax ==========================
		// Test with lazy __index function approach
		try
		{
			LuaVM vm;
			LuaScriptOptions options;
			TestLuaScriptOutputHandler handler;
			options.script_output_handler = &handler;

			// t:testMethod(x) is syntactic sugar for t.testMethod(t, x)           (See https://stackoverflow.com/a/18809740)
			const std::string src = 
				"function testFuncTakingTable(t)    \n"
				"   assert(t:testMethod(123.0) == 246.0, 't:testMethod(123.0) == 246.0')                  \n"
				//"   assert(t.testattribute == 100.0, 't.testattribute == 100.0')                          \n"
				"end";
			LuaScript script(&vm, options, src);
			script.exec();

			try
			{
				lua_createtable(script.thread_state, /*num array elems=*/0, /*num non-array elems=*/2); // Create metatable, push onto stack

				// Set glareLuaIndexMetaMethod as __index method on metatable
				lua_pushcfunction(script.thread_state, glareLuaIndexMetaMethod, /*debugname=*/"glareLuaIndexMetaMethod");
				lua_rawsetfield(script.thread_state, /*table index=*/-2, /*key=*/"__index"); // pops value from stack

				// store a reference to the metatable in the TestClassMetaTable global variable.
				lua_setglobal(script.thread_state, "TestClassMetaTable"); // Pops a value from the stack and sets it as the new value of global name

				Timer timer;

				const int num_iters = 1000;
				for(int i=0; i<num_iters; ++i)
				{
					lua_getglobal(script.thread_state, "testFuncTakingTable");  // Push function to be called onto stack

					// Create a table ('test_table')
					lua_createtable(script.thread_state, /*num array elems=*/0, /*num non-array elems=*/1); // Create table

					// Assign metatable to test_table
					lua_getglobal(script.thread_state, "TestClassMetaTable"); // Pushes onto the stack the value of the global name
					lua_setmetatable(script.thread_state, -2); // "Pops a table from the stack and sets it as the new metatable for the value at the given acceptable index."

					// Call function
					lua_call(script.thread_state, /*nargs=*/1, /*nresults=*/0);
				}

				conPrint("testFuncTakingTable with lazy __index call took " + doubleToStringNSigFigs(timer.elapsed() / num_iters * 1.0e9, 4) + " ns");
			}
			catch(std::exception& e) // Catch lua_exception
			{
				failTest("Failed: std::exception: " + std::string(e.what()));
			}
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}


		//==========================  Test very long chained assignment ==========================
		// Was crashing the Lua parser.
		try
		{
			LuaVM vm;
	
			const std::string src = " sulr	,eah,e__e	,e,ho	,e,h_e	,e,ho	,e,h__ehi,hi_e	,i	,e,h__e	,e,hi,hi_e	,i,i,hi_e	,i,ete,ho	,e,h__ehi,hi_e	,i	"
				",e,h__e	,e,hi,hi_e	,i_ehi,hi_e	,i	,e,h__e	,e,hi,hi_e	,i,i,hi_e	,i,ete,ho	,e,h__ehi,hi_e	,i	,e,h__e	,e,hi,hi_e	,i,i,hi_e	,i,etohi,ho	"
				",e,h__ehi,hi_e	,i	,e,h__e	,e,hi,hi_e	,i,i,hi_e	,i,eto	,e_,haehi,hi_e	,i	,e,h__e	,e,ho	,e,h_h,e	,e,h__e	,e,hi,hi_e	,i,i,hi_,i,hi_e	,i,"
				"etohi,ho	,e,h__ehi,hi_e	,i	,e,h__e	,e,hi,hi_e	,i,i,hi_e	,i,eto	,e_,haehi,hi_e	,i	,e,h__e	,e,ho	,e,h_h,eo	,e_,haehi__iter,i	,e,h__e	,"
				"e,ho	,e,h__ehi,hi_e	,i	,e_e	,i,i,hi_,i,hi_e	,i,etohi,ho	,e,h__ehi,hi_e	,i	,e,h__e	,e,hi,hi_e	,i,i,hi_e	,i,eto	,e_,haehi,hi_e	,i	,e,"
				"h__e	,e,ho	,e,h_h,e	,e,h__e	,e,hi,hi_e	,i,i,hi_,i,hi_e	,i,etohi,ho	,e,h__ehi,hi_e	,i	,e,h__e	,e,hi,hi_e	,i,i,hi_e	,i,eto	,e_,haehi,hi_e	"
				",i	,e,h__e	,e,ho	,e,h_h,eo	,e_,haehi__iter,i	,e,h__e	,e,ho	,e,h__ehi,hi_e	,i	,e,h__e	,e,hi,hi_e	,i,i,hi_e	,i,etohi,ho	,e,h__ehi,hi	,i"
				",e,h__e	,e,hi,hi_e	,i,i,hi_e	,i,eto	,e_e	,i	,e,h__e	,e,hi,hi_esort	,i,i,hi_e	,i,etohi,hi_e	,i	,i,hi_e	,i5Li_e	,i_es = unpack{1, 4, 9, 13/ 81}\n ";
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();
		}
		catch(glare::Exception& )
		{
		}


		if(false)
		try
		{
			LuaVM vm;

			const std::string src = FileUtils::readEntireFileTextMode("C:\\code\\glare-core\\lua\\test.luau");
			LuaScript script(&vm, LuaScriptOptions(), src);
			script.exec();

			const std::string src2 = FileUtils::readEntireFileTextMode("C:\\code\\glare-core\\lua\\test2.luau");
			LuaScript script2(&vm, LuaScriptOptions(), src2);
			script2.exec();

			{
				/* push functions and arguments */
				lua_getglobal(script.thread_state, "f");  /* function to be called */
				LuaUtils::printStack(script.thread_state);

				lua_pushnumber(script.thread_state, 1.0);   /* push 1st argument */
				lua_pushnumber(script.thread_state, 2.0);   /* push 2nd argument */
				LuaUtils::printStack(script.thread_state);

				int result = lua_pcall(script.thread_state, /*num args=*/2, /*num results1*/1, /*errfunc=*/0);
				if(result != LUA_OK)
				{
					conPrint("lua_pcall failed: " + toString(result));
				}

				// Get result
				int is_num;
				const double resnum = lua_tonumberx(script.thread_state, -1, &is_num);
				printVar(resnum);
			}

			{
				/* push functions and arguments */
				lua_getglobal(script2.thread_state, "f");  /* function to be called */
				LuaUtils::printStack(script2.thread_state);
		
				lua_pushnumber(script2.thread_state, 1.0);   /* push 1st argument */
				lua_pushnumber(script2.thread_state, 2.0);   /* push 2nd argument */
				LuaUtils::printStack(script2.thread_state);
		
				int result = lua_pcall(script2.thread_state, /*num args=*/2, /*num results1*/1, /*errfunc=*/0);
				if(result != LUA_OK)
				{
					conPrint("lua_pcall failed: " + toString(result));
				}
		
				// Get result
				int is_num;
				const double resnum = lua_tonumberx(script2.thread_state, -1, &is_num);
				printVar(resnum);
			}

			{
				/* push functions and arguments */
				lua_getglobal(script.thread_state, "f");  /* function to be called */
				LuaUtils::printStack(script.thread_state);

				lua_pushnumber(script.thread_state, 1.0);   /* push 1st argument */
				lua_pushnumber(script.thread_state, 2.0);   /* push 2nd argument */
				LuaUtils::printStack(script.thread_state);

				int result = lua_pcall(script.thread_state, /*num args=*/2, /*num results1*/1, /*errfunc=*/0);
				if(result != LUA_OK)
				{
					conPrint("lua_pcall failed: " + toString(result));
				}

				// Get result
				int is_num;
				const double resnum = lua_tonumberx(script.thread_state, -1, &is_num);
				printVar(resnum);
			}


		}
		catch(glare::Exception& e)
		{
			conPrint("Failed:" + e.what());
		}
	}
	catch(std::exception& e) // Catch lua_exception
	{
		failTest("Failed: std::exception: " + std::string(e.what()));
	}


#if 0
	try
	{
		//------------------------ VM init ------------------------
		lua_State* state = lua_newstate(glareLuaAlloc, /*userdata=*/NULL);

		luaL_openlibs(state);

		lua_callbacks(state)->interrupt = glareLuaInterrupt;

		luaL_sandbox(state);


		//------------------------ Script init ------------------------
		lua_newthread(state);

		luaL_sandboxthread(state);

		const std::string src = FileUtils::readEntireFileTextMode("C:\\code\\glare-core\\lua\\test.luau");

		lua_CompileOptions options;
		options.optimizationLevel = 1;
		options.debugLevel = 0;
		options.typeInfoLevel = 0;
		options.coverageLevel = 0;
		options.vectorLib = NULL;
		options.vectorCtor = NULL;
		options.vectorType = NULL;
		options.mutableGlobals = NULL;

		size_t result_size = 0;
		char* bytecode = luau_compile(src.c_str(), src.size(), &options, &result_size);

		//conPrint("Result: ");
		//conPrint(std::string(bytecode, result_size));

		const std::string chunkname = "test";
		int result = luau_load(state, chunkname.c_str(), bytecode, result_size, /*env=*/0);
		if(result != 0)
			throw glare::Exception("luau_load failed: " + toString(result));

		//lua_register(state, "testFunc", testFunc);
//		lua_pushcfunction(state, testFunc, /*debugname=*/"testFunc");
//		lua_setglobal(state, "testFunc");
//		
		lua_pushcfunction(state, createObjectLuaFunc, /*debugname=*/"createObjectLuaFunc");
		lua_setglobal(state, "createObject");

		//luaL_sandbox(state);
		//luaL_sandboxthread(state);

		// Execute script main code
		//lua_pcall(state, 0, LUA_MULTRET, 0);
		lua_call(state, /*nargs=*/0, LUA_MULTRET);

		printStack(state);


		//------------ Call onClick -------------------
		//{
		//	lua_getglobal(state, "onClick");  /* function to be called */
		//	printStack(state);
		//	if(lua_isfunction(state, -1))
		//	{
		//		lua_newtable(state);
		//		setNumberField(state, "user_id", 123456);
		//
		//		printStack(state);
		//
		//		result = lua_pcall(state, /*num args=*/1, /*num results1*/0, /*errfunc=*/0);
		//		if(result != LUA_OK)
		//		{
		//			conPrint("lua_pcall of onClick failed: " + toString(result));
		//		}
		//
		//		printStack(state);
		//	}
		//}






		/* push functions and arguments */
		lua_getglobal(state, "f");  /* function to be called */
		lua_pushnumber(state, 100.0);   /* push 1st argument */
		lua_pushnumber(state, 200.0);   /* push 2nd argument */

		printStack(state);

		result = lua_pcall(state, /*num args=*/2, /*num results1*/1, /*errfunc=*/0);
		if(result != LUA_OK)
		{
			conPrint("lua_pcall failed: " + toString(result));
		}

		// Get result
		int is_num;
		const double resnum = lua_tonumberx(state, -1, &is_num);

		testAssert(is_num);
		testAssert(resnum == 300);





		//bool run = true;
		//while(run)
		//{
		//	//keep running lua while we aren't erroring
		//	int status = lua_resume(state, /*from=*/NULL, /*narg=*/0);
		//	if(status == LUA_ERRRUN)
		//	{
		//		/*var s = Luau.Luau.macros_lua_tostring(L, -1);
		//		Console.WriteLine(Marshal.PtrToStringAnsi((IntPtr)s));
		//		var trace = Luau.Luau.lua_debugtrace(L);
		//		Console.WriteLine(Marshal.PtrToStringAnsi((IntPtr)trace));
		//		Luau.Luau.lua_close(L);*/
		//		run = false;
		//		break;
		//	}
		//}


		free(bytecode);

		lua_close(state);

		printVar(num_interrupts);
	}
	catch(std::exception& e) // Catch lua_exception
	{
		conPrint("Failed: std::exception: " + std::string(e.what()));
	}
	catch(glare::Exception& e)
	{
		conPrint("Failed:" + e.what());
	}

#endif

	conPrint("LuaTests::test() done");
}
