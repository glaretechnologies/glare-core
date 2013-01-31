/*=====================================================================
MiniDump.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-07-08 17:47:01 +0100
=====================================================================*/
#pragma once


#include "../utils/IncludeWindows.h"


/*=====================================================================
MiniDump
-------------------

=====================================================================*/
namespace MiniDump
{

#if defined(_WIN32) || defined(_WIN64)
	// Commented out because MiniDumpWriteDump requires dbghelp.lib and we currently don't link against it
	// it works fine not linking agains it in all other projects except for licence_manager
	//int generateDump(EXCEPTION_POINTERS* pExceptionPointers);
#endif

};
