/*=====================================================================
RuntimeCheck.h
--------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "Platform.h"


GLARE_NO_INLINE void runtimeCheckFailed(const char* message);

inline void _doRuntimeCheck(bool b, const char* message)
{
	// Since the check is unlikely to fail, put the code to handle failure in a different out-of-line function.
	// This should allow _doRuntimeCheck() to be lightweight and inlineable without causing code bloat.
	if(!b) // TODO: use [[unlikely]] when targetting C++20.
		runtimeCheckFailed(message);
}

// Kinda like assert(), but also checks in release mode (e.g. when NDEBUG is enabled), and throws glare::Exception on failure.
// Should be used to check a condition that indicates an internal logic error if not true, for example checking
// that an array index is in the array bounds, immediately before using the index.
#define runtimeCheck(v) _doRuntimeCheck((v), (#v))


// Throws an exception if b is false.
void checkProperty(bool b, const char* on_false_message);
