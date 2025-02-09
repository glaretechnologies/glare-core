/*=====================================================================
GlareProcess.cpp
----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "GlareProcess.h"


#include "IncludeWindows.h"
#include "StringUtils.h"
#include "FileUtils.h"
#include "Exception.h"
#include "PlatformUtils.h"
#include "ConPrint.h"
#if !defined(_WIN32)
#include <unistd.h>
#endif


namespace glare
{


#if defined(_WIN32)
static void createPipe(SECURITY_ATTRIBUTES* sa, HandleWrapper& read_handle_out, HandleWrapper& write_handle_out)
{
	if(!CreatePipe(&read_handle_out.handle, &write_handle_out.handle, sa, 0))
		throw glare::Exception("CreatePipe failed: " + PlatformUtils::getLastErrorString());
}


static void setNoInherit(HandleWrapper& handle)
{
	if(!SetHandleInformation(handle.handle, HANDLE_FLAG_INHERIT, 0))
		throw glare::Exception("SetHandleInformation failed: " + PlatformUtils::getLastErrorString());
}
#endif


Process::Process(const std::string& program_path, const std::vector<std::string>& command_line_args)
{
	// Convert the command_line_args vector to a string.  We also want to escape double-quotes in each argument.
	// See https://msdn.microsoft.com/en-us/library/windows/desktop/17w5ykft.aspx ("Parsing C++ Command-Line Arguments")
	// This could could be tested to see if it is the inverse of CommandLineToArgvW (See https://msdn.microsoft.com/en-us/library/windows/desktop/bb776391(v=vs.85).aspx)

	// TODO: This escaping is probably wrong for Mac/Linux.

	std::string combined_args_string;
	for(size_t i=0; i<command_line_args.size(); ++i)
	{
		const std::string arg = command_line_args[i];
		std::string escaped;
		for(size_t z=0; z<arg.size(); ++z)
		{
			if(arg[z] == '"')
				escaped += "\\\""; // Replace double-quote with backslash,double-quote.
			else if(arg[z] == '\\')
			{
				if(z + 1 < arg.size() && arg[z + 1] == '"') // If next char is double-quote:
					// The arg contains backslash,double-quote.  We will replace this with backslash,backslash,backslash,double-quote.
					escaped += "\\\\\\\"";
				else
					// "Backslashes are interpreted literally, unless they immediately precede a double quotation mark." - so just append a backslash.
					escaped.push_back('\\');
			}
			else
				escaped.push_back(arg[z]);
		}

		combined_args_string += "\"" + escaped + "\"";
		if(i + 1 < command_line_args.size())
			combined_args_string += " ";
	}

#if defined(_WIN32)
	// See http://stackoverflow.com/questions/14147138/capture-output-of-spawned-process-to-string
	// also https://msdn.microsoft.com/en-us/library/windows/desktop/ms682499(v=vs.85).aspx

	SECURITY_ATTRIBUTES sa; 
	// Set the bInheritHandle flag so pipe handles are inherited. 
	sa.nLength = sizeof(SECURITY_ATTRIBUTES); 
	sa.bInheritHandle = TRUE; 
	sa.lpSecurityDescriptor = NULL; 


	// Create a pipe for the child process's STDERR. 
	HandleWrapper child_stderr_write_handle;
	createPipe(&sa, this->child_stderr_read_handle, child_stderr_write_handle);
	setNoInherit(this->child_stderr_read_handle); // Ensure the read handle to the pipe for STDERR is not inherited.
	
	// Create a pipe for the child process's STDOUT. 
	HandleWrapper child_stdout_write_handle;
	createPipe(&sa, this->child_stdout_read_handle, child_stdout_write_handle);
	setNoInherit(this->child_stderr_read_handle); // Ensure the read handle to the pipe for STDOUT is not inherited.

	// Create a pipe for the child process's STDIN. 
	HandleWrapper child_stdin_read_handle;
	createPipe(&sa, child_stdin_read_handle, this->child_stdin_write_handle);
	setNoInherit(this->child_stdin_write_handle); // Ensure the write handle to the pipe for STDIN is not inherited. 


	STARTUPINFO startupInfo;
	ZeroMemory(&startupInfo, sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.hStdError = child_stderr_write_handle.handle;
	startupInfo.hStdOutput = child_stdout_write_handle.handle;
	startupInfo.hStdInput = child_stdin_read_handle.handle;
	startupInfo.dwFlags |= STARTF_USESTDHANDLES;

	PROCESS_INFORMATION procInfo;
	ZeroMemory(&procInfo, sizeof(procInfo));

	std::wstring w_command_line = StringUtils::UTF8ToPlatformUnicodeEncoding(combined_args_string);
	if(CreateProcess(
		StringUtils::UTF8ToPlatformUnicodeEncoding(program_path).c_str(),		// application name
		w_command_line.data(), // command line
		NULL,		// process security attributes
		NULL,		// primary thread security attributes 
		TRUE,		// handles are inherited 
		CREATE_NO_WINDOW, // creation flags - use CREATE_NO_WINDOW to prevent console window popping up.
		NULL,
		NULL,
		&startupInfo,
		&procInfo
		) == 0)
	{
		// Failure
		throw glare::Exception("CreateProcess Failed for program '" + program_path + "': " + PlatformUtils::getLastErrorString());
	}

	// "Handles in PROCESS_INFORMATION must be closed with CloseHandle when they are no longer needed." - https://docs.microsoft.com/en-us/windows/desktop/api/processthreadsapi/nf-processthreadsapi-createprocessa
	// So hang on to them with HandleWrapper and close later.
	this->process_handle.handle = procInfo.hProcess;
	this->thread_handle.handle = procInfo.hThread;
#else
	this->exit_code = 0;
	
	this->fp = popen((program_path + " " + combined_args_string).c_str(), "r");
	if(fp == NULL)
		throw glare::Exception("popen failed: " + PlatformUtils::getLastErrorString());
#endif
}


Process::~Process()
{
#if !defined(_WIN32)
	if(fp)
		pclose(fp);
#endif
}


void Process::terminateProcess()
{
#if defined(_WIN32)
	const BOOL res = TerminateProcess(this->process_handle.handle, /*exit-code=*/1);
	if(!res)
		throw glare::Exception("TerminateProcess Failed: " + PlatformUtils::getLastErrorString());
