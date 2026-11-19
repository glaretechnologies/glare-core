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


class GLUICallbackContextMenuEvent : public GLUICallbackEvent
{
public:
	GLUICallbackContextMenuEvent() {}

	Vec2f ui_coords; // Mouse position, e.g. for positioning the context menu.
};


class GLUISliderValueChangedEvent : public GLUICallbackEvent
{
public:
	GLUISliderValueChangedEvent() {}

	double value;
};


class GLUISpinBoxValueChangedEvent : public GLUICallbackEvent
{
public:
	GLUISpinBoxValueChangedEvent() {}

	double value;
};


class GLUIDropDownListValueChangedEvent : public GLUICallbackEvent
{
public:
	GLUIDropDownListValueChangedEvent() {}

	size_t index;
	std::string value;
};


class GLUICallbackHandler// : public RefCounted
{
public:
	virtual ~GLUICallbackHandler() {}

	virtual void eventOccurred(GLUICallbackEvent& /*event*/) = 0;

	virtual void closeWindowEventOccurred(GLUICallbackEvent& /*event*/) {}

	virtual void mouseWheelEventOccurred(GLUICallbackMouseWheelEvent& /*event*/) {}
	virtual void contextMenuRequestedEventOccurred(GLUICallbackContextMenuEvent& /*event*/) {} // Right mouse button pressed over event.widget.  Accept the event if a context menu was opened.

	virtual void sliderValueChangedEventOccurred(GLUISliderValueChangedEvent& /*event*/) {}

	virtual void spinBoxValueChangedEventOccurred(GLUISpinBoxValueChangedEvent& /*event*/) {}
};


//typedef Reference<GLUICallbackHandler> GLUICallbackHandlerRef;
