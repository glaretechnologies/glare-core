/*=====================================================================
LuaScript.cpp
-------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "LuaScript.h"


#include "LuaVM.h"
#include "LuaUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/Parser.h"
#include "../utils/Timer.h"
#include <lualib.h>
#include <Luau/Compiler.h>
#include "Luau/BytecodeBuilder.h"
#include "Luau/ParseResult.h"
#include "Luau/Parser.h"
#include "Luau/Ast.h"
#include <cassert>
#include <algorithm>
#include <unordered_set>


// Collects calls to global functions, like foo(), and the globals the script defines, by assignment or with 'function foo()'.
class GlobalCallCollector : public Luau::AstVisitor
{
public:
	struct GlobalCall
	{
		std::string name;
		int line; // 1-based
	};

	virtual bool visit(Luau::AstExprCall* node) override
	{
		if(Luau::AstExprGlobal* global = node->func->as<Luau::AstExprGlobal>())
			calls.push_back(GlobalCall({std::string(global->name.value), (int)node->location.begin.line + 1})); // Luau lines are 0-based.
		return true; // Visit children
	}

	virtual bool visit(Luau::AstStatAssign* node) override
	{
		for(size_t i=0; i<node->vars.size; ++i)
			if(Luau::AstExprGlobal* global = node->vars.data[i]->as<Luau::AstExprGlobal>())
				defined_globals.insert(global->name.value);
		return true;
	}

	virtual bool visit(Luau::AstStatCompoundAssign* node) override
	{
		if(Luau::AstExprGlobal* global = node->var->as<Luau::AstExprGlobal>())
			defined_globals.insert(global->name.value);
		return true;
	}

	virtual bool visit(Luau::AstStatFunction* node) override
	{
		if(Luau::AstExprGlobal* global = node->name->as<Luau::AstExprGlobal>())
			defined_globals.insert(global->name.value);
		return true;
	}

	std::vector<GlobalCall> calls;
	std::unordered_set<std::string> defined_globals;
};




LuaScript::LuaScript(LuaVM* lua_vm_, const LuaScriptOptions& options_, const std::string& script_src)
:	lua_vm(lua_vm_),
	thread_state(NULL),
	options(options_),
	num_interrupts(0),
	script_output_handler(options_.script_output_handler),
	userdata(options_.userdata)
{
	try
	{
		if(!lua_vm->init_finished)
			lua_vm->finishInitAndSandbox();

		this->Vec3dMetaTable_ref = lua_vm->Vec3dMetaTable_ref;
		this->Vec2MetaTable_ref  = lua_vm->Vec2MetaTable_ref;

		// conPrint("---------LuaScript::LuaScript()------------");
		thread_state = lua_newthread(lua_vm->state); // Creates a new thread, pushes it on the stack, and returns a pointer to a lua_State that represents this new thread
		if(!thread_state)
			throw glare::Exception("lua_newthread failed");

		thread_ref = lua_vm->createReferenceToLuaValue(lua_vm->state, /*stack index=*/-1); // Get reference to thread to prevent it being garbage collected, see https://github.com/luau-lang/luau/discussions/774 and https://github.com/luau-lang/luau/issues/247
		// lua_ref in Luau doesn't pop item from stack (see https://github.com/luau-lang/luau/issues/247#issuecomment-983042025)

		lua_pop(lua_vm->state, 1); // Pop thread from shared lua_vm stack.

		lua_setthreaddata(thread_state, this);

		luaL_sandboxthread(thread_state);

		Luau::CompileOptions compile_options;
		compile_options.optimizationLevel = 1;
		compile_options.debugLevel = 1;
		compile_options.typeInfoLevel = 0;
		compile_options.coverageLevel = 0;
		compile_options.vectorLib = NULL;
		compile_options.vectorCtor = "Vec3f";
		compile_options.vectorType = "Vec3f";
		compile_options.mutableGlobals = NULL;

		// Use C++ compilation API instead of C API so we can get error locations via the Luau::ParseError/Luau::ParseErrors exceptions.
		Luau::ParseOptions parse_options;
		Luau::BytecodeBuilder bytecode_builder(/*encoder*/NULL);

		// Parse, then compile from the parse result, so that we can also walk the AST to find calls to undefined global functions.
		// This is what Luau::compileOrThrow(bytecode_builder, script_src, ...) does.
		Luau::Allocator allocator;
		Luau::AstNameTable names(allocator);
		Luau::ParseResult parse_result = Luau::Parser::parse(script_src.c_str(), script_src.size(), names, allocator, parse_options);
		if(!parse_result.errors.empty())
			throw Luau::ParseErrors(parse_result.errors);

		GlobalCallCollector global_call_collector;
		if(options.find_calls_to_undefined_globals)
			parse_result.root->visit(&global_call_collector);

		Luau::compileOrThrow(bytecode_builder, parse_result, names, compile_options);

		const std::string bytecode = bytecode_builder.getBytecode();

		// Luau prints a chunkname starting with '@' as-is, and wraps any other one as [string "..."], so a name we were given gets
		// the prefix in order to read as a plain file:line: location.
		const std::string chunkname = options.chunkname.empty() ? std::string("script") : ("@" + options.chunkname);
		const int result = luau_load(thread_state, chunkname.c_str(), bytecode.c_str(), bytecode.size(), /*env=*/0);
		if(result != 0)
		{
			// Try and get error from top of stack where luau_load puts it.
			size_t stringlen = 0;
			const char* str = lua_tolstring(thread_state, /*index=*/-1, &stringlen); // May return NULL if not a string
			if(str)
				throw glare::Exception("luau_load failed: " + std::string(str, stringlen));
			else
				throw glare::Exception("luau_load failed.");
		}

		// TODO: Add c functions to shared global state instead?
		for(size_t i=0; i<options.c_funcs.size(); ++i)
		{
			lua_pushcfunction(thread_state, options.c_funcs[i].func, /*debugname=*/options.c_funcs[i].func_name.c_str());
			lua_setglobal(thread_state, options.c_funcs[i].func_name.c_str());
		}

		// Record calls to global functions that are neither defined by the script nor globals in the VM, such as a misspelt function name.
		// The VM globals include Luau's built-in functions as well as ours, so ask the VM whether each called name is a global.
		// global_call_collector.calls is empty if options.find_calls_to_undefined_globals is false.
		if(options.find_calls_to_undefined_globals)
		{
			//Timer timer;
			for(size_t i=0; i<global_call_collector.calls.size(); ++i) // For each call of a function:
			{
				const GlobalCallCollector::GlobalCall& call = global_call_collector.calls[i];
				if(global_call_collector.defined_globals.count(call.name) > 0) // If the script defines this global:
					continue;
				if(compile_options.vectorCtor && (call.name == compile_options.vectorCtor)) // The vector constructor is compiled to a builtin, and isn't a global.
					continue;

				// Query the lua state itself for this function name, to see if it is a global function (might be a built-in function)
				const int type = lua_getglobal(thread_state, call.name.c_str()); // Push global value (nil if not defined)
				lua_pop(thread_state, 1); // Pop global value
				if(type == LUA_TNIL)
					calls_to_undefined_globals.push_back(GlobalFuncCall({call.name, call.line}));
			}
			//conPrint("building calls_to_undefined_globals took " + timer.elapsedStringMS());
		}

		lua_vm->num_scripts++; // Only increment once we know we aren't going to throw from the constructor.
	}
	catch(Luau::ParseErrors& e)
	{
		// If we throw an exception in the constructor, the destructor isn't called, so we need to make sure to release the thread reference.
		lua_vm->destroyReferenceToLuaValue(lua_vm->state, thread_ref);

		LuaScriptExcepWithLocation loc_excep(e.what());
		loc_excep.chunkname = options.chunkname;
		for(size_t i=0; i<e.getErrors().size(); ++i)
			loc_excep.errors.push_back(LuaScriptParseError(e.getErrors()[i].getMessage(), e.getErrors()[i].getLocation()));

		throw loc_excep;
	}
	catch(Luau::ParseError& e)
	{
		// If we throw an exception in the constructor, the destructor isn't called, so we need to make sure to release the thread reference.
		lua_vm->destroyReferenceToLuaValue(lua_vm->state, thread_ref);

		LuaScriptExcepWithLocation loc_excep(e.what());
		loc_excep.chunkname = options.chunkname;
		loc_excep.errors.push_back(LuaScriptParseError(e.getMessage(), e.getLocation()));

		throw loc_excep;
	}
	catch(glare::Exception& e)
	{
		// If we throw an exception in the constructor, the destructor isn't called, so we need to make sure to release the thread reference.
		lua_vm->destroyReferenceToLuaValue(lua_vm->state, thread_ref);

		throw e; // Re-throw
	}
	catch(std::exception& e)
	{
		// If we throw an exception in the constructor, the destructor isn't called, so we need to make sure to release the thread reference.
		lua_vm->destroyReferenceToLuaValue(lua_vm->state, thread_ref);

		throw glare::Exception(e.what());
	}
}


