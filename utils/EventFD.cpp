/*=====================================================================
EventFD.cpp
-----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "EventFD.h"


#include "Exception.h"
#include "PlatformUtils.h"


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
	if(efd == -1) // "On error, -1 is returned and errno is set to indicate the error."
	{
		throw glare::Exception("eventfd() failed: " + PlatformUtils::getLastErrorString());
	}
#endif
}


EventFD::~EventFD()
{
#if defined(_WIN32) || defined(OSX)
#else
	close(efd);
#endif
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
