/*=====================================================================
GLUIWidget.cpp
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "GLUIWidget.h"


#include "GLUI.h"


GLUIWidget::GLUIWidget()
:	m_z(0.f),
	sizing_type_x(SizingType_Expanding),
	sizing_type_y(SizingType_Expanding),
	fixed_size(100, 100)
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


void GLUIWidget::updateGLTransform()
{
}


void GLUIWidget::setFixedWidthPx(float x_px, GLUI& gl_ui)
{ 
	sizing_type_x = SizingType_FixedSizePx; 
	fixed_size.x = x_px;

	this->rect = Rect2f(this->rect.getMin(), Vec2f(this->rect.getMin().x + gl_ui.getUIWidthForDevIndepPixelWidth(fixed_size.x), this->rect.getMax().y));
}


void GLUIWidget::setFixedHeightPx(float y_px, GLUI& gl_ui)
{ 
	sizing_type_y = SizingType_FixedSizePx; 
	fixed_size.y = y_px;

	this->rect = Rect2f(this->rect.getMin(), Vec2f(this->rect.getMax().x, this->rect.getMin().x + gl_ui.getUIWidthForDevIndepPixelWidth(fixed_size.x)));
}


void GLUIWidget::setFixedDimsPx(const Vec2f& dims_px, GLUI& gl_ui)
{
	sizing_type_x = SizingType_FixedSizePx; 
	sizing_type_y = SizingType_FixedSizePx; 
	fixed_size = dims_px;

	this->rect = Rect2f(this->rect.getMin(), this->rect.getMin() + gl_ui.getUIWidthForDevIndepPixelWidths(fixed_size));
}
