/*=====================================================================
boxconsole.h
------------
File created by ClassTemplate on Thu Sep 05 12:43:24 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __BOXCONSOLE_H_666_
#define __BOXCONSOLE_H_666_


#include <windows.h>
#include <string>

/*=====================================================================
BoxConsole
----------

=====================================================================*/
class BoxConsole
{
public:
	/*=====================================================================
	BoxConsole
	----------
	
	=====================================================================*/
	BoxConsole(const std::string& boxtitle = "BoxConsole");

	~BoxConsole();


	void print(const std::string& line);

private:
	HANDLE debugInputHandle, debugOutputHandle;

};



#endif //__BOXCONSOLE_H_666_




