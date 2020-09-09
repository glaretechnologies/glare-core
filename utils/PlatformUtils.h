/*=====================================================================
PlatformUtils.h
---------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include <vector>
#include <string>
#include "Platform.h"


/*=====================================================================
PlatformUtils
-------------
Some functions with OS-dependent implementations.
=====================================================================*/
namespace PlatformUtils
{


class PlatformUtilsExcep
{
public:
	PlatformUtilsExcep(const std::string& s_) : s(s_) {}
	const std::string& what() const { return s; }
private:
	std::string s;
};


void Sleep(int x); // Make current thread sleep for x milliseconds


unsigned int getNumLogicalProcessors();

uint64 getPhysicalRAMSize(); // Number of bytes of physical RAM

const std::string getLoggedInUserName();

// Only implemented for Windows currently.
unsigned int getNumThreadsInCurrentProcess(); // throws PlatformUtilsExcep




class CPUInfo
{
public:
	char vendor[13];
	bool mmx, sse1, sse2, sse3, sse4_1, sse4_2;
	unsigned int stepping;
	unsigned int model;
	unsigned int family;
	char proc_brand[48];
};

void getCPUInfo(CPUInfo& info_out); // throws PlatformUtilsExcep

/*
This is something like "C:\Documents and Settings\username\Application Data" on XP, and "C:\Users\Nicolas Chapman\AppData\Roaming" on Vista.
On Mac, returns the user's Application Support path.
This function is not implemented for Linux.
*/
const std::string getUserAppDataDirPath(); // throws PlatformUtilsExcep


// Only works on Windows, returns CSIDL_COMMON_DOCUMENTS, e.g. 'C:\Users\Public\Documents'.
const std::string getCommonDocumentsDirPath(); // throws PlatformUtilsExcep

const std::string getTempDirPath(); // throws PlatformUtilsExcep

const std::string getCurrentWorkingDirPath(); // throws PlatformUtilsExcep


/*
Returns the appdata directory for the application - a directory that is writeable by the app.
For example, on Windows: 
C:\Users\Nicholas Chapman\AppData\Roaming\app_name

On Linux this will return
$HOME/.glare_technologies/app_name

The directories are created if they do not yet exist.
*/
const std::string getAppDataDirectory(const std::string& app_name); // throws PlatformUtilsExcep

const std::string getOrCreateAppDataDirectory(const std::string& app_name); // throws PlatformUtilsExcep.

const std::string getResourceDirectoryPath(); // throws PlatformUtilsExcep
	
const std::string getFullPathToCurrentExecutable(); // throws PlatformUtilsExcep.

int execute(const std::string& command);

/*
On Windows, open Windows Explorer and select the given file or folder.
On Mac, open Finder.
On Linux.. erm.. do something good.

NOTE: be vary careful to only path in valid paths here, or there will be massive security vunerabilities.
*/
void openFileBrowserWindowAtLocation(const std::string& select_path);


#if defined(_WIN32)
const std::string getErrorStringForCode(unsigned long error_code);

const std::string COMErrorString(/*HRESULT=*/long hresult);
#else
const std::string getErrorStringForCode(int error_code);
#endif

// Gets the error string for the last error code, using GetLastError() on Windows and errno on OS X / Linux.
const std::string getLastErrorString();


uint64 getProcessID();


enum ProcessPriority
{
	BelowNormal_Priority,
	Normal_Priority
};

void setThisProcessPriority(ProcessPriority p);


// Gets the number of logical processors in each processor group.
#if defined(_WIN32)
std::vector<int> getProcessorGroups();
#endif


void ignoreUnixSignals();


const std::string getEnvironmentVariable(const std::string& varname);
bool isEnvironmentVariableDefined(const std::string& varname);
void setEnvironmentVariable(const std::string& varname, const std::string& new_value);


#if defined(_WIN32)
enum RegHKey
{
	RegHKey_CurrentUser,
	RegHKey_LocalMachine,
};

std::string getStringRegKey(RegHKey key, const std::string &regkey_, const std::string &regvalue_);
#endif


bool isWindows();


const std::string getOSVersionString();
	

void setCurrentThreadName(const std::string& name); // Sets the thread name as seen in the debugger
void setCurrentThreadNameIfTestsEnabled(const std::string& name); // Sets the thread name as seen in the debugger, if BUILD_TESTS is enabled.

// Disabled for now, since it requires linking to Psapi.lib, which is tricky to do with CMake and the SDK Lib.
//size_t getMemoryUsage();

void beginKeepSystemAwake(); // Stop the system from going to sleep.
void endKeepSystemAwake(); // Allow the system to go to sleep.

void testPlatformUtils();


} // end namespace PlatformUtils
