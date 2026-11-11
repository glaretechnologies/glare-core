/*=====================================================================
LuaVM.cpp
---------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "LuaVM.h"


#include "LuaScript.h"
#include "LuaUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../maths/mathstypes.h"
#include <lualib.h>
#include <Luau/Common.h>
#include <limits>


LuaVMOptions::LuaVMOptions()
:	max_total_mem_allowed(std::numeric_limits<int64>::max())
{}


static void* glareLuaAlloc(void* user_data, void* ptr, size_t original_size, size_t new_size)
{
	LuaVM* lua_vm = (LuaVM*)user_data;

	if(new_size == 0)
	{
		lua_vm->total_allocated -= (int64)original_size;

		free(ptr);
		return nullptr;
	}
	else
	{
		if(new_size != 0 && original_size == 0)
		{
			const int64 new_total_allocated = lua_vm->total_allocated + (int64)new_size;

			if(new_total_allocated > lua_vm->options.max_total_mem_allowed)
			{
				if(lua_vm->state == nullptr) // If we are in lua_newstate(), then return null instead of throwing, so lua_newstate() can clean up partially allocated data.
					return nullptr;
				else
					throw glare::Exception("Tried to allocate more Lua memory than max total allowed amount of " + toString(lua_vm->options.max_total_mem_allowed) + " B");
			}

			lua_vm->total_allocated = new_total_allocated;
			lua_vm->total_allocated_high_water_mark = myMax(lua_vm->total_allocated_high_water_mark, new_total_allocated);

			return malloc(new_size);
		}
		else
		{
			// Realloc behaviour
			const int64 new_total_allocated = lua_vm->total_allocated + ((int64)new_size - (int64)original_size);

			if(new_total_allocated > lua_vm->options.max_total_mem_allowed)
			{
				if(lua_vm->state == nullptr) // If we are in lua_newstate(), then return null instead of throwing, so lua_newstate() can clean up partially allocated data.
					return nullptr;
				else
					throw glare::Exception("Tried to allocate more Lua memory than max total allowed amount of " + toString(lua_vm->options.max_total_mem_allowed) + " B");
			}

			lua_vm->total_allocated = new_total_allocated;
			lua_vm->total_allocated_high_water_mark = myMax(lua_vm->total_allocated_high_water_mark, new_total_allocated);

			void* res = realloc(ptr, new_size);
			return res;
		}
	}
}


static void glareLuaInterrupt(lua_State* state, int gc)
{
	LuaScript* script = (LuaScript*)lua_getthreaddata(state);

	if(script) // script can be null if we get an interrupt callback setting up the shared Lua VM or in lua_newthread, before lua_setthreaddata is called.
	{
		script->num_interrupts++;
		if(script->num_interrupts >= script->options.max_num_interrupts)
			throw glare::Exception("Max num interrupts reached, aborting script");
	}
}


// We need to define this to pass the userdata pointer from parent states to child states for coroutines, which have their own state.
static void glareLuaUserThread(lua_State* parent, lua_State* child)
{
	// Luau passes a null parent when destroying a thread.
	if(parent)
		lua_setthreaddata(child, lua_getthreaddata(parent));
}


// Adapted from luaB_print in C:\programming\luau\0.627\VM\src\lbaselib.cpp
// We will supply our own implementation so print() calls don't output to stdout.
static int glareLuaPrint(lua_State* L)
{
	LuaScript* script = (LuaScript*)lua_getthreaddata(L);
	int n = lua_gettop(L); // number of arguments
	LuaScriptOutputHandler* handler = script->script_output_handler;
	if(handler != NULL)
	{
		for (int i = 1; i <= n; i++)
		{
			size_t l;
			const char* s = luaL_tolstring(L, i, &l); // convert to string using __tostring et al
			if (i > 1)
				handler->printFromLuaScript(script, "\t", 1);
			handler->printFromLuaScript(script, s, l);
			lua_pop(L, 1); // pop result
		}
		//handler->printFromLuaScript(script, "\n", 1);
	}
	return 0; // Return number of results left on stack
}


static int Vec3dConstructor(lua_State* state)
{
	Vec3d v;
	v.x = LuaUtils::getDoubleArg(state, /*index=*/1);
	v.y = LuaUtils::getDoubleArg(state, /*index=*/2);
	v.z = LuaUtils::getDoubleArg(state, /*index=*/3);

	LuaUtils::pushVec3d(state, v);

	return 1; // Return number of results left on stack
}

