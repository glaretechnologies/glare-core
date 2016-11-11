/*=====================================================================
ConPrint.cpp
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-05-03 12:20:21 +0100
=====================================================================*/
#include "ConPrint.h"


#include "../utils/Lock.h"
#include "../utils/Mutex.h"
#include "../utils/Clock.h"
#include "../utils/StringUtils.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>


// Use a mutex for conPrint etc.. to avoid intermingling of messages from multiple threads.
static Mutex print_mutex;


void conPrint(const std::string& s)
{
	Lock lock(print_mutex);

	std::cout << s << std::endl;
}


void conPrintStr(const std::string& s)
{
	Lock lock(print_mutex);

	std::cout << s;
}


void fatalError(const std::string& s)
{
	{
	Lock lock(print_mutex);

	std::cerr << "Fatal Error: " << s << std::endl;
	}

	exit(1);
}


static std::ofstream* logfile = NULL;


void logPrint(const std::string& s)
{
	Lock lock(print_mutex);

	if(!logfile)
		logfile = new std::ofstream("log.txt");

	(*logfile) << (toString(Clock::getCurTimeRealSec()) + ": " + s + "\n");
}


void stdErrPrint(const std::string& s)
{
	std::cerr << s << std::endl;
}
