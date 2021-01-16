/*=====================================================================
FileHandle.h
-------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include <string>
#include <cstdio>


/*=====================================================================
FileHandle
-------------------
A wrapper around FILE* which takes care of fclose()ing the file pointer,
by calling it in the destructor.
=====================================================================*/
class FileHandle
{
public:
	FileHandle();
	FileHandle(const std::string& pathname, const std::string& openmode); // throws glare::Exception
	~FileHandle();

	void open(const std::string& pathname, const std::string& openmode); // throws glare::Exception

	FILE* getFile() { return f; }

private:
	FileHandle(const FileHandle& );
	FileHandle& operator = (const FileHandle& );
	FILE* f;
};
