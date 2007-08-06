/*=====================================================================
systemmutex.h
-------------
File created by ClassTemplate on Fri Aug 27 19:16:05 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SYSTEMMUTEX_H_666_
#define __SYSTEMMUTEX_H_666_



#include <windows.h>
#include <string>

/*=====================================================================
SystemMutex
-----------
system wide win32 mutex
=====================================================================*/
class SystemMutex
{
public:
	/*=====================================================================
	SystemMutex
	-----------
	
	=====================================================================*/
	SystemMutex(const std::string& mutex_name);

	~SystemMutex();


	HANDLE getMutexHandle(){ return mutex_handle; }


	const std::string& getName() const { return mutex_name; }

private:
	HANDLE mutex_handle;
	std::string mutex_name;
};



#endif //__SYSTEMMUTEX_H_666_




