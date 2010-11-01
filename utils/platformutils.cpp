/*=====================================================================
platformutils.cpp
-----------------
File created by ClassTemplate on Mon Jun 06 00:24:52 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "platformutils.h"


#if defined(WIN32) || defined(WIN64)
// Stop windows.h from defining the min() and max() macros
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Iphlpapi.h>
//#define SECURITY_WIN32 1
//#include <Security.h>

#if !defined(__MINGW32__)
#include <intrin.h>
#endif

#include <shlobj.h>
#else
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <string.h> /* for strncpy */

#ifndef OSX
	#include <sys/sysinfo.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#endif

#include <cassert>
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include <cstdlib>
#include <iostream>//TEMP
#include <algorithm>

#if defined(OSX)
#include <CoreServices/CoreServices.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif


// Make current thread sleep for x milliseconds
void PlatformUtils::Sleep(int x)
{
#if defined(WIN32) || defined(WIN64)
	::Sleep(x);
#else
	int numseconds = x / 1000;
	int fraction = x % 1000;//fractional seconds in ms
	struct timespec t;
	t.tv_sec = numseconds;   //seconds
	t.tv_nsec = fraction * 1000000; //nanoseconds
	nanosleep(&t, NULL);
#endif
}


unsigned int PlatformUtils::getNumLogicalProcessors()
{
#if defined(WIN32) || defined(WIN64)
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	return system_info.dwNumberOfProcessors;
#else
	return sysconf(_SC_NPROCESSORS_CONF);
#endif
}


uint64 PlatformUtils::getPhysicalRAMSize() // Number of bytes of physical RAM
{
#if defined(WIN32) || defined(WIN64)
#if defined(__MINGW32__)
	return 0; // TEMP HACK
#else
	MEMORYSTATUSEX mem_state;
	mem_state.dwLength = sizeof(mem_state);

	// TODO: check error code
	GlobalMemoryStatusEx(&mem_state);
	
	return mem_state.ullTotalPhys;
#endif
#else

#ifdef OSX

	int mib[2];  
	uint64_t memsize;  
	size_t len;  

	mib[0] = CTL_HW;  
	mib[1] = HW_MEMSIZE; /*uint64_t: physical ram size */  
	len = sizeof(memsize);  
	sysctl(mib, 2, &memsize, &len, NULL, 0);

	return memsize;

#else
	struct sysinfo info;
	if(sysinfo(&info) != 0)
		throw PlatformUtilsExcep("sysinfo failed.");

	return info.totalram;
#endif

#endif
}


const std::string PlatformUtils::getLoggedInUserName()
{
#if defined(WIN32) || defined(WIN64)
	return getEnvironmentVariable("USERNAME");
#else
	return getEnvironmentVariable("USER");
#endif
}


static void doCPUID(unsigned int infotype, unsigned int* out)
{
#if (defined(WIN32) || defined(WIN64)) && !defined(__MINGW32__)
	int CPUInfo[4];
	__cpuid(
		CPUInfo,
		infotype // infotype
		);
	memcpy(out, CPUInfo, 16);
#else
	// ebx saving is necessary for PIC
	__asm__ volatile(
			"mov %%ebx, %%esi\n\t"
			"cpuid\n\t"
			"xchg %%ebx, %%esi"
            : "=a" (out[0]),
			"=S" (out[1]),
            "=c" (out[2]),
            "=d" (out[3])
            : "0" (infotype)
     );
#endif
}


