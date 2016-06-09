/*=====================================================================
ThreadMessage.h
---------------
File created by ClassTemplate on Sat Nov 03 08:25:44 2007
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "Reference.h"
#include <string>


/*=====================================================================
ThreadMessage
-------------
Base class for messages passed to and from threads.
ThreadMessages are expected to be immutable.
Therefore they don't have to be cloned to send to multiple threads.
=====================================================================*/
class ThreadMessage : public ThreadSafeRefCounted
{
public:
	ThreadMessage();

	virtual ~ThreadMessage();

	virtual const std::string debugName() const { return "ThreadMessage"; }
};


typedef Reference<ThreadMessage> ThreadMessageRef;
