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
#ifndef __DEBUGLOGCONSOLE_H__
#define __DEBUGLOGCONSOLE_H__

#include <fstream>
#include <assert.h>
#include <string>
#include <windows.h>



void dPrint(const std::string& text);
void dPrint(const char* text, ...);

//class Logfile;
class OutputWindow;

class DebugLogConsole
{
public:
	DebugLogConsole(HINSTANCE hInst, HWND hWndParent, bool log_to_file);
	~DebugLogConsole();

	void startLoggingToFile(const std::string& logfilepath);


	void print(const std::string& text);

	inline static bool instanceCreated(){ return instance != NULL; }

	inline static DebugLogConsole& getInstance();
	//inline static void setInstance(DebugLogConsole* instance);
	inline static void createInstance(HINSTANCE hInst, HWND hWndParent, bool log_to_file);
	inline static void deleteInstance();

private:
	static DebugLogConsole* instance;

	std::ofstream logfile;
	bool log_to_file;

	//Logfile* logfile;
	OutputWindow* outputwindow;
};

DebugLogConsole& DebugLogConsole::getInstance()
{
	assert(instance);
	return *instance;
}


/*void DebugLogConsole::setInstance(DebugLogConsole* instance_)
{
	assert(instance == NULL);
	instance = instance_;
}*/

void DebugLogConsole::createInstance(HINSTANCE hInst, HWND hWndParent, bool log_to_file)
{
	assert(instance == NULL);
	instance = new DebugLogConsole(hInst, hWndParent, log_to_file);
}

void DebugLogConsole::deleteInstance()
{
	delete instance;
	instance = NULL;
}









#endif //__DEBUGLOGCONSOLE_H__
