/*=====================================================================
dllwrapper.h
------------
File created by ClassTemplate on Thu Apr 25 14:34:02 2002
Code By Nicholas Chapman.

last changed 3 July 2004.

Send bugs/comments to nickamy@paradise.net.nz
http://homepages.paradise.net.nz/nickamy/
=====================================================================*/
#ifndef __DLLWRAPPER_H_666_
#define __DLLWRAPPER_H_666_

//#pragma warning(disable : 4786)//disable long debug name warning

#include <string>
#include <windows.h>
#include "platform.h"




class DLLWrapperExcep
{
public:
	DLLWrapperExcep(const std::string& s_) : s(s_) {}
	~DLLWrapperExcep(){}

	const std::string& what() const { return s; }

private:
	std::string s;
};




/*=====================================================================
DLLWrapper
----------

=====================================================================*/
class DLLWrapper
{
public:
	/*=====================================================================
	DLLWrapper
	----------
	
	=====================================================================*/
	DLLWrapper();
	DLLWrapper(const std::string& filename) throw (DLLWrapperExcep);

	~DLLWrapper();

	void load(const std::string& filename) throw (DLLWrapperExcep);


	void unload();//called on destruction


	void* getProcedureAddr(const std::string& procedure_name) throw (DLLWrapperExcep);
		


	const HINSTANCE& getHandle() const { return dll_handle; }
	HINSTANCE& getHandle(){ return dll_handle; }

	const std::string& getPathName() const { return filename; }

private:

	HINSTANCE dll_handle;
	std::string filename;

};



#endif //__DLLWRAPPER_H_666_