void PlatformUtils::getCPUInfo(CPUInfo& info_out)
{
	const int MMX_FLAG = 1 << 23;
	const int SSE_FLAG = 1 << 25;
	const int SSE2_FLAG = 1 << 26;
	const int SSE3_FLAG = 1;
	const int SSE_4_1_FLAG = 1 << 19; // SSE4.1 Extensions
	const int SSE_4_2_FLAG = 1 << 20; // SSE4.2 Extensions


	unsigned int CPUInfo[4];
	doCPUID(0, CPUInfo);

	const int highest_param = CPUInfo[0];
	if(highest_param < 1)
		throw PlatformUtilsExcep("CPUID highest_param error.");

	memcpy(info_out.vendor, &CPUInfo[1], 4);
	memcpy(info_out.vendor + 4, &CPUInfo[3], 4);
	memcpy(info_out.vendor + 8, &CPUInfo[2], 4);
	info_out.vendor[12] = 0;

	doCPUID(1, CPUInfo);

	info_out.mmx = (CPUInfo[3] & MMX_FLAG ) != 0;
	info_out.sse1 = (CPUInfo[3] & SSE_FLAG ) != 0;
	info_out.sse2 = (CPUInfo[3] & SSE2_FLAG ) != 0;
	info_out.sse3 = (CPUInfo[2] & SSE3_FLAG ) != 0;
	info_out.sse4_1 = (CPUInfo[2] & SSE_4_1_FLAG ) != 0;
	info_out.sse4_2 = (CPUInfo[2] & SSE_4_2_FLAG ) != 0;
	info_out.stepping = CPUInfo[0] & 0xF;
	info_out.model = (CPUInfo[0] >> 4) & 0xF;
	info_out.family = (CPUInfo[0] >> 8) & 0xF;

	doCPUID(0x80000000, CPUInfo);

	const unsigned int highest_extended_param = CPUInfo[0];
	if(highest_extended_param < 0x80000004)
		throw PlatformUtilsExcep("CPUID highest_extended_param error.");

	doCPUID(0x80000002, CPUInfo);
	memcpy(info_out.proc_brand + 0, CPUInfo + 0, 4);
	memcpy(info_out.proc_brand + 4, CPUInfo + 1, 4);
	memcpy(info_out.proc_brand + 8, CPUInfo + 2, 4);
	memcpy(info_out.proc_brand + 12, CPUInfo + 3, 4);
	doCPUID(0x80000003, CPUInfo);
	memcpy(info_out.proc_brand + 16, CPUInfo + 0, 4);
	memcpy(info_out.proc_brand + 20, CPUInfo + 1, 4);
	memcpy(info_out.proc_brand + 24, CPUInfo + 2, 4);
	memcpy(info_out.proc_brand + 28, CPUInfo + 3, 4);
	doCPUID(0x80000004, CPUInfo);
	memcpy(info_out.proc_brand + 32, CPUInfo + 0, 4);
	memcpy(info_out.proc_brand + 36, CPUInfo + 1, 4);
	memcpy(info_out.proc_brand + 40, CPUInfo + 2, 4);
	memcpy(info_out.proc_brand + 44, CPUInfo + 3, 4);
}


#if defined(OSX)
std::string GetPathFromCFURLRef(CFURLRef urlRef)
{
	char buffer[2048];
	std::string path;
	if(CFURLGetFileSystemRepresentation(urlRef, true, (UInt8*)buffer, 1024))
		path = std::string(buffer);
	return path;
}
#endif

const std::string PlatformUtils::getAPPDataDirPath() // throws PlatformUtilsExcep
{
#if defined(WIN32) || defined(WIN64)
	TCHAR path[MAX_PATH];
	const HRESULT res = SHGetFolderPath(
		NULL, // hwndOwner
		CSIDL_APPDATA, // nFolder
		NULL, // hToken
		0, // 0=SHGFP_TYPE_CURRENT, // dwFlags
		path // pszPath
		);

	if(res != S_OK)
		throw PlatformUtilsExcep("SHGetFolderPath() failed, Error code: " + toString((int)res));

	return StringUtils::PlatformToUTF8UnicodeEncoding(path);
#elif defined(OSX)
	FSRef f;
	CFURLRef url;
	std::string filepath = "";
	
	if(noErr == FSFindFolder(kUserDomain, kApplicationSupportFolderType, kDontCreateFolder, &f ))
	{
		url = CFURLCreateFromFSRef( 0, &f );
		if(url)
		{
			filepath = GetPathFromCFURLRef(url);
		}
		else
			throw PlatformUtilsExcep("Couldn't create CFURLRef for User's application support Path");
		
		url = NULL;
	}
	else 
	{
		throw PlatformUtilsExcep("Couldn't find the User's Application Support Path");
	}
	
	return filepath;
#else
	throw PlatformUtilsExcep("getAPPDataDirPath() is only valid on Windows and OSX.");
#endif
}


const std::string PlatformUtils::getTempDirPath() // throws PlatformUtilsExcep
{
#if defined(WIN32) || defined(WIN64)
	TCHAR path[MAX_PATH];
	const DWORD num_chars_rqrd = GetTempPath(
		MAX_PATH,
		path
	);

	if(num_chars_rqrd == 0 || num_chars_rqrd > MAX_PATH)
		throw PlatformUtilsExcep("GetTempPath() failed.");

	const std::string p = StringUtils::PlatformToUTF8UnicodeEncoding(path);
	return ::eatSuffix(p, "\\"); // Remove trailing backslash.
#elif defined(OSX)
	return "/tmp";
#else
	// Linux
	return "/tmp";
#endif
}


