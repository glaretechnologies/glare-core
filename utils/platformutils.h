/*=====================================================================
platformutils.h
---------------
File created by ClassTemplate on Mon Jun 06 00:24:52 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PLATFORMUTILS_H_666_
#define __PLATFORMUTILS_H_666_


#include <vector>
#include <string>


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


void Sleep(int x);//make current thread sleep for x milliseconds


unsigned int getNumLogicalProcessors();


void getMACAddresses(std::vector<std::string>& addresses_out); // throws PlatformUtilsExcep


class CPUInfo
{
public:
	char vendor[13];
	bool mmx, sse1, sse2, sse3;
	unsigned int stepping;
	unsigned int model;
	unsigned int family;
	char proc_brand[48];
};

void getCPUInfo(CPUInfo& info_out); // throws PlatformUtilsExcep


}//end namespace PlatformUtils


#endif //__PLATFORMUTILS_H_666_
