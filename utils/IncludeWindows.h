/*=====================================================================
IncludeWindows.h
-------------------
Copyright Glare Technologies Limited 2013 -
=====================================================================*/
#pragma once


// This file exists just so we can isolate this NOMINMAX gunk.


#if defined(_WIN32)

// Stop windows.h from defining the min() and max() macros
#ifndef NOMINMAX
#define NOMINMAX
#endif


// Try and cut down the number of header files included by windows.h
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#endif
