/*=====================================================================
PlatformUtils.cpp
-----------------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Mon Jun 06 00:24:52 2005
=====================================================================*/
#include "PlatformUtils.h"


#if defined(_WIN32) || defined(_WIN64)
	#include "IncludeWindows.h"
	#include <Iphlpapi.h>
	//#define SECURITY_WIN32 1
	//#include <Security.h>

	#if !defined(__MINGW32__)
		#include <intrin.h>
	#endif

	#include <shlobj.h>
	#include <tlhelp32.h>
	#include <Psapi.h>
#else
	#include <errno.h>
	#include <time.h>
	#include <unistd.h>
	#include <string.h> /* for strncpy */

	#ifndef OSX
		#include <sys/sysinfo.h>
		#include <sys/utsname.h>
	#endif

	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/ioctl.h>
	#include <netinet/in.h>
	#include <net/if.h>
	#include <cpuid.h>
#endif

#include <cassert>
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include <cstdlib>
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
	#include <mach-o/dyld.h>
	#include <pthread.h>
	//#include <signal.h>
#endif

#if (!defined(_WIN32) && !defined(_WIN64))
	#include <signal.h>
#endif


// Make current thread sleep for x milliseconds
void PlatformUtils::Sleep(int x)
{
#if defined(_WIN32) || defined(_WIN64)
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
#if defined(_WIN32) || defined(_WIN64)

	DWORD processorCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

	if(processorCount == 0)
		return 1;//throw PlatformUtilsExcep("GetActiveProcessorCount failed: error code: " + toString((unsigned int)GetLastError()));

	return processorCount;

	/*SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	return system_info.dwNumberOfProcessors;*/
#else
	return sysconf(_SC_NPROCESSORS_CONF);
#endif
}


uint64 PlatformUtils::getPhysicalRAMSize() // Number of bytes of physical RAM
{
#if defined(_WIN32) || defined(_WIN64)
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
#if defined(_WIN32) || defined(_WIN64)
	return getEnvironmentVariable("USERNAME");
#else
	return getEnvironmentVariable("USER");
#endif
}


unsigned int PlatformUtils::getNumThreadsInCurrentProcess()
{
#if defined(_WIN32)
	// See http://msdn.microsoft.com/en-us/library/ms686701(v=VS.85).aspx

    const DWORD process_id = GetCurrentProcessId();

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if(snapshot == INVALID_HANDLE_VALUE)
		throw PlatformUtilsExcep("CreateToolhelp32Snapshot failed.");

    THREADENTRY32 entry;
    entry.dwSize = sizeof(THREADENTRY32);

    BOOL result = Thread32First(snapshot, &entry);
	if(!result)
	{
		CloseHandle(snapshot);
		throw PlatformUtilsExcep("Thread32First failed.");
	}

	unsigned int thread_count = 0;
	while(result)
	{
		if(entry.th32OwnerProcessID == process_id)
			thread_count++;
		result = Thread32Next(snapshot, &entry);
	}

    CloseHandle(snapshot);
    
	return thread_count;
#else
	throw PlatformUtilsExcep("getNumThreadsInCurrentProcess not implemented.");
#endif
}


static void doCPUID(unsigned int infotype, unsigned int* out)
{
#if (defined(_WIN32) || defined(_WIN64)) && !defined(__MINGW32__)
	int CPUInfo[4];
	__cpuid(
		CPUInfo,
		infotype // infotype
		);
	memcpy(out, CPUInfo, 16);
#else
	__get_cpuid(infotype, out, out + 1, out + 2, out + 3);
	
	// ebx saving is necessary for PIC
	/*__asm__ volatile(
			"mov %%ebx, %%esi\n\t"
			"cpuid\n\t"
			"xchg %%ebx, %%esi"
            : "=a" (out[0]),
			"=S" (out[1]),
            "=c" (out[2]),
            "=d" (out[3])
            : "0" (infotype)
     );*/
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

	// See https://en.wikipedia.org/wiki/CPUID, also Intel 64 and IA-32 Architectures Software Developer’s Manual, Volume 2A, "INPUT EAX = 01H: Returns Model, Family, Stepping Information"
	if(info_out.family == 6 || info_out.family == 15)
	{
		if(info_out.family == 15)
			info_out.family += (CPUInfo[0] >> 20) & 0xFF;

		info_out.model += ((CPUInfo[0] >> 16) & 0xF) << 4;
	}

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
#if defined(_WIN32) || defined(_WIN64)
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
	// Supressing warnings about deprecation of FSFindFile (10.8) and CFURLCreateFromFSRef (10.9). There is no suitable replacement for C++ and
	// apparently is not going to be removed any time soon.
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wdeprecated-declarations"
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
# pragma clang diagnostic pop
#else
	throw PlatformUtilsExcep("getAPPDataDirPath() is only valid on Windows and OSX.");
#endif
}


