/*=====================================================================
testworker.h
------------
File created by ClassTemplate on Mon Sep 29 02:54:22 2003
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TESTWORKER_H_666_
#define __TESTWORKER_H_666_



#include "poolworkerthread.h"
#include <string>

/*=====================================================================
TestWorker
----------

=====================================================================*/
class TestWorker : public PoolWorkerThread<std::string>
{
public:
	/*=====================================================================
	TestWorker
	----------
	
	=====================================================================*/
	TestWorker(ThreadPool<std::string>* pool, float poll_period);

	~TestWorker();


	virtual void doHandleTask(std::string& task);

};



#endif //__TESTWORKER_H_666_




