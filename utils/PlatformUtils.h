/*=====================================================================
PlatformUtils.h
---------------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Mon Jun 06 00:24:52 2005
=====================================================================*/
#pragma once


#include <vector>
#include <string>
#include "Platform.h"


/*=====================================================================
PlatformUtils
-------------

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
*/
const std::string getAPPDataDirPath(); // throws PlatformUtilsExcep

const std::string getTempDirPath(); // throws PlatformUtilsExcep

const std::string getCurrentWorkingDirPath(); // throws PlatformUtilsExcep


/*
Returns a directory that is writeable by the app.
On Vista, this can't be indigo_base_dir_path, because that path might be in program files, and so won't be writeable.
*/
const std::string getOrCreateAppDataDirectory(const std::string& app_base_path, const std::string& app_name); // throws PlatformUtilsExcep.

const std::string getOrCreateAppDataDirectoryWithDummyFallback(); // Doesn't throw.

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


const std::string getErrorStringForReturnCode(unsigned long return_code);

const std::string getLastErrorString();


uint64 getProcessID();


enum ProcessPriority
{
	BelowNormal_Priority,
	Normal_Priority
};

void setThisProcessPriority(ProcessPriority p);
	
	
void ignoreUnixSignals();


const std::string getEnvironmentVariable(const std::string& varname);


const std::string readRegistryKey(const std::string& key, const std::string& value);


bool isWindows();


bool isWindowsXPOrEarlier();
	
	
#if defined(OSX)
void getOSXVersion(int32& majorVersion, int32& minorVersion, int32& bugFixVersion);
#endif


void testPlatformUtils();


} // end namespace PlatformUtils
