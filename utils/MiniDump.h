/*=====================================================================
MiniDump.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-07-08 17:47:01 +0100
=====================================================================*/
#pragma once


#if defined(WIN32) || defined(WIN64)
#include <windows.h>
#endif


/*=====================================================================
MiniDump
-------------------

=====================================================================*/
namespace MiniDump
{

#if defined(WIN32) || defined(WIN64)
	int generateDump(EXCEPTION_POINTERS* pExceptionPointers);
#endif

};
