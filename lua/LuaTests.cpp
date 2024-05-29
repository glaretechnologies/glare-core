/*=====================================================================
LuaTests.cpp
------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "LuaTests.h"


#include "LuaVM.h"
#include "LuaScript.h"
#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/FileUtils.h"
#include "../utils/PlatformUtils.h"
#include <lualib.h>
#include <luacode.h>


// Adapted from https://www.lua.org/pil/24.2.3.html
static void printStack(lua_State *L)
{
	conPrint("---------------- Lua stack ----------------");
	int i;
	int top = lua_gettop(L);
	for (i = 1; i <= top; i++) {  /* repeat for each level */
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
			break;
    
		default:  /* other values */
			conPrint("other type: " + std::string(lua_typename(L, t)));
			break;
		}
		//conPrintStr("  ");  /* put a separator */
	}
	//conPrint("");  /* end the listing */
	conPrint("-------------------------------------------");
}


// Adds two numbers together
static int testFunc(lua_State* state)
{
  const int a = lua_tointeger(state, 1); // First argument
  const int b = lua_tointeger(state, 2); // Second argument
  const int result = a + b;

  lua_pushinteger(state, result);

  return 1; // Count of returned values
}


static int createObjectLuaFunc(lua_State* state)
{
	conPrint("----createObjectLuaFunc()----");
	printStack(state);

	int val_type = lua_getfield(state, /*index=*/1, /*field key=*/"name");
	if(val_type == LUA_TSTRING)
	{
		size_t stringlen = 0;
		//const char* str = luaL_checklstring(state, /*index=*/-1, &stringlen);
		const char* str = lua_tolstring(state, /*index=*/-1, &stringlen); // May return NULL if not a string

		printStack(state);
		if(str)
		{
			//size_t stringlen = 0;
			//const char* str = lua_tolstring(state, /*index=*/1, &stringlen);

			conPrint("str: " + std::string(str, stringlen));
		}
	}
	lua_pop(state, 1); // Pop the string off the stack
	printStack(state);

	val_type = lua_getfield(state, /*index=*/1, /*field key=*/"mass");
	if(val_type == LUA_TNUMBER)
	{
		const double mass = lua_tonumber(state, /*index=*/-1);
		printVar(mass);
	}
	lua_pop(state, 1); // Pop mass value off stack

	printStack(state);

	return 0;
}


// Assume that table is at the top of stack.  From https://www.lua.org/pil/25.1.html
static void setNumberField(lua_State* state, const char* key, double value)
{
	lua_pushstring(state, key);
	lua_pushnumber(state, value);
	lua_settable(state, /*table index=*/-3);
}


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
				failTest("Expected excep.");
			}
			catch(glare::Exception& e)
			{
				conPrint("Got expected excep: " + e.what());
			}

			{
				const std::string src = "print('hello')";
				LuaScript script(&vm, LuaScriptOptions(), src);
			}
		}
		catch(glare::Exception& e)
		{
			failTest("Failed:" + e.what());
		}

		//========================== Test script interruption ==========================
		try
		{
			LuaVM vm;

			LuaScriptOptions options;
			options.max_num_interrupts = 1000;

			const std::string src = "z = 0 \n for i=1, 100000000 do z = z + 1 end ";
			LuaScript script(&vm, options, src);

			failTest("Expected excep.");
		}
		catch(glare::Exception& e)
		{
			// Expected
			conPrint("Got expected excep: " + e.what());
		}

		//========================== Test maximum memory usage ==========================
		try
		{
			LuaVM vm;
			vm.max_total_mem_allowed = 500000;

			LuaScriptOptions options;
			//options.max_num_interrupts = 1000;

			const std::string src = "my_table = {} \n for i=1, 100000000 do  my_table[i] = 'boo' end ";
			LuaScript script(&vm, options, src);

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

			const std::string src = "assert(false 'assert failed')";
			LuaScript script(&vm, LuaScriptOptions(), src);

			failTest("Expected excep.");
		}
		catch(glare::Exception& e)
		{
			conPrint("Got expected excep: " + e.what()); // Expected
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

			const std::string src2 = FileUtils::readEntireFileTextMode("C:\\code\\glare-core\\lua\\test2.luau");
			LuaScript script2(&vm, LuaScriptOptions(), src2);

			{
				/* push functions and arguments */
				lua_getglobal(script.thread_state, "f");  /* function to be called */
				printStack(script.thread_state);

				lua_pushnumber(script.thread_state, 1.0);   /* push 1st argument */
				lua_pushnumber(script.thread_state, 2.0);   /* push 2nd argument */
				printStack(script.thread_state);

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
				printStack(script2.thread_state);
		
				lua_pushnumber(script2.thread_state, 1.0);   /* push 1st argument */
				lua_pushnumber(script2.thread_state, 2.0);   /* push 2nd argument */
				printStack(script2.thread_state);
		
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
				printStack(script.thread_state);

				lua_pushnumber(script.thread_state, 1.0);   /* push 1st argument */
				lua_pushnumber(script.thread_state, 2.0);   /* push 2nd argument */
				printStack(script.thread_state);

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
