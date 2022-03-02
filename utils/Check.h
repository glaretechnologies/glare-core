/*=====================================================================
Check.h
-------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "Exception.h"
#include <assert.h>


// Kinda like assert(), but checks when NDEBUG enabled, and throws glare::Exception on failure.
inline void _doRuntimeCheck(bool b, const char* message)
{
	assert(b);
	if(!b)
		throw glare::Exception(std::string(message));
}

#define doRuntimeCheck(v) _doRuntimeCheck((v), (#v))


// Throws an exception if b is false.
inline void checkProperty(bool b, const char* on_false_message)
{
	if(!b)
		throw glare::Exception(std::string(on_false_message));
}
