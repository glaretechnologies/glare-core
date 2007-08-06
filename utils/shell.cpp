/*=====================================================================
shell.cpp
---------
File created by ClassTemplate on Fri Aug 27 20:24:56 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "shell.h"


#include <assert.h>


Shell::Shell()
{	
	child_stdin = NULL;
	child_stdin_localend_dup = NULL;


	child_stdout = NULL;
	child_stdout_localend_dup = NULL;

	shell_process_handle = NULL;




	bool result;
	
	// Save the handle to the current STDOUT.
	HANDLE hSaveStdout = GetStdHandle(STD_OUTPUT_HANDLE);

	// Save the handle to the current STDIN.
	HANDLE hSaveStdin = GetStdHandle(STD_INPUT_HANDLE);

	
	//------------------------------------------------------------------------
	//create pipes and handles to the endpoints
	//------------------------------------------------------------------------
	SECURITY_ATTRIBUTES security_atttributes;

	// Set the bInheritHandle flag so pipe handles are inherited.
	security_atttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	security_atttributes.bInheritHandle = true;
	security_atttributes.lpSecurityDescriptor = NULL;


	HANDLE child_stdin_localend = NULL;
	HANDLE child_stdout_localend = NULL;


	//child_stdin = read end of pipe - shell reads from this.
	//child_stdin_localend = write end of pipe, this is used to write to the shell
	result = CreatePipe(&child_stdin, &child_stdin_localend, &security_atttributes, 0);
	assert(result);

	//child_stdout_localend = read end of pipe - used to read output of shell
	//child_stdout = write end of pipe - used by shell to write to.
	result = CreatePipe(&child_stdout_localend, &child_stdout, &security_atttributes, 0);
	assert(result);

	//------------------------------------------------------------------------
	//make the created pipes into the standard output and input
	//------------------------------------------------------------------------
	result = SetStdHandle(STD_OUTPUT_HANDLE, child_stdout);
	result = SetStdHandle(STD_INPUT_HANDLE, child_stdin);
	assert(result);



	//------------------------------------------------------------------------
	//Duplicate the write handle to the pipe so it is not inherited.  
	//------------------------------------------------------------------------	
	result = DuplicateHandle(GetCurrentProcess(), child_stdin_localend, 
		GetCurrentProcess(), &child_stdin_localend_dup, 
		0, FALSE,                  // not inherited       
		DUPLICATE_SAME_ACCESS ); 

	assert(result);

	CloseHandle(child_stdin_localend);  
	child_stdin_localend = NULL;

	//------------------------------------------------------------------------
	//Duplicate the read handle to the pipe so it is not inherited.  
	//------------------------------------------------------------------------	
	result = DuplicateHandle(GetCurrentProcess(), child_stdout_localend, 
		GetCurrentProcess(), &child_stdout_localend_dup, 
		0, FALSE,                  // not inherited       
		DUPLICATE_SAME_ACCESS ); 

	assert(result);

	CloseHandle(child_stdout_localend); 
	child_stdout_localend = NULL;



	//------------------------------------------------------------------------
	//create shell process
	//------------------------------------------------------------------------
	//HANDLE child_stderror;

	//NW:
	STARTUPINFO siStartInfo;

	memset(&siStartInfo, 0, sizeof(STARTUPINFO) );
	siStartInfo.cb = sizeof(STARTUPINFO);

	//siStartInfo.lpTitle = "command interpreter";

	siStartInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	siStartInfo.hStdInput = child_stdin;
	siStartInfo.hStdOutput = child_stdout;//child_stdout;
	siStartInfo.hStdError = child_stdout;//child_stderror;


	//------------------------------------------------------------------------
	//get pathname of the shell program
	//------------------------------------------------------------------------
	std::string shellname;
	shellname.resize(256);
	const int namesize = GetEnvironmentVariable("ComSpec", &(*shellname.begin()), shellname.size());

	shellname.resize(namesize);

	/*return FALSE;
#ifdef _UNICODE
_tcscat( shellCmd, _T(" /U") );
#else
_tcscat( shellCmd, _T(" /A") );
#endif*/


	PROCESS_INFORMATION piProcInfo;

	result = CreateProcess( NULL,
							&(*shellname.begin()),//shellCmd, // applicatin name
							NULL, // process security attributes
							NULL, // primary thread security attributes
							TRUE, // handles are inherited (this must be true to use std handles)
							CREATE_NEW_CONSOLE,//DETACHED_PROCESS, // creation flags
							NULL, // use parent's environment
							NULL, // use parent's current directory
							&siStartInfo, // STARTUPINFO pointer
							&piProcInfo); // receives PROCESS_INFORMATION

	if(result)
		shell_process_handle = piProcInfo.hProcess;

	assert(result);

	//------------------------------------------------------------------------
	//restore saved input and output
	//------------------------------------------------------------------------
	result = SetStdHandle(STD_OUTPUT_HANDLE, hSaveStdout);
	result = SetStdHandle(STD_INPUT_HANDLE, hSaveStdin);
	assert(result);
}


Shell::~Shell()
{
	//------------------------------------------------------------------------
	//terminate the shell process
	//------------------------------------------------------------------------
	//::TerminateProcess(

	bool result;

	//result = ::CloseHandle(this->shell_process_handle);
	result = ::TerminateProcess(this->shell_process_handle, 1);

	//TerminateProcess may return false if shell already killed, eg. by 'exit' command

//	assert(result);

	//------------------------------------------------------------------------
	//close pipes
	//------------------------------------------------------------------------
	result = ::CloseHandle(child_stdin);
	assert(result);
	result = ::CloseHandle(child_stdin_localend_dup);
	assert(result);
	result = ::CloseHandle(child_stdout);
	assert(result);
	result = ::CloseHandle(child_stdout_localend_dup);
	assert(result);


}

void Shell::enterString(const std::string& s)
{
	unsigned long numbyteswritten;
	const bool result = WriteFile(child_stdin_localend_dup, s.c_str(), s.size(), &numbyteswritten, NULL);
	assert(result);
}


void Shell::peekRead(std::string& output)
{
	unsigned long numbytesread;
	unsigned long numbytes_available;
	unsigned long unreadbytes;
	bool result;
	result = PeekNamedPipe(child_stdout_localend_dup, NULL, 0, &numbytesread, &numbytes_available, &unreadbytes);

	if(numbytes_available)
	{

		output.resize(numbytes_available);

		//unsigned long numbytesread;
		result = ReadFile(child_stdout_localend_dup, &(*output.begin()), numbytes_available, &numbytesread, NULL);
		//assert(result);

		assert(numbytes_available == numbytesread);

		if(!result)
		{
			int error = GetLastError();
			assert(0);
		}

		//assert(numbytesread <= 10000);
		//cout << output;

	}
	else
	{
		output.resize(0);
	}
}