#else
	throw glare::Exception("Not implemented.");
#endif
}


bool Process::isStdOutReadable()
{
#if defined(_WIN32)
	DWORD bytes_avail;
	const BOOL success = PeekNamedPipe(child_stdout_read_handle.handle, /*buf=*/NULL, 0, /*bytes_read=*/NULL, &bytes_avail, /*bytes_left_this_msg=*/NULL);
	if(success)
		return bytes_avail > 0;
	else
		return false; // In the case where the process has been terminated, don't throw an exception, just return false.
#else
	throw glare::Exception("Not implemented.");
#endif
}


const std::string Process::readStdOut()
{
#if defined(_WIN32)
	char buf[2048]; 
	DWORD dwRead;
	const BOOL bSuccess = ReadFile(child_stdout_read_handle.handle, buf, sizeof(buf), &dwRead, NULL);
	if(!bSuccess || dwRead == 0)
		return ""; 

	return std::string(buf, dwRead);
#else
	if(fp)
	{
		char buf[2048];
		const ssize_t bytes_read = fread(buf, sizeof(buf), 1, fp);
		if(bytes_read == 0) // If EOF:
			return "";
		else
			return std::string(buf, bytes_read);
	}
	else
		return "";
#endif
}


const std::string Process::readStdErr()
{
#if defined(_WIN32)
	char buf[2048]; 
	DWORD dwRead;
	const BOOL bSuccess = ReadFile(child_stderr_read_handle.handle, buf, sizeof(buf), &dwRead, NULL);
	if(!bSuccess || dwRead == 0)
		return ""; 

	return std::string(buf, dwRead);
#else
	// We can't read from stderr with popen(), so just return "".
	return "";
#endif
}


