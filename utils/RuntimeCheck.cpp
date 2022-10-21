/*=====================================================================
RuntimeCheck.cpp
----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "RuntimeCheck.h"


void _doRuntimeCheck(bool b, const char* message)
{
	assert(b);
	if(!b)
	{
#if defined(_WIN32)
		__debugbreak();
#endif
		throw glare::Exception(std::string(message));
	}
}


void checkProperty(bool b, const char* on_false_message)
{
	if(!b)
		throw glare::Exception(std::string(on_false_message));
}
