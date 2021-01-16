/*=====================================================================
MiniDump.h
----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <string>


/*=====================================================================
MiniDump
--------
We tell Windows where to store crash dumps for Indigo in the Indigo installer:

;Write registry keys to enable automatic minidump saving on crash
WriteRegExpandStr HKLM "SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\indigo.exe" "DumpFolder" "$APPDATA\Indigo Renderer\crash dumps"
WriteRegDWORD HKLM     "SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\indigo.exe" "CustomDumpFlags" 0
WriteRegDWORD HKLM     "SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\indigo.exe" "DumpCount" 10
WriteRegDWORD HKLM     "SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\indigo.exe" "DumpType" 1  ; 1 = Minidump type

See scripts\IndigoRenderer_script_x64.nsi and https://docs.microsoft.com/en-us/windows/win32/wer/collecting-user-mode-dumps

Note that for testing, depending on your registry keys, the executable may need to be called indigo.exe, not indigo_gui.exe.
=====================================================================*/
namespace MiniDump
{
	struct MiniDumpResult
	{
		std::string path;
		uint64 created_time;
	};
	// Returns a MiniDumpResult with created_time = 0 if no new minidump is found.
	MiniDumpResult checkForNewMiniDumps(const std::string& appdata_dir, uint64 most_recent_dump_creation_time);


#if defined(_WIN32)
	// Commented out because MiniDumpWriteDump requires dbghelp.lib and we currently don't link against it
	// it works fine not linking agains it in all other projects except for licence_manager
	//int generateDump(EXCEPTION_POINTERS* pExceptionPointers);
#endif

};
