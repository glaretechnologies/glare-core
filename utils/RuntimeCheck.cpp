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
