/*=====================================================================
ConPrint.cpp
------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "ConPrint.h"


#include "../utils/Lock.h"
#include "../utils/Mutex.h"
#include <iostream>
#include <stdlib.h>


// Use a mutex for conPrint etc.. to avoid intermingling of messages from multiple threads.
static Mutex print_mutex;


void conPrint(const string_view s)
{
	Lock lock(print_mutex);

	std::cout << s << std::endl;
}


void conPrintStr(const string_view s)
{
	Lock lock(print_mutex);

	std::cout << s;
}


void fatalError(const string_view s)
{
	{
	Lock lock(print_mutex);

	std::cerr << "Fatal Error: " << s << std::endl;
	}

	exit(1);
}


void stdErrPrint(const string_view s)
{
	std::cerr << s << std::endl;
}