LuaScript::~LuaScript()
{
	// Remove all 'leaked' (unfreed) function references from the LuaVM.
	for(auto it = lua_func_references.begin(); it != lua_func_references.end(); ++it)
		lua_vm->destroyReferenceToLuaValue(lua_vm->state, *it);

	lua_vm->destroyReferenceToLuaValue(lua_vm->state, thread_ref);

	lua_vm->num_scripts--;
}


void LuaScript::exec()
{
	// Warn about calls to undefined global functions, once per function name.  Calling one would fail with just "attempt to call a nil value", which doesn't say what the name was.
	// A global created in some other way, e.g. by a function the script calls, would be warned about wrongly, so these are warnings, not errors.
	if(script_output_handler)
	{
		const size_t max_num_warnings = 10;
		std::unordered_set<std::string> warned_names;
		for(size_t i=0; i<calls_to_undefined_globals.size() && warned_names.size() < max_num_warnings; ++i)
		{
			const GlobalFuncCall& call = calls_to_undefined_globals[i];
			if(warned_names.insert(call.name).second)
			{
				const std::string msg = "Warning: line " + toString(call.line) + ": call to undefined function '" + call.name + "'";
				script_output_handler->printFromLuaScript(this, msg.data(), msg.size());
			}
		}
	}

	try
	{
		// Execute script main code
		lua_call(thread_state, /*nargs=*/0, LUA_MULTRET);
	}
	catch(std::exception& e)
	{
		throw glare::Exception(improveErrorMessage(e.what()));
	}
}


