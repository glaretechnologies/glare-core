/*=====================================================================
MiniDump.cpp
------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "MiniDump.h"


#if defined(_WIN32)
//#include <windows.h>
//#include <dbghelp.h>
//#include <shellapi.h>
//#include <shlobj.h>
//#include <Strsafe.h>
#endif


#include "StringUtils.h"
#include "PlatformUtils.h"
#include "Exception.h"
#include "FileUtils.h"


MiniDump::MiniDumpResult MiniDump::checkForNewMiniDumps(const std::string& appdata_dir, uint64 most_recent_dump_creation_time)
{
#if defined(_WIN32)
	try
	{
		const std::string minidump_dir = FileUtils::join(appdata_dir, "crash dumps");

		const std::vector<std::string> dmp_paths = FileUtils::getFilesInDirWithExtensionFullPaths(minidump_dir, "dmp");
		for(size_t i=0; i<dmp_paths.size(); ++i)
		{
			const uint64 minidump_t = FileUtils::getFileCreatedTime(dmp_paths[i]);

			if(minidump_t > most_recent_dump_creation_time) // If minidump is more recent than 'most_recent_dump_creation_time', return it
			{
				MiniDumpResult res;
				res.path = dmp_paths[i];
				res.created_time = minidump_t;
				return res;
			}
		}

		MiniDumpResult res;
		res.created_time = 0;
		return res;
	}
	catch(FileUtils::FileUtilsExcep&)
	{
		MiniDumpResult res;
		res.created_time = 0;
		return res;
	}
#else
	MiniDumpResult res;
	res.created_time = 0;
	return res;
#endif
}


/*
void MiniDump::checkEnableMiniDumps()
{
	try
	{
		// Run this process again, in admin mode

		// Get the path to this process
		const std::string process_path = PlatformUtils::getFullPathToCurrentExecutable();

		HINSTANCE result = ShellExecute(
			NULL, // hwnd
			StringUtils::UTF8ToWString("runas").c_str(), // lpOperation
			StringUtils::UTF8ToWString(process_path).c_str(), // file
			StringUtils::UTF8ToWString(" --set_crash_dump_key").c_str(), // lpParameters
			NULL, // default working dir for action
			SW_SHOWNORMAL // nShowCmd
		);

		// "If the function succeeds, it returns a value greater than 32."
		if((int)result <= 32)
			throw Indigo::Exception("ShellExecute failed: " + toString((int)result));

	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{}
	catch(Indigo::Exception& e)
	{}
}

*/

/*int MiniDump::generateDump(EXCEPTION_POINTERS* pExceptionPointers)
{
	BOOL bMiniDumpSuccessful;
	WCHAR szPath[MAX_PATH]; 
	WCHAR szFileName[MAX_PATH]; 
	WCHAR* szAppName = L"Indigo";
	WCHAR* szVersion = L"v1.0";
	//std::wstring szVersion = StringUtils::UTF8ToWString(::indigoVersionDescription());
	DWORD dwBufferSize = MAX_PATH;
	HANDLE hDumpFile;
	SYSTEMTIME stLocalTime;
	MINIDUMP_EXCEPTION_INFORMATION ExpParam;

	GetLocalTime( &stLocalTime );
	GetTempPath( dwBufferSize, szPath );

	StringCchPrintf( szFileName, MAX_PATH, L"%s%s", szPath, szAppName );
	CreateDirectory( szFileName, NULL );

	StringCchPrintf( szFileName, MAX_PATH, L"%s%s\\%s-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp", 
		szPath, szAppName, szVersion, 
		stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay, 
		stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond, 
		GetCurrentProcessId(), GetCurrentThreadId());
	hDumpFile = CreateFile(szFileName, GENERIC_READ|GENERIC_WRITE, 
		FILE_SHARE_WRITE|FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

	ExpParam.ThreadId = GetCurrentThreadId();
	ExpParam.ExceptionPointers = pExceptionPointers;
	ExpParam.ClientPointers = TRUE;

	bMiniDumpSuccessful = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), 
		hDumpFile, MiniDumpWithDataSegs, &ExpParam, NULL, NULL);

	//conPrint("MiniDump created at " + StringUtils::WToUTF8String(szFileName));

	return EXCEPTION_EXECUTE_HANDLER;
}*/
