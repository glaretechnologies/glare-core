/*=====================================================================
ConPrint.cpp
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-05-03 12:20:21 +0100
=====================================================================*/
#include "ConPrint.h"


#include "../utils/lock.h"
#include "../utils/mutex.h"
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