std::string LuaScript::improveErrorMessage(const std::string& msg) const
{
	// Runtime error messages look like '[string "script"]:42: attempt to call a nil value'.  Get the line number before the error text.
	const std::string call_nil_error = ": attempt to call a nil value";
	const size_t error_pos = msg.find(call_nil_error);
	if(error_pos == std::string::npos)
		return msg;

	if(error_pos == 0)
		return msg;
	const size_t line_colon_pos = msg.rfind(':', error_pos - 1); // Position of the ':' before the line number
	if(line_colon_pos == std::string::npos)
		return msg;

	Parser parser(msg);
	parser.setCurrentPos(line_colon_pos + 1);
	uint32 line;
	if(!parser.parseUnsignedInt(line) || (parser.currentPos() != error_pos))
		return msg;

	std::vector<std::string> names; // Undefined functions called on the error line
	for(size_t i=0; i<calls_to_undefined_globals.size(); ++i)
		if(((uint32)calls_to_undefined_globals[i].line == line) && (std::find(names.begin(), names.end(), calls_to_undefined_globals[i].name) == names.end()))
			names.push_back(calls_to_undefined_globals[i].name);

	if(names.empty())
		return msg;

	std::string names_str;
	for(size_t i=0; i<names.size(); ++i)
		names_str += ((i == 0) ? "'" : " or '") + names[i] + "'";

	return msg + " (probably because " + names_str + " is not defined)";
}


