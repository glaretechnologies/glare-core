/*=====================================================================
KillThreadMessage.h
-------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "ThreadMessage.h"


/*=====================================================================
KillThreadMessage
-----------------
If a thread gets this message, it should try and finish its work and terminate
ASAP.
=====================================================================*/
class KillThreadMessage : public ThreadMessage
{
public:
	KillThreadMessage();

	~KillThreadMessage();

	virtual const std::string debugName() const { return "KillThreadMessage"; }
};
