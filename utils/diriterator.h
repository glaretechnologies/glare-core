/*=====================================================================
diriterator.h
-------------
File created by ClassTemplate on Thu Mar 07 09:05:43 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DIRITERATOR_H_666_
#define __DIRITERATOR_H_666_

// Stop windows.h from defining the min() and max() macros
#define NOMINMAX

#include "platform.h"
#include <windows.h>
#include <string>
#include <assert.h>

class DirIteratorExcep
{
public:
	DirIteratorExcep(const std::string& s_) : s(s_) {}
	~DirIteratorExcep(){}

	const std::string what() const { return s; }

private:
	std::string s;
};

/*=====================================================================
DirIterator
-----------

=====================================================================*/
class DirIterator
{
public:
	/*=====================================================================
	DirIterator
	-----------
	
	=====================================================================*/
	DirIterator(const std::string& dir);

	~DirIterator();


	void advance();


	inline bool isFinished() const { return finished; }


	//-----------------------------------------------------------------
	//ops on derefed files
	//-----------------------------------------------------------------
	inline const std::string& getFileName() const 
	{ 
		assert(!finished);
		return current_filename; 
	}

	inline bool isDirectory() const
	{
		assert(!finished);
		//NEWCODE: use != 0 to avoid warning
		return (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}


	inline const std::string& getFullDirname() const 
	{ 
		assert(!finished);
		return full_dirname; 
	}

	inline const std::string getFullFilename() const 
	{ 
		assert(!finished);
		return getFullDirname() + "\\" + getFileName();
	}
	
	inline int getFileSize() const
	{
		assert(!finished);
		return find_data.nFileSizeLow;
	}

private:
	std::string full_dirname;
	std::string current_filename;
	bool finished;
	HANDLE search_handle;
	WIN32_FIND_DATAA find_data;

};



#endif //__DIRITERATOR_H_666_




