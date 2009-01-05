/*=====================================================================
dllwrapper.cpp
--------------
File created by ClassTemplate on Thu Apr 25 14:34:02 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "dllwrapper.h"


DLLWrapper::DLLWrapper()
{
	dll_handle = NULL;
}


DLLWrapper::DLLWrapper(const std::string& filename_)// throw (DLLWrapperExcep)
{
	filename = filename_;
	dll_handle = NULL;

	load(filename);
}


DLLWrapper::~DLLWrapper()
{
	unload();
}


void DLLWrapper::load(const std::string& filename_)// throw (DLLWrapperExcep)
{
	filename = filename_;

	dll_handle = ::LoadLibraryA(filename.c_str());

	if(dll_handle == NULL)
	{
		throw DLLWrapperExcep("LoadLibrary('" + filename + "') failed.");
		return;
	}
}


void DLLWrapper::unload()//called on destruction
{
	if(dll_handle)
	{
		::FreeLibrary(dll_handle);

		dll_handle = NULL;

		filename = "";
	}
}


void* DLLWrapper::getProcedureAddr(const std::string& procedure_name)// throw (DLLWrapperExcep)
{
	if(!dll_handle)
	{
		throw DLLWrapperExcep("attempted to get proc addr while dll not loaded");
		return NULL;
	}

	void* procaddr = ::GetProcAddress(dll_handle, procedure_name.c_str());

	if(!procaddr)
	{
		throw DLLWrapperExcep("unable to get address of procedure: " + procedure_name);
		return NULL;
	}

	return procaddr;
}

