/*=====================================================================
StreamShouldAbortCallback.h
---------------------------
Copyright Glare Technologies Limited 2013 -
=====================================================================*/
#pragma once


/*=====================================================================
StreamShouldAbortCallback
-------------------------
=====================================================================*/
class StreamShouldAbortCallback
{
public:
	StreamShouldAbortCallback();
	virtual ~StreamShouldAbortCallback();

	virtual bool shouldAbort() = 0;
};
