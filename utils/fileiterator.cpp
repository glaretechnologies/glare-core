/*=====================================================================
fileiterator.cpp
----------------
File created by ClassTemplate on Thu Mar 07 08:57:18 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "fileiterator.h"

#include "diriterator.h"
#include <assert.h>



FileIterator::FileIterator(const std::string& dir, bool recursively_iterate_)
:	rootdir(dir)
{
	recursively_iterate = recursively_iterate_;
	finished = false;	


	try
	{
		dirstack.push_back(new DirIterator(dir));


		if(dirstack.back()->isFinished())
		{
			finished = true;
		}

	}
	catch(DirIteratorExcep& e)
	{
		throw FileIteratorExcep(e.what());
	}


	//advance();//this right?
}


FileIterator::~FileIterator()
{
	for(int i=0; i<dirstack.size(); ++i)
	{
		delete dirstack[i];
	}
	
}



void FileIterator::advance()
{
	assert(!finished);
	assert(!dirstack.empty());
	assert(!dirstack.back()->isFinished());
	//we will have just visited a valid node


	if(recursively_iterate)
	{


		if(dirstack.back()->isDirectory())
		{
			dirstack.push_back(new DirIterator(dirstack.back()->getFullFilename()));//recurse

			if(dirstack.back()->isFinished())//if new dir is empty
			{
				delete dirstack.back();//pop it
				dirstack.pop_back();

				//continue as normal
			}
			else
			{
				//visit the first node in this subdir
				return;
			}
		}


		dirstack.back()->advance();

		while(dirstack.back()->isFinished())
		{
			if(dirstack.size() > 1)
			{
				delete dirstack.back();//pop it
				dirstack.pop_back();

				dirstack.back()->advance();

				//visit node
				//return;
			}
			else
			{//else if this is the root dir we have run out of files in...
					
				//we have run out of files in the root dir, so are finished
				finished = true;
				return;
			}
		}


	}
	else //else if not recursively iterating
	{
		dirstack.back()->advance();

		if(dirstack.back()->isFinished())
			finished = true;
	}




		
		





/*
 // working postorder visiting:


	dirstack.back()->advance();


	while(1)
	{
		if(dirstack.back()->isFinished())
		{
			if(dirstack.size() > 1)
			{
				delete dirstack.back();//pop it
				dirstack.pop_back();

				//visit dir node
				return;

			}
			else
			{//else if this is the root dir we have run out of files in...

				//we have run out of files in the root dir, so are finished
				finished = true;
				return;
			}
		}
		else
		{
			//pointing to a file or dir

			if(dirstack.back()->isDirectory())
			{
				dirstack.push_back(new DirIterator(dirstack.back()->getFullFilename()));//recurse
			}
			else
			{
				return;//visit this file node
			}
		}
	}

*/



	/*if(dirstack.back()->isDirectory())
	{
		if(!havevisited)
		{
			havevisited = true;
			return;
		}
		else
		{
			//have visited it so recurse

			dirstack.push_back(new DirIterator(dirstack.back()->getFullFilename()));


			if(dirstack.back()->isFinished())
			{
				//pop it
				delete dirstack.back();
				dirstack.pop_back();

				//go to next node

			}

*/











/*


	dirstack.back()->advance();



	while(1)
	{
		//-----------------------------------------------------------------
		//advance to next file
		//-----------------------------------------------------------------

		if(dirstack.back()->isDirectory() && recursively_iterate)
		{
			//if current file being pointed at is a directory, recurse into it

			//-----------------------------------------------------------------
			//recurse into this dir
			//-----------------------------------------------------------------
			dirstack.push_back(new DirIterator(dirstack.back()->getFullFilename()));
		}
		else
		{
			//else just advance to the next file in this dir
			dirstack.back()->advance();
		}

		if(dirstack.back()->isFinished())//if no more files in current deepest dir
		{
			//-----------------------------------------------------------------
			//if current deepest dir is not the root dir, pop it
			//-----------------------------------------------------------------
			if(dirstack.size() > 1)
			{
				//-----------------------------------------------------------------
				//stop traversing the deepest dir
				//-----------------------------------------------------------------
				delete dirstack.back();
				dirstack.pop_back();


				//-----------------------------------------------------------------
				//go to the next file in the next dir up
				//-----------------------------------------------------------------
				dirstack.back()->advance();

				//continue while loop....
			}
			else
			{//else if this is the root dir we have run out of files in...

				//we have run out of files in the root dir, so are finished
				finished = true;
				return;
			}
		}
		else
		{	//else we have succesfully advanced to the next file
			break;
		}
	}

  */
}





const std::string& FileIterator::getFileName() const
{
	assert(!finished && !dirstack.empty());//should not call this if search has finished
	return dirstack.back()->getFileName();
}

const std::string FileIterator::getFullFileName() const
{
	assert(!finished && !dirstack.empty());//should not call this if search has finished
	return dirstack.back()->getFullFilename();
}

const std::string FileIterator::getDirectory() const
{
	assert(!finished && !dirstack.empty());//should not call this if search has finished
	return dirstack.back()->getFullDirname();
}


bool FileIterator::isDirectory() const
{
	assert(!finished && !dirstack.empty());//should not call this if search has finished
	return dirstack.back()->isDirectory();
}

int FileIterator::getFileSize() const
{
	assert(!finished && !dirstack.empty());//should not call this if search has finished
	return dirstack.back()->getFileSize();
}



