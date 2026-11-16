/*=====================================================================
RuntimeCheck.cpp
----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "RuntimeCheck.h"


#include "Exception.h"
#include "ConPrint.h"
#include <assert.h>


void runtimeCheckFailed(const char* message)
{
	conPrint("RUNTIME CHECK FAILED: " + std::string(message));
	assert(false);
#if FUZZING
	// Fuzzing not in debug mode won't stop on the assert, and out of the debugger __debugbreak() will possibly throw a structured exception that may be caught and ignored by the fuzzer.
	// So abort here so the fuzzer considers it an error and stops.
	abort(); 
#endif
#if defined(_WIN32)
	__debugbreak();
#endif
	throw glare::Exception("RUNTIME CHECK FAILED: " + std::string(message));
}


void checkProperty(bool b, const char* on_false_message)
{
	if(!b)
		throw glare::Exception(std::string(on_false_message));
}
