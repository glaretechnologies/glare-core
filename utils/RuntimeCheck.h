/*=====================================================================
RuntimeCheck.h
--------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "Exception.h"
#include <assert.h>


// Kinda like assert(), but checks when NDEBUG enabled, and throws glare::Exception on failure.
void _doRuntimeCheck(bool b, const char* message);

#define runtimeCheck(v) _doRuntimeCheck((v), (#v))