const std::string PlatformUtils::getCommonDocumentsDirPath() // throws PlatformUtilsExcep
{
#if defined(_WIN32) || defined(_WIN64)
	TCHAR path[MAX_PATH];
	const HRESULT res = SHGetFolderPath(
		NULL, // hwndOwner
		CSIDL_COMMON_DOCUMENTS, // nFolder
		NULL, // hToken
		0, // 0=SHGFP_TYPE_CURRENT, // dwFlags
		path // pszPath
	);

	if(res != S_OK)
		throw PlatformUtilsExcep("SHGetFolderPath() failed, Error code: " + toString((int)res));

	return StringUtils::PlatformToUTF8UnicodeEncoding(path);
#else
	throw PlatformUtilsExcep("getCommonDocumentsDirPath() is only valid on Windows");
#endif
}


const std::string PlatformUtils::getTempDirPath() // throws PlatformUtilsExcep
{
#if defined(_WIN32) || defined(_WIN64)
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


const std::string PlatformUtils::getCurrentWorkingDirPath() // throws PlatformUtilsExcep
{
#if defined(_WIN32) || defined(_WIN64)
	TCHAR path[MAX_PATH];

	TCHAR* result = _wgetcwd(
		path,
		MAX_PATH
	);

	if(result == NULL)
		throw PlatformUtilsExcep("_wgetcwd() failed.");

	return StringUtils::PlatformToUTF8UnicodeEncoding(path);
#else
	char path[4096];

	char* result = getcwd(
		path,
		4096
	);
	if(result == NULL)
		throw PlatformUtilsExcep("getcwd() failed.");

	return result;
#endif
}


const std::string PlatformUtils::getOrCreateAppDataDirectory(const std::string& app_name)
{
#if defined(_WIN32) || defined(_WIN64) || defined(OSX)
	// e.g. C:\Users\Nicholas Chapman\AppData\Roaming
	const std::string appdatapath_base = PlatformUtils::getAPPDataDirPath();

	// e.g. C:\Users\Nicholas Chapman\AppData\Roaming\Indigo Renderer
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
#else // Else on Linux:
	const std::string home_dir = getEnvironmentVariable("HOME");
	const std::string company_dir = home_dir + "/.glare_technologies";
	const std::string appdatapath = company_dir + "/" + app_name;

	// Create the dir if it doesn't exist
	try
	{
		FileUtils::createDirIfDoesNotExist(company_dir);
		FileUtils::createDirIfDoesNotExist(appdatapath);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw PlatformUtilsExcep(e.what());
	}
	return appdatapath;
#endif
}


const std::string PlatformUtils::getOrCreateAppDataDirectoryWithDummyFallback(const std::string& app_name) // Doesn't throw.
{
	try
	{
		return PlatformUtils::getOrCreateAppDataDirectory(app_name);
	}
	catch(PlatformUtils::PlatformUtilsExcep&)
	{
		//conPrint("WARNING: Failed to determine APPDATA dir, using 'dummy_appdata'.");
		return "dummy_appdata";
	}
}


const std::string PlatformUtils::getResourceDirectoryPath() // throws PlatformUtilsExcep.
{
#if defined(_WIN32) || defined(_WIN64)
	return FileUtils::getDirectory(PlatformUtils::getFullPathToCurrentExecutable());
#elif defined(OSX)
	//std::vector<std::string> components;
	
	//FileUtils::getDirectoriesFromPath(FileUtils::getDirectory(PlatformUtils::getFullPathToCurrentExecutable()), components);

	//components.push_back("..");
	
	//components.push_back("Resources");
	
	//return StringUtils::join(components,"/");
	return FileUtils::getDirectory(PlatformUtils::getFullPathToCurrentExecutable()) + "/../Resources";
#else // Linux.
	return FileUtils::getDirectory(PlatformUtils::getFullPathToCurrentExecutable());
#endif
}


// http://stackoverflow.com/questions/143174/c-c-how-to-obtain-the-full-path-of-current-directory
const std::string PlatformUtils::getFullPathToCurrentExecutable() // throws PlatformUtilsExcep.
{
#if defined(_WIN32) || defined(_WIN64)
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
#elif defined(OSX)
	char path[8192];
	uint32_t size = sizeof(path);
	if (_NSGetExecutablePath(path, &size) != 0)
		throw PlatformUtilsExcep("getFullPathToCurrentExecutable(): buffer too small; need size " + ::toString(size));
	
	return std::string(path);
#else
	// From http://www.flipcode.com/archives/Path_To_Executable_On_Linux.shtml

	const std::string linkname = "/proc/" + ::toString(getpid()) + "/exe";
	
	// Now read the symbolic link
	char buf[4096];
	const int ret = readlink(linkname.c_str(), buf, sizeof(buf));
	
	if(ret == -1)
		throw PlatformUtilsExcep("getFullPathToCurrentExecutable: readlink failed [1].");
	
	// Insufficient buffer size
	if(ret >= (int)sizeof(buf))
		throw PlatformUtilsExcep("getFullPathToCurrentExecutable: readlink failed [2].");

	// Ensure proper NULL termination
	buf[ret] = 0;
	
	return std::string(buf);
#endif
}


int PlatformUtils::execute(const std::string& command)
{
	return std::system(command.c_str());
}


void PlatformUtils::openFileBrowserWindowAtLocation(const std::string& select_path)
{
#if defined(_WIN32) || defined(_WIN64)

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

	// "Handles in PROCESS_INFORMATION must be closed with CloseHandle when they are no longer needed." - MSDN 'CreateProcess function'.
	CloseHandle(procInfo.hProcess);

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


#if defined(_WIN32)

// Windows implementation of getErrorStringForCode()
const std::string PlatformUtils::getErrorStringForCode(unsigned long error_code)
{
	std::vector<wchar_t> buf(2048);

	const DWORD result = FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // See http://blogs.msdn.com/oldnewthing/archive/2007/11/28/6564257.aspx on why FORMAT_MESSAGE_IGNORE_INSERTS should be used.
		NULL, // source
		error_code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		&buf[0],
		(DWORD)buf.size(),
		NULL // arguments
	);
	if(result == 0)
		return "[Unknown (error code=" + toString((int)error_code) + ")]";
	else
		// The formatted message contains a trailing newline (\r\n) for some stupid reason, remove it.
		return ::stripTailWhitespace(StringUtils::PlatformToUTF8UnicodeEncoding(std::wstring(&buf[0]))) + " (code " + toString((int)error_code) + ")";
}

#else

// OS X and Linux implementation of getErrorStringForCode()
const std::string PlatformUtils::getErrorStringForCode(int error_code)
{
#if defined(OSX)
	
	char buf[4096];
	if(strerror_r(error_code, buf, sizeof(buf)) == 0) // returns 0 upon success: https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/strerror.3.html
		return std::string(buf);
	else
		return "[Unknown (error code=" + toString(error_code) + ")]";
		
#else

	// Linux:
	char buf[4096];
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
	// XSI-compliant version of strerror_r(), returns zero on success.  See http://linux.die.net/man/3/strerror_r
	if(strerror_r(error_code, buf, sizeof(buf)) == 0)
		return std::string(buf);
	else
		return "[Unknown (error code=" + toString(error_code) + ")]";
#else
	const char* msg = strerror_r(error_code, buf, sizeof(buf));
	return std::string(msg);
#endif

#endif
}

#endif


// Gets the error string for the last error code, using GetLastError() on Windows and errno on OS X / Linux.
const std::string PlatformUtils::getLastErrorString()
{
#if defined(_WIN32)
	return getErrorStringForCode(GetLastError());
#else
	return getErrorStringForCode(errno);
#endif
}


uint64 PlatformUtils::getProcessID()
{
#if defined(_WIN32) || defined(_WIN64)
	return ::GetCurrentProcessId();
#else
	return getpid();
#endif
}


void PlatformUtils::setThisProcessPriority(ProcessPriority p)
{
#if defined(_WIN32) || defined(_WIN64)
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


#if defined(_WIN32)
std::vector<int> PlatformUtils::getProcessorGroups()
{
	std::vector<int> res(GetActiveProcessorGroupCount());
	for(size_t i=0; i<res.size(); ++i)
		res[i] = GetActiveProcessorCount((WORD)i);
	return res;
}
#endif


void PlatformUtils::ignoreUnixSignals()
{
#if defined(_WIN32) || defined(_WIN64)
	 
#else
	// Ignore sigpipe in unix.
	signal(SIGPIPE, SIG_IGN);

	//sigset_t sigset;
	//sigemptyset(&sigset);
	//sigaddset(&sigset, SIGPIPE);
	//int res = pthread_sigmask(SIG_BLOCK, &sigset, NULL);
	
	//if(res != 0)
	//{
	//	conPrint("pthread_sigmask() failed.");
	//	exit(1);
	//}
	//else
	//	conPrint("pthread_sigmask() success.");

#endif
}


const std::string PlatformUtils::getEnvironmentVariable(const std::string& varname)
{
#if defined(_WIN32) || defined(_WIN64)
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
	const char* env_val = getenv(varname.c_str());
	if(env_val == NULL)
		throw PlatformUtilsExcep("getEnvironmentVariable failed.");
	return std::string(env_val);
#endif
}


bool PlatformUtils::isEnvironmentVariableDefined(const std::string& varname)
{
#if defined(_WIN32) || defined(_WIN64)
	const std::wstring varname_w = StringUtils::UTF8ToWString(varname);
	TCHAR buffer[2048];
	const DWORD size = 2048;

	if(GetEnvironmentVariable(varname_w.c_str(), buffer, size) == 0)
	{
		// If Failed to get env var value:
		if(GetLastError() == ERROR_ENVVAR_NOT_FOUND)
			return false;
		else
			throw PlatformUtilsExcep("GetEnvironmentVariable failed: " + getLastErrorString());
	}
	else
		return true;
#else
	return getenv(varname.c_str()) != NULL;
#endif
}


void PlatformUtils::setEnvironmentVariable(const std::string& varname, const std::string& new_value)
{
#if defined(_WIN32) || defined(_WIN64)
	const std::wstring varname_w   = StringUtils::UTF8ToWString(varname);
	const std::wstring new_value_w = StringUtils::UTF8ToWString(new_value);

	if(SetEnvironmentVariable(varname_w.c_str(), new_value_w.c_str()) == 0)
		throw PlatformUtilsExcep("SetEnvironmentVariable failed: " + getLastErrorString());
#else
	if(setenv(varname.c_str(), new_value.c_str(), /*overwrite=*/1) != 0)
		throw PlatformUtilsExcep("setenv failed: " + getLastErrorString());
#endif
}


#if defined(_WIN32)
std::string PlatformUtils::getStringRegKey(RegHKey key, const std::string &regkey_, const std::string &regvalue_)
{
	HKEY hKey;
	const std::wstring regkey = StringUtils::UTF8ToPlatformUnicodeEncoding(regkey_);
	const std::wstring regvalue = StringUtils::UTF8ToPlatformUnicodeEncoding(regvalue_);

	switch(key)
	{
	default:
	case RegHKey_CurrentUser:
		hKey = HKEY_CURRENT_USER;
		break;

	case RegHKey_LocalMachine:
		hKey = HKEY_LOCAL_MACHINE;
		break;
	}

	WCHAR lszValue[1024];
	LONG returnStatus;
	DWORD dwType = REG_SZ;
	DWORD dwSize = 1024;
	HKEY rKey;

	returnStatus = RegOpenKeyExW(hKey, regkey.c_str(), NULL, KEY_QUERY_VALUE, &rKey);
	if(returnStatus != ERROR_SUCCESS)
		throw PlatformUtilsExcep("Failed to open registry key: error code " + getErrorStringForCode(returnStatus));

	returnStatus = RegQueryValueExW(rKey, regvalue.c_str(), NULL, &dwType, (LPBYTE)lszValue, &dwSize);

	// Close key.
	RegCloseKey(rKey);

	if(returnStatus != ERROR_SUCCESS)
		throw PlatformUtilsExcep("Failed to query registry key: error code " + getErrorStringForCode(returnStatus));

	return StringUtils::PlatformToUTF8UnicodeEncoding(lszValue);
}
#endif


bool PlatformUtils::isWindows()
{
#ifdef _WIN32
	return true;
#else
	return false;
#endif
}


const std::string PlatformUtils::getOSVersionString()
{
#if defined(_WIN32)

	std::string osname;

	try
	{
		osname = getStringRegKey(RegHKey::RegHKey_LocalMachine, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName");
	}
	catch(PlatformUtilsExcep&)
	{
		osname = "Unknown Windows version";
	}

	return osname;

	/*OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof(info);

	GetVersionEx(&info);

	if(info.dwMajorVersion == 5 && info.dwMinorVersion == 1)
		return "Windows XP";
	else if(info.dwMajorVersion == 5 && info.dwMinorVersion == 2)
		return "Windows XP 64-Bit Edition or Windows Server 2003";
	else if(info.dwMajorVersion == 6 && info.dwMinorVersion == 0)
		return "Windows Vista or Windows Server 2008";
	else if(info.dwMajorVersion == 6 && info.dwMinorVersion == 1)
		return "Windows 7 or Windows Server 2008 R2";
	else if(info.dwMajorVersion == 6 && info.dwMinorVersion == 2)
		return "Windows 8 +";
	else
		return "Unknown Windows version";*/
#elif OSX
	SInt32 majorVersion, minorVersion, bugFixVersion;
	
	// Stop Clang complaining about Gestalt being deprecated, since it is the only half-decent way of getting the OS version.
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	
	Gestalt(gestaltSystemVersionMajor, &majorVersion);
	Gestalt(gestaltSystemVersionMinor, &minorVersion);
	Gestalt(gestaltSystemVersionBugFix, &bugFixVersion);
	
	#pragma clang diagnostic pop

	const std::string version_string = toString(majorVersion) + "." + toString(minorVersion) + "." + toString(bugFixVersion);
	
	// Since 10.12 (Sierra), OS X is known as macOS.
	return ((majorVersion >= 10 && minorVersion >= 12) ? std::string("macOS") : std::string("OS X")) + " " + version_string;
#else
	struct utsname name_info;
	if(uname(&name_info) != 0)
		throw PlatformUtilsExcep("uname failed.");
	
	return std::string(name_info.sysname) + " " + std::string(name_info.release);
#endif
}


// See 'How to: Set a Thread Name in Native Code', https://msdn.microsoft.com/en-gb/library/xcb2z8hs.aspx
#if defined(_WIN32)
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
 } THREADNAME_INFO;
#pragma pack(pop)
#endif


// The SetThreadDescription API was introduced in version 1607 of Windows 10.
typedef HRESULT(WINAPI* SetThreadDescriptionFuncType)(HANDLE hThread, PCWSTR lpThreadDescription);


void PlatformUtils::setCurrentThreadName(const std::string& name)
{
#if defined(_WIN32)
	// Adapted from https://chromium.googlesource.com/chromium/src/+/f6988f8ff1fb0a3d8e5f0f9cc798f7f971a1baaa/base/threading/platform_thread_win.cc:
	SetThreadDescriptionFuncType set_thread_description_func = (SetThreadDescriptionFuncType)(::GetProcAddress(
		::GetModuleHandle(L"Kernel32.dll"), "SetThreadDescription"
	));
	if(set_thread_description_func)
		set_thread_description_func(::GetCurrentThread(), StringUtils::UTF8ToWString(name).c_str());

#elif defined(OSX)
	pthread_setname_np(name.c_str());
#else
	pthread_setname_np(pthread_self(), name.c_str());
#endif
}


void PlatformUtils::setCurrentThreadNameIfTestsEnabled(const std::string& name)
{
#if BUILD_TESTS
	setCurrentThreadName(name);
#endif
}


/*size_t PlatformUtils::getMemoryUsage()
{
#if defined(_WIN32)
	PROCESS_MEMORY_COUNTERS counters;
	GetProcessMemoryInfo(
		GetCurrentProcess(),
		&counters,
		sizeof(PROCESS_MEMORY_COUNTERS)
	);
	return counters.WorkingSetSize;
#else
	// TODO
	return 0;
#endif
}*/


void PlatformUtils::beginKeepSystemAwake()
{
#if defined(_WIN32)
	// ES_CONTINUOUS = "Informs the system that the state being set should remain in effect until the next call that uses ES_CONTINUOUS and one of the other state flags is cleared."
	// ES_SYSTEM_REQUIRED = "Forces the system to be in the working state by resetting the system idle timer."
	SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
#endif
}


void PlatformUtils::endKeepSystemAwake()
{
#if defined(_WIN32)
	SetThreadExecutionState(ES_CONTINUOUS); // Clear flags to allow sleep.
#endif
}



#if BUILD_TESTS


#include "../utils/ConPrint.h"
#include "../indigo/TestUtils.h"


void PlatformUtils::testPlatformUtils()
{

	try
	{
		conPrint("PlatformUtils::getOSVersionString(): " + PlatformUtils::getOSVersionString());
		conPrint("PlatformUtils::getFullPathToCurrentExecutable(): " + PlatformUtils::getFullPathToCurrentExecutable());
		conPrint("PlatformUtils::getCurrentWorkingDirPath(): " + PlatformUtils::getCurrentWorkingDirPath());

		//openFileBrowserWindowAtLocation("C:\\testscenes");
		//openFileBrowserWindowAtLocation("C:\\testscenes\\sun_glare_test.igs");

		conPrint("PlatformUtils::getLoggedInUserName(): " + PlatformUtils::getLoggedInUserName());


		//--------------------- Test getEnvironmentVariable() -------------------
		testAssert(getEnvironmentVariable("PATH") != "");

		//--------------------- Test isEnvironmentVariableDefined() -------------------
		testAssert(isEnvironmentVariableDefined("PATH"));
		testAssert(!isEnvironmentVariableDefined("PATHKJHDFKDSYFJHG"));

		//--------------------- Test setEnvironmentVariable() -------------------
		testAssert(!isEnvironmentVariableDefined("TEST_ENV_VAR_34545"));
		setEnvironmentVariable("TEST_ENV_VAR_34545", "ABC12345");
		testAssert(getEnvironmentVariable("TEST_ENV_VAR_34545") == "ABC12345");
	}
	catch(PlatformUtilsExcep& e)
	{
		failTest(e.what());
	}

#if defined(_WIN32)
	// Test existing key/value.
	try
	{
		PlatformUtils::getStringRegKey(RegHKey::RegHKey_LocalMachine, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName");
	}
	catch(PlatformUtilsExcep& e)
	{
		conPrint(e.what());

		failTest("Existing registry key/value not found");
	}

	// Key does not exist.
	try
	{
		PlatformUtils::getStringRegKey(RegHKey::RegHKey_LocalMachine, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersionDoesNotExist", "ProductName");
		failTest("Non-existent registry key found");
	}
	catch(PlatformUtilsExcep&)
	{
	}

	// Value does not exist.
	try
	{
		PlatformUtils::getStringRegKey(RegHKey::RegHKey_LocalMachine, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductNameDoesNotExist");
		failTest("Non-existent registry value found");
	}
	catch(PlatformUtilsExcep&)
	{
	}
#endif
}


#endif // BUILD_TESTS
