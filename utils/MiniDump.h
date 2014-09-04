/*=====================================================================
MiniDump.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-07-08 17:47:01 +0100
=====================================================================*/
#pragma once


#include "IncludeWindows.h"
#include "Platform.h"
#include <string>


/*=====================================================================
MiniDump
-------------------

=====================================================================*/
namespace MiniDump
{
	struct MiniDumpResult
	{
		std::string path;
		uint64 created_time;
	};
	MiniDumpResult checkForNewMiniDumps(const std::string& appdata_dir, uint64 most_recent_dump_creation_time);


#if defined(_WIN32)
	// Commented out because MiniDumpWriteDump requires dbghelp.lib and we currently don't link against it
	// it works fine not linking agains it in all other projects except for licence_manager
	//int generateDump(EXCEPTION_POINTERS* pExceptionPointers);
#endif

};
