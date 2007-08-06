/*=====================================================================
testworker.cpp
--------------
File created by ClassTemplate on Mon Sep 29 02:54:22 2003
Code By Nicholas Chapman.
=====================================================================*/
#include "testworker.h"


#include <iostream>

TestWorker::TestWorker(ThreadPool<std::string>* pool, float poll_period)
:	PoolWorkerThread<std::string>(pool, poll_period)
{
	
}


TestWorker::~TestWorker()
{
	
}


void TestWorker::doHandleTask(std::string& stringtask)
{
	std::cout << stringtask << std::endl;
}