static int Vec3dAdd(lua_State* state)
{
	const Vec3d v_a = LuaUtils::getVec3d(state, /*index=*/1);
	const Vec3d v_b = LuaUtils::getVec3d(state, /*index=*/2);
	LuaUtils::pushVec3d(state, v_a + v_b);
	return 1; // Return number of results left on stack
}

static int Vec3dSub(lua_State* state)
{
	const Vec3d v_a = LuaUtils::getVec3d(state, /*index=*/1);
	const Vec3d v_b = LuaUtils::getVec3d(state, /*index=*/2);
	LuaUtils::pushVec3d(state, v_a - v_b);
	return 1; // Return number of results left on stack
}

static int Vec3dUnaryMinus(lua_State* state)
{
	const Vec3d v_a = LuaUtils::getVec3d(state, /*index=*/1);
	LuaUtils::pushVec3d(state, -v_a);
	return 1; // Return number of results left on stack
}

// Supports Vec3d * number and number * Vec3d.
static int Vec3dMul(lua_State* state)
{
	if(lua_type(state, /*index=*/1) == LUA_TNUMBER)
	{
		const double s = lua_tonumber(state, /*index=*/1);
		const Vec3d v = LuaUtils::getVec3d(state, /*index=*/2);
		LuaUtils::pushVec3d(state, v * s);
	}
	else
	{
		const Vec3d v = LuaUtils::getVec3d(state, /*index=*/1);
		const double s = LuaUtils::getDoubleArg(state, /*index=*/2);
		LuaUtils::pushVec3d(state, v * s);
	}
	return 1; // Return number of results left on stack
}

static int Vec3dEqualityOperator(lua_State* state)
{
	const Vec3d v_a = LuaUtils::getVec3d(state, /*index=*/1);
	const Vec3d v_b = LuaUtils::getVec3d(state, /*index=*/2);
	lua_pushboolean(state, v_a == v_b);
	return 1; // Return number of results left on stack
}


static int Vec2Constructor(lua_State* state)
{
	Vec2d v;
	v.x = LuaUtils::getDoubleArg(state, /*index=*/1);
	v.y = LuaUtils::getDoubleArg(state, /*index=*/2);

	LuaUtils::pushVec2(state, v);

	return 1; // Return number of results left on stack
}

static int Vec2Add(lua_State* state)
{
	const Vec2d v_a = LuaUtils::getVec2(state, /*index=*/1);
	const Vec2d v_b = LuaUtils::getVec2(state, /*index=*/2);
	LuaUtils::pushVec2(state, v_a + v_b);
	return 1; // Return number of results left on stack
}

static int Vec2Sub(lua_State* state)
{
	const Vec2d v_a = LuaUtils::getVec2(state, /*index=*/1);
	const Vec2d v_b = LuaUtils::getVec2(state, /*index=*/2);
	LuaUtils::pushVec2(state, v_a - v_b);
	return 1; // Return number of results left on stack
}

static int Vec2UnaryMinus(lua_State* state)
{
	const Vec2d v_a = LuaUtils::getVec2(state, /*index=*/1);
	LuaUtils::pushVec2(state, -v_a);
	return 1; // Return number of results left on stack
}

// Supports Vec2 * number and number * Vec2.
static int Vec2Mul(lua_State* state)
{
	if(lua_type(state, /*index=*/1) == LUA_TNUMBER)
	{
		const double s = lua_tonumber(state, /*index=*/1);
		const Vec2d v = LuaUtils::getVec2(state, /*index=*/2);
		LuaUtils::pushVec2(state, v * s);
	}
	else
	{
		const Vec2d v = LuaUtils::getVec2(state, /*index=*/1);
		const double s = LuaUtils::getDoubleArg(state, /*index=*/2);
		LuaUtils::pushVec2(state, v * s);
	}
	return 1; // Return number of results left on stack
}

