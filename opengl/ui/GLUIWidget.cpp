/*=====================================================================
GLUIWidget.cpp
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "GLUIWidget.h"


GLUIWidget::GLUIWidget()
:	m_z(0.f)
{
}


GLUIWidget::~GLUIWidget()
{
}


void GLUIWidget::handleMouseMoved(MouseEvent& mouse_event)
{
	doHandleMouseMoved(mouse_event);
}


void GLUIWidget::handleMouseWheelEvent(MouseWheelEvent& event)
{
	doHandleMouseWheelEvent(event);
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
