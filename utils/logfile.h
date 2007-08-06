/*===================================================================

  
  digital liberation front 2001
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|	      \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#ifndef __LOGFILE_H__
#define __LOGFILE_H__

#include <stdio.h>
#include <string>

//this is a totally useless class.

//NOTE: fixme, this has a crappy assert in it
class Logfile
{
public:

	Logfile(const char* filename);
	Logfile(const std::string& filename);
	~Logfile();

	void flushWrites();

	void write(const char* text, ...);
	void write(const std::string& text);

private:
	FILE* f;
};



#endif //__LOGFILE_H__