static int Vec2EqualityOperator(lua_State* state)
{
	const Vec2d v_a = LuaUtils::getVec2(state, /*index=*/1);
	const Vec2d v_b = LuaUtils::getVec2(state, /*index=*/2);
	lua_pushboolean(state, v_a == v_b);
	return 1; // Return number of results left on stack
}


bool LuaVM::static_init_called = false;


LuaVM::LuaVM(const LuaVMOptions& options_)
:	state(nullptr),
	options(options_),
	total_allocated(0),
	total_allocated_high_water_mark(0),
	init_finished(false),
	num_references(0)
{
	runtimeCheck(static_init_called);

	try
	{
		state = lua_newstate(glareLuaAlloc, /*ud (auxiliary data to `frealloc')=*/this);
		if(!state)
			throw glare::Exception("lua_newstate failed.");

		luaL_openlibs(state); // Open all built-in Luau libraries

		// Override print.  We will supply our own implementation so print() calls don't output to stdout.
		lua_pushcfunction(state, glareLuaPrint, /*debugname=*/"glareLuaPrint");
		lua_setglobal(state, "print");

		lua_pushcfunction(state, Vec3dConstructor, /*debugname=*/"Vec3d");
		lua_setglobal(state, "Vec3d");

		lua_pushcfunction(state, Vec2Constructor, /*debugname=*/"Vec2");
		lua_setglobal(state, "Vec2");

		// Set the caller's global functions.  These come after the built-ins above so they can replace one.
		for(size_t i=0; i<options.c_funcs.size(); ++i)
		{
			lua_pushcfunction(state, options.c_funcs[i].func, /*debugname=*/options.c_funcs[i].func_name.c_str()); // "Important: Luau does not copy the debugname, the pointer lifetime has to encompass the lifetime of the VM." (https://luau.org/api/)
			lua_setglobal(state, options.c_funcs[i].func_name.c_str());
		}

		//--------------------------- Create Vec3d Metatable ---------------------------
		lua_createtable(state, /*num array elems=*/0, /*num non-array elems=*/6); // Create metatable
			
		LuaUtils::setNumberAsTableField(state, "uid", 1); // Set metatable UID (for serialisation)

		setCFunctionAsTableField(Vec3dAdd,              /*debugname=*/"Vec3dAdd",              /*key=*/"__add");
		setCFunctionAsTableField(Vec3dSub,              /*debugname=*/"Vec3dSub",              /*key=*/"__sub");
		setCFunctionAsTableField(Vec3dUnaryMinus,       /*debugname=*/"Vec3dUnaryMinus",       /*key=*/"__unm");
		setCFunctionAsTableField(Vec3dMul,              /*debugname=*/"Vec3dMul",              /*key=*/"__mul");
		setCFunctionAsTableField(Vec3dEqualityOperator, /*debugname=*/"Vec3dEqualityOperator", /*key=*/"__eq");

		lua_setreadonly(state, /*index=*/-1, /*enabled=*/1); // Set metatable as read-only.

		Vec3dMetaTable_ref = createReferenceToLuaValue(state, /*index=*/-1); // Get reference to metatable.  Does not pop.
		lua_pop(state, 1); // Pop metatable from stack
		//--------------------------- End create Vec3d Metatable ---------------------------

		//--------------------------- Create Vec2 Metatable ---------------------------
		lua_createtable(state, /*num array elems=*/0, /*num non-array elems=*/6); // Create metatable

		LuaUtils::setNumberAsTableField(state, "uid", 2); // Set metatable UID (for serialisation)

		setCFunctionAsTableField(Vec2Add,              /*debugname=*/"Vec2Add",              /*key=*/"__add");
		setCFunctionAsTableField(Vec2Sub,              /*debugname=*/"Vec2Sub",              /*key=*/"__sub");
		setCFunctionAsTableField(Vec2UnaryMinus,       /*debugname=*/"Vec2UnaryMinus",       /*key=*/"__unm");
		setCFunctionAsTableField(Vec2Mul,              /*debugname=*/"Vec2Mul",              /*key=*/"__mul");
		setCFunctionAsTableField(Vec2EqualityOperator, /*debugname=*/"Vec2EqualityOperator", /*key=*/"__eq");

		lua_setreadonly(state, /*index=*/-1, /*enabled=*/1); // Set metatable as read-only.

		Vec2MetaTable_ref = createReferenceToLuaValue(state, /*index=*/-1); // Get reference to metatable.  Does not pop.
		lua_pop(state, 1); // Pop metatable from stack
		//--------------------------- End create Vec2 Metatable ---------------------------

		lua_callbacks(state)->interrupt  = glareLuaInterrupt;
		lua_callbacks(state)->userthread = glareLuaUserThread;
	}
	catch(std::exception& e)
	{
		if(state)
			lua_close(state);

		throw glare::Exception(e.what());
	}
	catch(glare::Exception& e)
	{
		if(state)
			lua_close(state);

		throw e;
	}
}


