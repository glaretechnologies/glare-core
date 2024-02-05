/*=====================================================================
EventFD.cpp
-----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "EventFD.h"


#include "Exception.h"
#include "PlatformUtils.h"
#include "StringUtils.h"
#include "ConPrint.h"


#if defined(_WIN32) || defined(__APPLE__) || defined(EMSCRIPTEN)
#else
#include <sys/eventfd.h>
#include <unistd.h>
#endif
#include <assert.h>


EventFD::EventFD()
{
#if defined(_WIN32) || defined(__APPLE__) || defined(EMSCRIPTEN)
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
#if defined(_WIN32) || defined(__APPLE__) || defined(EMSCRIPTEN)
#else
	close(efd);
#endif
}


void EventFD::notify()
{
#if defined(_WIN32) || defined(__APPLE__) || defined(EMSCRIPTEN)
#else
	uint64 val = 1;
	write(efd, &val, 8);
#endif
}


uint64 EventFD::read()
{
#if defined(_WIN32) || defined(__APPLE__) || defined(EMSCRIPTEN)
#else
	uint64 val;
	const ssize_t res = ::read(efd, &val, 8); // Note that this returns the number of bytes read, or an error code.
	assert(res == 8);
	return val;
#endif
	return 0;
}
