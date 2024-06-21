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


// C++ implemenetation of __index.  Used when a table field is read from.
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

	lua_pushnil(state);
	return 1; // Count of returned values
}


// C++ implemenetation of __namecall.  Used when a table method is called.
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


// C++ implemenetation of __newindex.  Used when a value is assigned to a table field.
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

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		LuaVM vm;
		vm.max_total_mem_allowed = 1024 * 1024;
		
		LuaScriptOptions options;
		options.max_num_interrupts = 64000;

		const std::string src((const char*)data, size);
		LuaScript script(&vm, options, src);
		script.exec();
	}
	catch(glare::Exception& e)
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

		//========================== Test vector type ==========================
		try
		{
			LuaVM vm;

			LuaScriptOptions options;

			//const std::string src = "print(v)  \n  print(v.Y)  \n  print(v.Z)";
			//const std::string src = "print(tostring(v.x))  \n  print(tostring(v.y))  \n  print(tostring(v.z))";
			//const std::string src = "local v = vector(1, 2, 3)  \n   local a = v.x    \n    ";
			const std::string src = "function testVecFunc(v : Vec3f) print(tostring(v.x)) end ";
			LuaScript script(&vm, options, src);
			script.exec();
			
			lua_getglobal(script.thread_state, "testVecFunc");  // Push function to be called onto stack
			lua_pushvector(script.thread_state, 1.0, 2.0, 3.0);
			lua_call(script.thread_state, /*nargs=*/1, /*nresults=*/0);
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

		//========================== Test maximum memory usage ==========================
		try
		{
			LuaVM vm;
			vm.max_total_mem_allowed = 500000;

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
			LuaVM vm;
			vm.max_total_mem_allowed = 500000;

			LuaScriptOptions options;
			options.c_funcs.push_back(LuaCFunction(testFunc, "testFunc"));

			TestLuaScriptOutputHandler handler;
			options.script_output_handler = &handler;

			const std::string src = "print('test123')";
			LuaScript script(&vm, options, src);
			script.exec();

			testAssert(handler.buf == "test123\n");
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}


		//========================== Test calling back into a C function ==========================
		try
		{
			LuaVM vm;
			vm.max_total_mem_allowed = 500000;

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
			LuaVM vm;
			vm.max_total_mem_allowed = 500000;

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
			LuaVM vm;
			vm.max_total_mem_allowed = 500000;

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
			LuaVM vm;
			vm.max_total_mem_allowed = 500000;

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
			LuaVM vm;
			vm.max_total_mem_allowed = 500000;

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
