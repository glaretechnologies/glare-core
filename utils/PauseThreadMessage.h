/*=====================================================================
PauseThreadMessage.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Thu Jul 22 13:00:48 +1200 2010
=====================================================================*/
#pragma once


#include "ThreadMessage.h"
#include "mutex.h"
#include "Condition.h"


/*=====================================================================
PauseThreadMessage
------------------
=====================================================================*/
class PauseThreadMessage : public ThreadMessage
{
public:
	/*=====================================================================
	PauseThreadMessage
	------------------

	=====================================================================*/
	PauseThreadMessage() {} // Mutex* _num_paused_threads_mutex, int* _num_paused_threads, Condition* _all_threads_paused);

	virtual ~PauseThreadMessage() {}

	virtual ThreadMessage* clone() const { return new PauseThreadMessage(); } // num_paused_threads_mutex, num_paused_threads, all_threads_paused); }

	virtual const std::string debugName() const { return "PauseThreadMessage"; }

//private:
	//Mutex* num_paused_threads_mutex;
	//int* num_paused_threads;
	//Condition* all_threads_paused;
};
