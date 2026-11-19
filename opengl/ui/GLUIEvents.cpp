/*=====================================================================
GLUIEvents.cpp
--------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "GLUIEvents.h"


GLUIMouseEvent::GLUIMouseEvent(const MouseEvent& event, const Vec2f& ui_coords_)
:	cursor_pos(event.cursor_pos),
	gl_coords(event.gl_coords),
	ui_coords(ui_coords_),
	modifiers(event.modifiers),
	button(event.button),
	button_state(event.button_state),
	accepted(event.accepted)
{}


GLUIMouseWheelEvent::GLUIMouseWheelEvent(const MouseWheelEvent& event, const Vec2f& ui_coords_)
:	cursor_pos(event.cursor_pos),
	gl_coords(event.gl_coords),
	ui_coords(ui_coords_),
	angle_delta(event.angle_delta),
	modifiers(event.modifiers),
	accepted(event.accepted)
{}
