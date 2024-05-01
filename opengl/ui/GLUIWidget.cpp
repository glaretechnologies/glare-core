/*=====================================================================
GLUIWidget.cpp
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "GLUIWidget.h"


GLUIWidget::GLUIWidget()
{
}


GLUIWidget::~GLUIWidget()
{
}


void GLUIWidget::handleMouseMoved(MouseEvent& mouse_event)
{
	return doHandleMouseMoved(mouse_event);
}


bool GLUIWidget::handleMouseWheelEvent(const Vec2f& coords, const GLUIMouseWheelEvent& event)
{
	return doHandleMouseWheelEvent(coords, event);
}


void GLUIWidget::handleKeyPressedEvent(KeyEvent& key_event)
{
	doHandleKeyPressedEvent(key_event);
}


void GLUIWidget::handleTextInputEvent(TextInputEvent& text_input_event)
{
	doHandleTextInputEvent(text_input_event);
}


void GLUIWidget::updateGLTransform(GLUI& /*glui*/)
{
}
