/*=====================================================================
GLUICallbackHandler.h
---------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"


class GLUIMouseWheelEvent;


class GLUICallbackEvent
{
public:
	GLUICallbackEvent() : accepted(false) {}
	GLUIWidget* widget;
	bool accepted; // If true, event is not propagated to other widgets.
};


class GLUICallbackMouseWheelEvent : public GLUICallbackEvent
{
public:
	GLUICallbackMouseWheelEvent() {}

	const GLUIMouseWheelEvent* wheel_event;
};


class GLUISliderValueChangedEvent : public GLUICallbackEvent
{
public:
	GLUISliderValueChangedEvent() {}

	double value;
};


class GLUICallbackHandler// : public RefCounted
{
public:
	virtual ~GLUICallbackHandler() {}

	virtual void eventOccurred(GLUICallbackEvent& /*event*/) = 0;

	virtual void mouseWheelEventOccurred(GLUICallbackMouseWheelEvent& /*event*/) {}

	virtual void sliderValueChangedEventOccurred(GLUISliderValueChangedEvent& /*event*/) {}
};


//typedef Reference<GLUICallbackHandler> GLUICallbackHandlerRef;
