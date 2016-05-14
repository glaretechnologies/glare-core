/*=====================================================================
Process.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2016-05-08 19:24:12 +0100
=====================================================================*/
#include "GlareProcess.h"


#include "IncludeWindows.h"
#include "StringUtils.h"
#include "FileUtils.h"
#include "Exception.h"
#include "PlatformUtils.h"




Process::Process(const std::string& program_path, const std::string& command_line_args)
{
#if defined(_WIN32)
	// See http://stackoverflow.com/questions/14147138/capture-output-of-spawned-process-to-string
	// also https://msdn.microsoft.com/en-us/library/windows/desktop/ms682499(v=vs.85).aspx

	SECURITY_ATTRIBUTES sa; 
    // Set the bInheritHandle flag so pipe handles are inherited. 
    sa.nLength = sizeof(SECURITY_ATTRIBUTES); 
    sa.bInheritHandle = TRUE; 
    sa.lpSecurityDescriptor = NULL; 

	// Create a pipe for the child process's STDERR. 
	HANDLE child_stderr_write_handle;
    if(!CreatePipe(&child_stderr_read_handle, &child_stderr_write_handle, &sa, 0))
		throw Indigo::Exception("CreatePipe failed: " + PlatformUtils::getLastErrorString());

    // Ensure the read handle to the pipe for STDERR is not inherited.
    if(!SetHandleInformation(child_stderr_read_handle, HANDLE_FLAG_INHERIT, 0))
		throw Indigo::Exception("SetHandleInformation failed: " + PlatformUtils::getLastErrorString());

	// Create a pipe for the child process's STDOUT. 
	HANDLE child_stdout_write_handle;
    if(!CreatePipe(&child_stdout_read_handle, &child_stdout_write_handle, &sa, 0))
		throw Indigo::Exception("CreatePipe failed: " + PlatformUtils::getLastErrorString());

    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if(!SetHandleInformation(child_stdout_read_handle, HANDLE_FLAG_INHERIT, 0))
		throw Indigo::Exception("SetHandleInformation failed: " + PlatformUtils::getLastErrorString());


	// Create a pipe for the child process's STDIN. 
	HANDLE child_stdin_write_handle, child_stdin_read_handle;
	if(!CreatePipe(&child_stdin_read_handle, &child_stdin_write_handle, &sa, 0))
		throw Indigo::Exception("CreatePipe failed: " + PlatformUtils::getLastErrorString());

	// Ensure the write handle to the pipe for STDIN is not inherited. 
	if(!SetHandleInformation(child_stdin_write_handle, HANDLE_FLAG_INHERIT, 0))
		throw Indigo::Exception("SetHandleInformation failed: " + PlatformUtils::getLastErrorString());


	STARTUPINFO startupInfo;
	ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
	startupInfo.hStdError = child_stderr_write_handle;
    startupInfo.hStdOutput = child_stdout_write_handle;
	startupInfo.hStdInput = child_stdin_read_handle;
    startupInfo.dwFlags |= STARTF_USESTDHANDLES;

	PROCESS_INFORMATION procInfo;
	ZeroMemory(&procInfo, sizeof(procInfo));

	std::wstring w_command_line = StringUtils::UTF8ToPlatformUnicodeEncoding(command_line_args);
	if(CreateProcess(
		StringUtils::UTF8ToPlatformUnicodeEncoding(program_path).c_str(),		// application name
		&(w_command_line[0]), // command line
		NULL,		// process security attributes
		NULL,		// primary thread security attributes 
		TRUE,		// handles are inherited 
		0,			// creation flags 
		NULL,
		NULL,
		&startupInfo,
		&procInfo
		) == 0)
	{
		// Failure
		throw Indigo::Exception("CreateProcess Failed: " + PlatformUtils::getLastErrorString());
	}

	this->process_handle = procInfo.hProcess;

	CloseHandle(child_stderr_write_handle);
    CloseHandle(child_stdout_write_handle);
#else
	throw Indigo::Exception("Process only implemented for Windows.");
#endif
}


Process::~Process()
{
}


const std::string Process::readStdOut()
{
#if defined(_WIN32)
	char buf[2048]; 
	DWORD dwRead;
	const BOOL bSuccess = ReadFile(child_stdout_read_handle, buf, sizeof(buf), &dwRead, NULL);
	if(!bSuccess || dwRead == 0)
		return ""; 

	return std::string(buf, dwRead);
#endif
}


const std::string Process::readStdErr()
{
#if defined(_WIN32)
	char buf[2048]; 
	DWORD dwRead;
	const BOOL bSuccess = ReadFile(child_stderr_read_handle, buf, sizeof(buf), &dwRead, NULL);
	if(!bSuccess || dwRead == 0)
		return ""; 

	return std::string(buf, dwRead);
#endif
}


int Process::getExitCode() // Throws exception if process not terminated.
{
#if defined(_WIN32)
	DWORD exit_code;
	if(!GetExitCodeProcess(this->process_handle, &exit_code))
		throw Indigo::Exception("GetExitCodeProcess failed: " + PlatformUtils::getLastErrorString());
	return exit_code;
#endif
}


void Process::readAllRemainingStdOutAndStdErr(std::string& stdout_out, std::string& stderr_out)
{
	stdout_out = "";
	stderr_out = "";

	bool stdout_open = true;
	bool stderr_open = true;
	while(stdout_open || stderr_open)
	{
		if(stdout_open)
		{
			const std::string s = readStdOut();
			if(s.empty())
				stdout_open = false;
			stdout_out += s;
		}
		if(stderr_open)
		{
			const std::string s = readStdErr();
			if(s.empty())
				stderr_open = false;
			stderr_out += s;
		}
	}
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"


void Process::test()
{
	conPrint("Process::test()");
#if defined(_WIN32)
	try
	{
		wchar_t buf[2048];
		testAssert(GetWindowsDirectory(buf, 2048) != 0);
		const std::string cmd_exe_path = StringUtils::PlatformToUTF8UnicodeEncoding(buf) + "\\system32\\cmd.exe";
		Process p(cmd_exe_path, cmd_exe_path + " /C ls");
		while(1)
		{
			const std::string output = p.readStdOut();
			//conPrint(output);
			if(output.empty())
				break;
		}
		testAssert(p.getExitCode() == 0);
		//conPrint("Exit code: " + toString(p.getExitCode()));
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
#endif
	/*
	try
	{
		//Process p("D:\\indigo_installs\\Indigo x64 3.8.33\\indigo_console.exe", "\"D:\\indigo_installs\\Indigo x64 3.8.33\\indigo_console.exe\" -v");
		Process p("C:\\windows\\system32\\cmd.exe", "C:\\windows\\system32\\cmd.exe /C azure bleh");

		std::string std_out, std_err;
		p.readAllRemainingStdOutAndStdErr(std_out, std_err);

		conPrint("std_out: " + std_out);
		conPrint("std_err: " + std_err);

		conPrint("Exit code: " + toString(p.getExitCode()));

		//PlatformUtils::Sleep(5000);
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}*/

}


#endif
