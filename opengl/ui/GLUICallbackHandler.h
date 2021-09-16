/*=====================================================================
GLUICallbackHandler.h
---------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"


class GLUICallbackEvent
{
public:
	GLUICallbackEvent() : accepted(false) {}
	GLUIWidget* widget;
	bool accepted; // If true, event is not propagated to other widgets.
};


class GLUICallbackHandler// : public RefCounted
{
public:
	virtual ~GLUICallbackHandler() {}

	virtual void eventOccurred(GLUICallbackEvent& event) = 0;
};


//typedef Reference<GLUICallbackHandler> GLUICallbackHandlerRef;
