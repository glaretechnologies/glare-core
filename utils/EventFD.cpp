/*=====================================================================
EventFD.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2014-10-03 22:57:41 +0100
=====================================================================*/
#include "EventFD.h"


#if defined(_WIN32) || defined(OSX)
#else
#include <sys/eventfd.h>
#include <unistd.h>
#endif


EventFD::EventFD()
{
#if defined(_WIN32) || defined(OSX)
#else
	efd = eventfd(
		0, // initial value
		0 // flags
	);
#endif
}


EventFD::~EventFD()
{
}


void EventFD::notify()
{
#if defined(_WIN32) || defined(OSX)
#else
	uint64 val = 1;
	write(efd, &val, 8);
#endif
}


uint64 EventFD::read()
{
#if defined(_WIN32) || defined(OSX)
#else
	uint64 val;
	::read(efd, &val, 8);
	return val;
#endif
	return 0;
}
