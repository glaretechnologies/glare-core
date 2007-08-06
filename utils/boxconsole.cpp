/*=====================================================================
boxconsole.cpp
--------------
File created by ClassTemplate on Thu Sep 05 12:43:24 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "boxconsole.h"

#include <assert.h>


BoxConsole::BoxConsole(const std::string& boxtitle)
{
	debugInputHandle = NULL;
	debugOutputHandle = NULL;

	BOOL success = AllocConsole();

	assert(success);

	SetConsoleTitle(boxtitle.c_str());
	debugInputHandle = GetStdHandle(STD_INPUT_HANDLE);
	debugOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
}


BoxConsole::~BoxConsole()
{
	FreeConsole();
}


void BoxConsole::print(const std::string& line)
{
	std::string line_ = line + "\n";

	DWORD num;
	WriteConsole(debugOutputHandle, line_.c_str(), line_.size()+1, &num, NULL);
}




