/*=====================================================================
systemmutex.cpp
---------------
File created by ClassTemplate on Fri Aug 27 19:16:05 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "systemmutex.h"




SystemMutex::SystemMutex(const std::string& mutex_name_)
:	mutex_name(mutex_name_)
{
	mutex_handle = NULL;


	mutex_handle = ::CreateMutex(NULL,//security attributed
								FALSE,//initial ownership
								mutex_name.c_str());//name
}


SystemMutex::~SystemMutex()
{
	if(mutex_handle != NULL)
		CloseHandle(mutex_handle);
}






