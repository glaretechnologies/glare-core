/*=====================================================================
IncludeHalf.h
-------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once

/*
half.h from IlmBase (OpenEXR) gives the following warning on Windows:
O:\indigo\trunk\IlmBase\Half\half.h(474): warning C4244: '=': conversion from 'int' to 'unsigned short', possible loss of data
So suppress this warning with #pragma warning.
We'll do this in this separate file for prettiness.
*/

#ifdef _MSC_VER
#pragma warning(push, 0) // Disable warnings
#endif

#include <half.h>

#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif
