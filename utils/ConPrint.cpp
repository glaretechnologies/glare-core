/*=====================================================================
ConPrint.cpp
-------------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "ConPrint.h"


#include "../utils/Lock.h"
#include "../utils/Mutex.h"
#include "../utils/StringUtils.h"
#include <iostream>
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


void stdErrPrint(const std::string& s)
{
	std::cerr << s << std::endl;
}
