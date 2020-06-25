/*=====================================================================
ShouldCancelCallback.h
----------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


/*=====================================================================
ShouldCancelCallback
-------------------------

=====================================================================*/
class ShouldCancelCallback
{
public:
	virtual ~ShouldCancelCallback() {}

	virtual bool shouldCancel() = 0;
};


class DummyShouldCancelCallback : public ShouldCancelCallback
{
public:
	virtual bool shouldCancel() { return false; }
};