LuaScript::LuaFuncRefAndPtr LuaScript::getRefToFunction(const char* func_name)
{
	lua_getglobal(thread_state, func_name); // Push function onto stack (or nil if it isn't defined)

	LuaFuncRefAndPtr res;
	assert(res.ref.ref == LUA_NOREF);
	res.func_ptr = nullptr;

	if(lua_isfunction(thread_state, -1)) // If function was defined
	{
		res.ref = lua_vm->createReferenceToLuaValue(thread_state, /*index=*/-1); // Get reference to func (does not pop)
		res.func_ptr = lua_topointer(thread_state, /*index=*/-1); // Get pointer to func (does not pop)

		// Add to tracking set for this LuaScript
		if(res.ref.ref > LUA_REFNIL)
			lua_func_references.insert(res.ref);
	}

	lua_pop(thread_state, 1);
	return res;
}


LuaVM::LuaValueRef LuaScript::getRefToFunctionOnStack(lua_State* calling_state, int stack_index)
{
	LuaVM::LuaValueRef res = lua_vm->createReferenceToLuaValue(calling_state, stack_index);

	// Add to tracking set for this LuaScript
	if(res.ref > LUA_REFNIL)
		lua_func_references.insert(res);

	return res;
}


LuaVM::LuaValueRef LuaScript::getOptionalRefToFunctionInTable(lua_State* calling_state, int table_index, const char* key)
{
	const int value_type = lua_rawgetfield(calling_state, table_index, key); // Push value onto stack
	if(!(value_type == LUA_TNIL || value_type == LUA_TFUNCTION))
	{
		lua_pop(calling_state, 1); // Pop value
		throw glare::Exception("'" + std::string(key) + "' must be a function" + "\n" + LuaUtils::getCallStackAsString(calling_state));
	}

	LuaVM::LuaValueRef val_ref;
	if(value_type == LUA_TFUNCTION)
		val_ref = getRefToFunctionOnStack(calling_state, /*index=*/-1);
	// else if value_type == LUA_TNIL, just leave val_ref as the default LUA_NOREF.

	lua_pop(calling_state, 1); // Pop function value
	return val_ref;
}


void LuaScript::freeRefToFunction(LuaVM::LuaValueRef ref)
{
	if(ref.ref <= LUA_REFNIL)
		return;

	// Check and remove from tracking set for this LuaScript
	#ifndef NDEBUG
	if(lua_func_references.count(ref) == 0)
	{
		conPrint("============================ Error: LuaScript: tried to destroy a LuaValueRef that was not in lua_func_references (destroyed twice?) ============================");
		#if defined(_WIN32)
		__debugbreak();
		#else
		assert(false);
		#endif
	}
	#endif

	lua_func_references.erase(ref);


	lua_vm->destroyReferenceToLuaValue(thread_state, ref);
}


std::string LuaScriptExcepWithLocation::messageWithLocations()
{
	std::string msg;// = what();

	for(size_t i=0; i<errors.size(); ++i)
	{
		const std::string line = toString(errors[i].location.begin.line + 1);
		const std::string col  = toString(errors[i].location.begin.column + 1);

		// Name the script when we know what it's called, in the file:line:col form editors and tools expect.
		if(chunkname.empty())
			msg += "Line " + line + ", col " + col + ": " + errors[i].msg + "\n";
		else
			msg += chunkname + ":" + line + ":" + col + ": " + errors[i].msg + "\n";
	}
	return msg;
}
