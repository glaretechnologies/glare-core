/*=====================================================================
Process.h
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2016-05-08 19:24:12 +0100
=====================================================================*/
#pragma once


#include <string>
#include <vector>
#include <stdio.h>
#include "IncludeWindows.h"
#include "ArrayRef.h"


/*=====================================================================
Process
-------------------
Creates a process.  The standard output and standard error streams of the 
process can then be read from.
=====================================================================*/
class Process
{
public:
	/*
	program_path - full path to the program to execute.
	command_line_args - arguments to the program.  Note that the first argument, by convention, should be the name of the program, for example
	
	Process p("D:\\indigo_installs\\Indigo x64 3.8.33\\indigo_console.exe", "\"D:\\indigo_installs\\Indigo x64 3.8.33\\indigo_console.exe\" -v");

	Throws Indigo::Exception on failure.
	*/
	Process(const std::string& program_path, const std::vector<std::string>& command_line_args);
	~Process();

	// Returns empty string if no more output (process is dead)
	const std::string readStdOut();
	
	// Returns empty string if no more output (process is dead)
	const std::string readStdErr();

	void writeToProcessStdIn(const ArrayRef<unsigned char>& data);

	int getExitCode(); // Throws exception if process not terminated.

	void readAllRemainingStdOutAndStdErr(std::string& stdout_out, std::string& stderr_out);

	static void test();
private:

#if defined(_WIN32)
	HANDLE process_handle;
	HANDLE child_stdout_read_handle, child_stderr_read_handle, child_stdin_write_handle;
#else
	FILE* fp;
	int exit_code;
#endif
};