const std::string PlatformUtils::getOrCreateAppDataDirectory(const std::string& app_base_path, const std::string& app_name)
{
#if defined(WIN32) || defined(WIN64) || defined(OSX)
	// e.g. C:\Users\Nicolas Chapman\AppData\Roaming
	const std::string appdatapath_base = PlatformUtils::getAPPDataDirPath();

	// e.g. C:\Users\Nicolas Chapman\AppData\Roaming\Indigo Renderer
	const std::string appdatapath = FileUtils::join(appdatapath_base, app_name);

	// Create the dir if it doesn't exist
	try
	{
		if(!FileUtils::fileExists(appdatapath))
			FileUtils::createDir(appdatapath);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw PlatformUtilsExcep(e.what());
	}
	return appdatapath;
#else
	return app_base_path;
#endif
}


// http://stackoverflow.com/questions/143174/c-c-how-to-obtain-the-full-path-of-current-directory
const std::string PlatformUtils::getFullPathToCurrentExecutable() // throws PlatformUtilsExcep, only works on Windows.
{
#if defined(WIN32) || defined(WIN64)
	TCHAR buf[2048];
	const DWORD result = GetModuleFileName(
		NULL, // hModule "If this parameter is NULL, GetModuleFileName retrieves the path of the executable file of the current process."
		buf,
		2048
		);

	if(result == 0)
		throw PlatformUtilsExcep("GetModuleFileName failed.");
	else
		return StringUtils::PlatformToUTF8UnicodeEncoding(buf);
#else
	throw PlatformUtilsExcep("getFullPathToCurrentExecutable only supported on Windows.");
#endif
}


int PlatformUtils::execute(const std::string& command)
{
	return std::system(command.c_str());
}


void PlatformUtils::openFileBrowserWindowAtLocation(const std::string& select_path)
{
#if defined(WIN32) || defined(WIN64)

	//execute("Explorer.exe /select," + select_path + "");

	STARTUPINFO startupInfo;
	ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
	PROCESS_INFORMATION procInfo;

	if(CreateProcess(
		NULL, // L"explorer", // app name
		const_cast<wchar_t*>(StringUtils::UTF8ToPlatformUnicodeEncoding("explorer /select," + FileUtils::toPlatformSlashes(select_path)).c_str()), // command line
		NULL,
		NULL,
		0,
		0,
		NULL,
		NULL,
		&startupInfo,
		&procInfo
		) == 0)
	{
		// Failure
		throw PlatformUtilsExcep("openFileBrowserWindowAtLocation Failed: error code: " + toString((unsigned int)GetLastError()));
	}

	/*const HINSTANCE res = ShellExecute(
		NULL,
		L"explore", // operation
		StringUtils::UTF8ToPlatformUnicodeEncoding(select_path).c_str(), // file
		NULL, // parameters
		NULL, // directory
		SW_SHOWNORMAL // cmd
	);

	// "If the function succeeds, it returns a value greater than 32." - http://msdn.microsoft.com/en-us/library/bb762153%28VS.85%29.aspx
	if((int)res <= 32)
	{
		const int r = (int)res;
		if(r == ERROR_FILE_NOT_FOUND)
			throw PlatformUtilsExcep("ERROR_FILE_NOT_FOUND");
		else if(r == ERROR_PATH_NOT_FOUND)
			throw PlatformUtilsExcep("ERROR_PATH_NOT_FOUND");
		else if(r == ERROR_BAD_FORMAT)
			throw PlatformUtilsExcep("ERROR_BAD_FORMAT");
		else if(r == SE_ERR_ACCESSDENIED)
			throw PlatformUtilsExcep("SE_ERR_ACCESSDENIED");
		else if(r == SE_ERR_ASSOCINCOMPLETE)
			throw PlatformUtilsExcep("SE_ERR_ASSOCINCOMPLETE");
		else if(r == SE_ERR_FNF)
			throw PlatformUtilsExcep("SE_ERR_FNF");
		else if(r == SE_ERR_NOASSOC)
			throw PlatformUtilsExcep("SE_ERR_NOASSOC");
		else if(r == SE_ERR_PNF)
			throw PlatformUtilsExcep("SE_ERR_PNF");
		else if(r == SE_ERR_SHARE)
			throw PlatformUtilsExcep("SE_ERR_SHARE");
		else
			throw PlatformUtilsExcep("openFileBrowserWindowAtLocation: Unknown error");
	}*/


#elif defined(OSX)

	// Uses applescript
	std::string command = "osascript -e 'tell application \"Finder\" to activate' -e 'tell application \"Finder\" to reveal POSIX file \"" + select_path + "\"'";

	system(command.c_str());

#else
	// Linux
	throw PlatformUtilsExcep("openFileBrowserWindowAtLocation not available on Linux.");
#endif
}