LuaVM::~LuaVM()
{
	if(state)
	{
		destroyReferenceToLuaValue(state, Vec3dMetaTable_ref);
		destroyReferenceToLuaValue(state, Vec2MetaTable_ref);

		// Check there are no outstanding references leaked.
#ifndef NDEBUG
		if(!debug_references.empty())
		{
			conPrint("============================ Error: there are " + toString(debug_references.size()) + " leaked references to Lua objects on LuaVM shutdown ============================");
			#if defined(_WIN32)
			__debugbreak();
			#else
			assert(false);
			#endif
		}
#endif

		lua_close(state);
	}
}


void LuaVM::staticInit()
{
	// Set LuauRecursionLimit to 256.  The default value of 1000 is too high - get a stack overflow in some cases.  See https://github.com/luau-lang/luau/issues/1277
	Luau::FValue<int>* cur = Luau::FValue<int>::list;
	while(cur)
	{
		if(stringEqual(cur->name, "LuauRecursionLimit"))
			cur->value = 256;
		cur = cur->next;
	}

	static_init_called = true;
}


void LuaVM::finishInitAndSandbox()
{
	try
	{
		luaL_sandbox(state);
		init_finished = true;
	}
	catch(std::exception& e)
	{
		throw glare::Exception(e.what());
	}
}


// Assumes table is on top of stack.
// Important: Luau does not copy the debugname, the pointer lifetime has to encompass the lifetime of the VM.
void LuaVM::setCFunctionAsTableField(lua_CFunction fn, const char* debugname, const char* field_key)
{
	assert(lua_istable(state, -1));

	lua_pushcfunction(state, fn, debugname);
	lua_rawsetfield(state, /*table_index=*/-2, field_key); // pops value (function) from stack
}


LuaVM::LuaValueRef LuaVM::createReferenceToLuaValue(lua_State* calling_state, int index)
{
	LuaValueRef ref;
	ref.ref = lua_ref(calling_state, index);

	if(ref.ref > LUA_REFNIL)
	{
		#ifndef NDEBUG
		debug_references.insert(ref);
		#endif
		num_references++;
	}

	return ref;
}


void LuaVM::destroyReferenceToLuaValue(lua_State* calling_state, LuaValueRef reference)
{
	if(reference.ref <= LUA_REFNIL)
		return;

#ifndef NDEBUG
	if(debug_references.count(reference) == 0)
	{
		conPrint("============================ Error: tried to destroy a LuaValueRef that was not in references (destroyed twice?) ============================");
		#if defined(_WIN32)
		__debugbreak();
		#else
		assert(false);
		#endif
	}
	debug_references.erase(reference);
#endif

	lua_unref(calling_state, reference.ref);

	num_references--;
}
