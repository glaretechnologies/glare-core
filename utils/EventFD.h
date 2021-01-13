/*=====================================================================
EventFD.h
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2014-10-03 22:57:41 +0100
=====================================================================*/
#pragma once


#include "Platform.h"


/*=====================================================================
EventFD
-------------------
See http://man7.org/linux/man-pages/man2/eventfd.2.html
=====================================================================*/
class EventFD
{
public:
	EventFD(); // Throws Indigo::Exception on failure.
	~EventFD();


	void notify(); // Writes 1

	uint64 read();

	int efd;
private:
	GLARE_DISABLE_COPY(EventFD);
};
