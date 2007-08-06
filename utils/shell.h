/*=====================================================================
shell.h
-------
File created by ClassTemplate on Fri Aug 27 20:24:56 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SHELL_H_666_
#define __SHELL_H_666_



#include <windows.h>
#include <string>


/*=====================================================================
Shell
-----
A windows shell.  Runs in another process.
Have to poll this object to get the shell output.
=====================================================================*/
class Shell
{
public:
	/*=====================================================================
	Shell
	-----
	
	=====================================================================*/
	Shell();

	~Shell();

	void enterString(const std::string& s);

	//returns any queued output from the shell
	void peekRead(std::string& output);

private:
	HANDLE child_stdin;
	//HANDLE child_stdin_localend;
	HANDLE child_stdin_localend_dup;


	HANDLE child_stdout;
	//HANDLE child_stdout_localend;
	HANDLE child_stdout_localend_dup;

	HANDLE shell_process_handle;
};



#endif //__SHELL_H_666_