const std::string PlatformUtils::getLastErrorString()
{
#if defined(WIN32) || defined(WIN64)
	std::vector<wchar_t> buf(2048);

	const DWORD result = FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // See http://blogs.msdn.com/oldnewthing/archive/2007/11/28/6564257.aspx on why FORMAT_MESSAGE_IGNORE_INSERTS should be used.
		NULL, // source
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		&buf[0],
		buf.size(),
		NULL // arguments
	);
	if(result == 0)
		return "[Unknown (error code=" + toString((int)GetLastError()) + "]";
	else
		return StringUtils::PlatformToUTF8UnicodeEncoding(std::wstring(&buf[0])) + " (error code=" + toString((int)GetLastError()) + ")";
#else
	return "[Unknown (error code=" + ::toString(errno) + "]";
#endif
}


uint64 PlatformUtils::getProcessID()
{
#if defined(WIN32) || defined(WIN64)
	return ::GetCurrentProcessId();
#else
	return getpid();
#endif
}


void PlatformUtils::setThisProcessPriority(ProcessPriority p)
{
#if defined(WIN32) || defined(WIN64)
	if(p == BelowNormal_Priority)
	{
		if(!::SetPriorityClass(::GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS))
			throw PlatformUtilsExcep("SetPriorityClass failed: " + getLastErrorString());
	}
	else if(p == Normal_Priority)
	{
		if(!::SetPriorityClass(::GetCurrentProcess(), NORMAL_PRIORITY_CLASS))
			throw PlatformUtilsExcep("SetPriorityClass failed: " + getLastErrorString());
	}
#else
	// For now, we'll just make this a Null op.
	//#error implement me, maybe?
#endif
}


/*const std::string PlatformUtils::readRegistryKey(const std::string& keystr, const std::string& value)
{
#if defined(WIN32) || defined(WIN64)
	HKEY key = 0;
	LONG result = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		StringUtils::UTF8ToPlatformUnicodeEncoding(keystr).c_str(),
		0, //options
		KEY_QUERY_VALUE, //
		//KEY_READ, // rights
		&key // key out
		);
	if(result != ERROR_SUCCESS)
		throw PlatformUtilsExcep("RegOpenKeyEx failed: " + toString((int)result));


	std::vector<wchar_t> buf(2048);
	DWORD bytesize = buf.size() * sizeof(wchar_t);

	result = RegGetValue(
		key, // key
		L"", // subkey
		StringUtils::UTF8ToPlatformUnicodeEncoding(value).c_str(), // value
		0x00000002, // RRF_RT_REG_SZ = 0x00000002,
		NULL,
		&buf[0],
		&bytesize
		);
	if(result != ERROR_SUCCESS)
		throw PlatformUtilsExcep("RegGetValue failed: " + toString((int)result));

	return StringUtils::PlatformToUTF8UnicodeEncoding(std::wstring(&buf[0]));
#else
	throw PlatformUtilsExcep("Called readRegistryKey() not on Windows.");
#endif
}*/


const std::string PlatformUtils::getEnvironmentVariable(const std::string& varname)
{
#if defined(WIN32) || defined(WIN64)
	//NOTE: Using GetEnvironmentVariable instead of getenv() here so we get the result in Unicode.

	const std::wstring varname_w = StringUtils::UTF8ToWString(varname);

	TCHAR buffer[2048];

	const DWORD size = 2048;

	if(GetEnvironmentVariable(
		varname_w.c_str(),
		buffer,
		size
		) == 0)
		throw PlatformUtilsExcep("getEnvironmentVariable failed: " + getLastErrorString());

	return StringUtils::WToUTF8String(buffer);

#else
	if(!getenv(varname.c_str()))
		throw PlatformUtilsExcep("getEnvironmentVariable failed.");
	return getenv(varname.c_str());
#endif
}


#if (BUILD_TESTS)
void PlatformUtils::testPlatformUtils()
{

	try
	{
		//openFileBrowserWindowAtLocation("C:\\testscenes");
		//openFileBrowserWindowAtLocation("C:\\testscenes\\sun_glare_test.igs");

		conPrint("PlatformUtils::getLoggedInUserName(): " + PlatformUtils::getLoggedInUserName());
	}
	catch(PlatformUtilsExcep& e)
	{
		conPrint(e.what());

		testAssert(!"test Failed.");
	}
}
#endif
