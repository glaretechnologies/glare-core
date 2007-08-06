/*=====================================================================
diriterator.cpp
---------------
File created by ClassTemplate on Thu Mar 07 09:05:43 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "diriterator.h"




DirIterator::DirIterator(const std::string& dir)
:	full_dirname(dir)
{
	finished = false;
	search_handle = NULL;

	
	const std::string full_direc_name = dir;//NOTE: TEMP HACK TEST ME

	const std::string search_string = full_direc_name + "\\*";


	search_handle = FindFirstFile(search_string.c_str(), &find_data);

	if(search_handle == INVALID_HANDLE_VALUE)
		throw DirIteratorExcep("Could not find a file in direc '" + full_direc_name + "'");

	current_filename = find_data.cFileName;

	//assert(find_data.cFileName == ".");

	//go to next file
	//advance();

	//-----------------------------------------------------------------
	//skip if '..'
	//-----------------------------------------------------------------
	if(!finished && (current_filename == "." || current_filename == ".."))
	{
		advance();
	}

	if(!finished && (current_filename == "." || current_filename == ".."))
	{
		advance();
	}


	/*BOOL result = FindNextFile(search_handle, &find_data);

	if(!result)
		throw DirIteratorExcep("invalid directory: '" + dir + "'");
	//assert(result);

	//-----------------------------------------------------------------
	//advance to the first proper file
	//-----------------------------------------------------------------
	advance();*/

	
}


DirIterator::~DirIterator()
{
	assert(search_handle);

	//-----------------------------------------------------------------
	//free the search handle
	//-----------------------------------------------------------------
	BOOL result = FindClose(search_handle);

	assert(result);
}




void DirIterator::advance()
{
	//-----------------------------------------------------------------
	//advance to the next file
	//-----------------------------------------------------------------
	const bool found_file = FindNextFile(search_handle, &find_data);

	if(!found_file)//no more files in this direc
	{
		finished = true;
		return;
	}

	current_filename = find_data.cFileName;
}

/*
bool DirIterator::isFinished() const
{
	return finished;
}

const std::string& DirIterator::getFileName()//deref
{
	return current_filename;
}

bool DirIterator::isDirectory() const
{
	assert(!finished);
	return find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
}

*/

