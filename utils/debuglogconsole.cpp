#include "debuglogconsole.h"

#include "../outputwindow/outputwindow.h"
//#include "logfile.h"


const std::string DEFAULT_LOGFILENAME = "debuglog.txt";


DebugLogConsole::DebugLogConsole(HINSTANCE hInst, HWND hWndParent, bool log_to_file_)
:	log_to_file(log_to_file_)
{
	outputwindow = NULL;
	
	//logfile = new Logfile(LOGFILENAME.c_str());

	if(log_to_file)
		logfile.open(DEFAULT_LOGFILENAME.c_str());


	RECT rect;
	rect.left = 500;
	rect.right = 1000;
	rect.top = 0;
	rect.bottom = 400;
	//NOTE: This is causing hangs in multithreaded version
	outputwindow = new OutputWindow(hInst, hWndParent, &rect);

	
}
	
DebugLogConsole::~DebugLogConsole()
{
	delete outputwindow;
	outputwindow = NULL;

	//delete logfile;

}

void DebugLogConsole::startLoggingToFile(const std::string& logfilepath)
{
	if(log_to_file)
	{
		logfile.close();
	}

	logfile.open(logfilepath.c_str());
	log_to_file = true;
}

void DebugLogConsole::print(const std::string& text)
{
	//logfile->write(text);
	if(log_to_file)
		logfile << text << std::endl;

	if(outputwindow)
		outputwindow->printLine((char*)text.c_str());
}

DebugLogConsole* DebugLogConsole::instance = NULL;




void dPrint(const std::string& text)
{
	if(DebugLogConsole::instanceCreated())
		DebugLogConsole::getInstance().print(text);
}

void dPrint(const char* text, ...)
{
	/*static */char textBuffer[1024];
	va_list args;
	va_start(args, text);
	vsprintf(textBuffer, text, args);
	va_end(args);

	if(DebugLogConsole::instanceCreated())
		DebugLogConsole::getInstance().print(std::string(textBuffer));
}
