/*=====================================================================
Log.cpp
-------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "ConPrint.h"


#include <iostream>
#include <Mutex.h>
#include <Lock.h>


static Mutex print_mutex;


void print(const std::string& s)
{
	Lock lock(print_mutex);
	std::cout << s << "\n";
}
