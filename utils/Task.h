/*=====================================================================
Task.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-10-05 21:56:27 +0100
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "Reference.h"
#include <stdlib.h>


namespace Indigo
{


/*=====================================================================
Task
-------------------

=====================================================================*/
class Task : public ::ThreadSafeRefCounted
{
public:
	Task();
	virtual ~Task();

	virtual void run(size_t thread_index) = 0;
private:

};


typedef Reference<Task> TaskRef;


} // end namespace Indigo 


