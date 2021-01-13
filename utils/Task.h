/*=====================================================================
Task.h
------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "Reference.h"
#include <stdlib.h>


namespace glare
{


/*=====================================================================
Task
----
Tests in TaskTests.
=====================================================================*/
class Task : public ::ThreadSafeRefCounted
{
public:
	Task();
	virtual ~Task();

	virtual void run(size_t thread_index) = 0;

	virtual void cancelTask() {} 
private:

};


typedef Reference<Task> TaskRef;


} // end namespace glare 
