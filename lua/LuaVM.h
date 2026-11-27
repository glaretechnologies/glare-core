/*=====================================================================
LuaVM.h
-------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
#include "../utils/ThreadSafeRefCounted.h"
#include <string>
#include <vector>
#include <set>
struct lua_State;
typedef int (*lua_CFunction)(lua_State* L);


struct LuaCFunction
{
	LuaCFunction() {}
	LuaCFunction(lua_CFunction func_, const std::string& func_name_) : func(func_), func_name(func_name_) {}

	lua_CFunction func;
	std::string func_name;
};


struct LuaVMOptions
{
	LuaVMOptions();

	int64 max_total_mem_allowed; // Default val = std::numeric_limits<int64>::max()

	// Functions to set as globals on the VM, in addition to the built-in ones.  They are set after the built-ins and before the VM
	// is sandboxed, so unlike LuaScriptOptions::c_funcs they can replace a built-in such as print.
	std::vector<LuaCFunction> c_funcs;
};


/*=====================================================================
LuaVM
-----
A wrapper around lua_State.

Vector types
------------
Luau has support for a single built-in vector type, with configurable name, see 
compile_options.vectorCtor = "Vec3f";
compile_options.vectorType = "Vec3f";
in LuaScript::LuaScript().

We will also create a 'class' (really metatable and constructor) called Vec3d.
We will define operators on it so Vec3ds can be added, subtracted with '+', '-' etc.

We will define a Vec2 class as well, leaving off the 'f' suffix since there is no Vec2f class.

See 'Create Vec3d Metatable' code in LuaVM::LuaVM().

=====================================================================*/
class LuaVM : public ThreadSafeRefCounted
{
public:
	LuaVM(const LuaVMOptions& options = LuaVMOptions());
	~LuaVM();

	static void staticInit();

	// Call once you have finished adding global functions
	void finishInitAndSandbox();

	// Assumes table is on top of stack.
	// Important: Luau does not copy the debugname, the pointer lifetime has to encompass the lifetime of the VM.
	void setCFunctionAsTableField(lua_CFunction fn, const char* debugname, const char* field_key);

	struct LuaValueRef
	{
		LuaValueRef() : ref(/*LUA_NOREF=*/-1) {}
		bool operator <  (LuaValueRef other) const { return ref < other.ref; }
		bool operator == (LuaValueRef other) const { return ref == other.ref; }
		bool operator != (LuaValueRef other) const { return ref != other.ref; }
		int ref;
	};
	struct LuaValueRefHasher
	{
		size_t operator () (LuaValueRef ref) const
		{
			std::hash<int> h;
			return h(ref.ref);
		}
	};

	LuaValueRef createReferenceToLuaValue(lua_State* calling_state, int index);
	void destroyReferenceToLuaValue(lua_State* calling_state, LuaValueRef reference); // Has no effect if the reference is LUA_NOREF or LUA_REFNIL.


	int getNumLuaValueReferences() const { return num_references; }

	// Does an explicit step of incremental garbage collection, doing about as much GC work as if step_size_KB KB had been allocated.
	// If the GC is paused, starts a new collection cycle.  Returns true if a collection cycle finished.
	// Luau only does GC work when Lua code allocates, so garbage may never be collected in a VM that isn't running any code, or that has reached max_total_mem_allowed.
	// Call this regularly to make sure garbage is collected.  Must not be called while Lua code is executing in this VM.
	bool doGCStep(int step_size_KB);

	lua_State* state;

	LuaVMOptions options; // Kept because Luau stores the debugname a function was registered with, without copying it.

	int64 total_allocated; // Current number of bytes allocated in this VM.

	// Stats tracking:
	int64 total_allocated_high_water_mark;
	uint64 num_mem_allocation_failures;
	uint64 total_num_interrupts; // Total num interrupts that occurred while executing LuaScripts that use this LuaVM.
	int64 num_scripts; // Number of LuaScripts using this LuaVM.
	int64 num_threads; // Number of Lua threads (apart from the main thread) in this LuaVM, including threads that are no longer referenced but haven't been freed by the garbage collector yet.
	

	bool init_finished; // has finishInitAndSandbox() been called?

	LuaVM::LuaValueRef Vec3dMetaTable_ref; // reference to the Vec3d metatable, to stop it being garbage-collected, and for assigning to Vec3d tables.
	LuaVM::LuaValueRef Vec2MetaTable_ref;

private:
	static bool static_init_called; // Has staticInit() been called?

#ifndef NDEBUG
	std::set<LuaVM::LuaValueRef> debug_references;
#endif
	int num_references;
};