void Process::writeToProcessStdIn(const ArrayRef<unsigned char>& data)
{
#if defined(_WIN32)
	size_t bytes_written_total = 0;
	while(bytes_written_total < data.size())
	{
		DWORD write_size = 0;
		const BOOL res = WriteFile(child_stdin_write_handle.handle, data.data() + bytes_written_total, (DWORD)(data.size() - bytes_written_total), &write_size, /*lpOverlapped=*/NULL);
		bytes_written_total += write_size;

		if(res == FALSE)
		{
			if(GetLastError() == ERROR_IO_PENDING)
			{
			}
			else
				throw glare::Exception("WriteFile failed: " + PlatformUtils::getLastErrorString());
		}
	}
#else
	throw glare::Exception("Not implemented.");
#endif
}


bool Process::isProcessAlive()
{
#if defined(_WIN32)
	DWORD exit_code;
	if(!GetExitCodeProcess(this->process_handle.handle, &exit_code))
		throw glare::Exception("GetExitCodeProcess failed: " + PlatformUtils::getLastErrorString());
	return exit_code == STILL_ACTIVE;
#else
	throw glare::Exception("Not implemented.");
#endif
}


int Process::getExitCode()
{
#if defined(_WIN32)
	DWORD exit_code;
	if(!GetExitCodeProcess(this->process_handle.handle, &exit_code))
		throw glare::Exception("GetExitCodeProcess failed: " + PlatformUtils::getLastErrorString());
	return exit_code;
#else
	if(fp)
	{
		const int status = pclose(fp);
		if(status == -1)
			throw glare::Exception("pclose failed: " + PlatformUtils::getLastErrorString());
		this->exit_code = status; // NOTE: this is the exit value of the command interpreter, not our actual process.
		fp = NULL;
	}
	return this->exit_code;
#endif
}


void Process::readAllRemainingStdOutAndStdErr(std::string& stdout_out, std::string& stderr_out)
{
	stdout_out.clear();
	stderr_out.clear();

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


} // end namespace glare


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"


void glare::Process::test()
{
	conPrint("Process::test()");
#if defined(_WIN32)
	try
	{
		wchar_t buf[2048];
		testAssert(GetWindowsDirectory(buf, 2048) != 0);
		const std::string cmd_exe_path = StringUtils::PlatformToUTF8UnicodeEncoding(buf) + "\\system32\\ipconfig.exe";
		std::vector<std::string> command_line_args;
		command_line_args.push_back(cmd_exe_path);
		command_line_args.push_back("/all");
		Process p(cmd_exe_path, command_line_args);
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
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	// Test trying to start an invalid process, e.g. test exception handling.
	// Note that on Linux this doesn't actually throw an exception since popen() executes a shell.
	try
	{
		const std::string cmd_exe_path = "invalid_exe_path";
		std::vector<std::string> command_line_args;
		command_line_args.push_back(cmd_exe_path);
		command_line_args.push_back("/all");

		Process p(cmd_exe_path, command_line_args);

		failTest("Expected to throw exception before here.");
	}
	catch(glare::Exception&)
	{
	}
#else
	try
	{
		std::vector<std::string> command_line_args;
		command_line_args.push_back("-l");
		Process p("ls", command_line_args);
		while(1)
		{
			const std::string output = p.readStdOut();
			conPrint(output);
			if(output.empty())
				break;
		}
		conPrint("Exit code: " + toString(p.getExitCode()));
		testAssert(p.getExitCode() == 0);
	}
	catch(glare::Exception& e)
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
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}*/

	conPrint("Process::test() done.");
}


#endif // BUILD_TESTS
