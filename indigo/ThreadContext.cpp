/*=====================================================================
ThreadContext.cpp
-----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "ThreadContext.h"


ThreadContext::ThreadContext()
{}


ThreadContext::~ThreadContext()
{}


static GLARE_THREAD_LOCAL ThreadContext* thread_local_context;


void ThreadContext::setThreadLocalContext(ThreadContext* context)
{
	thread_local_context = context;
}


ThreadContext* ThreadContext::getThreadLocalContext()
{
	return thread_local_context;
}
