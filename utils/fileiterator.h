/*=====================================================================
fileiterator.h
--------------
File created by ClassTemplate on Thu Mar 07 08:57:18 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __FILEITERATOR_H_666_
#define __FILEITERATOR_H_666_

#include "platform.h"
#include <string>
#include <vector>
class DirIterator;


class FileIteratorExcep
{
public:
	FileIteratorExcep(const std::string& s_) : s(s_) {}
	~FileIteratorExcep(){}

	const std::string what() const { return s; }

private:
	std::string s;
};

/*=====================================================================
FileIterator
------------



=====================================================================*/
class FileIterator
{
public:
	/*=====================================================================
	FileIterator
	------------
	throws FileIteratorExcep
	=====================================================================*/
	FileIterator(const std::string& dir, bool recursively_iterate);

	~FileIterator();


	void advance();

	inline bool isFinished() const;


	//-----------------------------------------------------------------
	//info about the dereferenced file/dir
	//-----------------------------------------------------------------
	const std::string& getFileName() const;
	const std::string getFullFileName() const;
	const std::string getDirectory() const;
	bool isDirectory() const;
	int getFileSize() const;

private:
	std::string rootdir;

	std::vector<DirIterator*> dirstack;
	bool finished;
	bool recursively_iterate;
};



bool FileIterator::isFinished() const
{
	return finished;
}


#endif //__FILEITERATOR_H_666_




