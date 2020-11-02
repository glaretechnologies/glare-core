/*=====================================================================
Process.h
---------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "IncludeWindows.h"
#include "HandleWrapper.h"
#include "ArrayRef.h"
#include <string>
#include <vector>
#include <stdio.h>


/*=====================================================================
Process
-------------------
Creates a process.  The standard output and standard error streams of the 
process can then be read from, and the standard input stream of the process
can be written to.

Does not currently terminate the process in the Process destructor -
so you may get 'zombie'/orphaned processes.

Not fully implemented on Mac and Linux.
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

	// Does not currently terminate the process in the destructor.
	~Process();

	void terminateProcess();

	bool isStdOutReadable();

	// Returns empty string if no more output (process is dead)
	const std::string readStdOut();
	
	// Returns empty string if no more output (process is dead)
	const std::string readStdErr();

	void writeToProcessStdIn(const ArrayRef<unsigned char>& data);

	bool isProcessAlive(); // Has process not been terminated yet?

	// Get the exit/return code from a terminated process.
	// May throw an exception if the process has not terminated.  Note: this is not required to block until the process has terminated, only use when you know the process has terminated.
	int getExitCode();

	void readAllRemainingStdOutAndStdErr(std::string& stdout_out, std::string& stderr_out);

	static void test();
private:

#if defined(_WIN32)
	HandleWrapper child_stdout_read_handle, child_stderr_read_handle, child_stdin_write_handle;
	HandleWrapper process_handle, thread_handle;
#else
	FILE* fp;
	int exit_code;
#endif
};
