/*=====================================================================
GLUIEvents.h
------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "../../ui/UIEvents.h"


/*=====================================================================
GLUIMouseEvent
--------------
A mouse event as passed to GLUI widgets.
Has the members of MouseEvent, plus the mouse position in UI coordinates, computed once by GLUI.
=====================================================================*/
class GLUIMouseEvent
{
public:
	GLUIMouseEvent(const MouseEvent& event, const Vec2f& ui_coords);

	Vec2i cursor_pos; // GLWidget physical pixel coordinates
	Vec2f gl_coords;
	Vec2f ui_coords;
	uint32 modifiers;
	MouseButton button;
	uint32 button_state;

	bool accepted;
};


/*=====================================================================
GLUIMouseWheelEvent
-------------------
A mouse wheel event as passed to GLUI widgets.
Has the members of MouseWheelEvent, plus the mouse position in UI coordinates, computed once by GLUI.
=====================================================================*/
class GLUIMouseWheelEvent
{
public:
	GLUIMouseWheelEvent(const MouseWheelEvent& event, const Vec2f& ui_coords);

	Vec2i cursor_pos; // GLWidget physical pixel coordinates
	Vec2f gl_coords;
	Vec2f ui_coords;
	Vec2f angle_delta; // in degrees
	uint32 modifiers;

	bool accepted;
};
