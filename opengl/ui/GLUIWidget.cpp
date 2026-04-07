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
	fixed_size(100, 100),
	m_parent(nullptr),
	glui(nullptr),
	opengl_engine(nullptr)
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


void GLUIWidget::viewportResized()
{
}


void GLUIWidget::setFixedWidthPx(float x_px)
{ 
	sizing_type_x = SizingType_FixedSizePx; 
	fixed_size.x = x_px;

	this->rect = Rect2f(this->rect.getMin(), Vec2f(this->rect.getMin().x + glui->getUIWidthForDevIndepPixelWidth(fixed_size.x), this->rect.getMax().y));
}


void GLUIWidget::setFixedHeightPx(float y_px)
{ 
	sizing_type_y = SizingType_FixedSizePx; 
	fixed_size.y = y_px;

	this->rect = Rect2f(this->rect.getMin(), Vec2f(this->rect.getMax().x, this->rect.getMin().x + glui->getUIWidthForDevIndepPixelWidth(fixed_size.x)));
}


void GLUIWidget::setFixedDimsPx(const Vec2f& dims_px)
{
	sizing_type_x = SizingType_FixedSizePx; 
	sizing_type_y = SizingType_FixedSizePx; 
	fixed_size = dims_px;

	this->rect = Rect2f(this->rect.getMin(), this->rect.getMin() + glui->getUIWidthForDevIndepPixelWidths(fixed_size));
}


Vec2f GLUIWidget::getMinDims() const
{
	Vec2f min_dims(0.f);

	if(sizing_type_x == SizingType::SizingType_FixedSizePx)
		min_dims.x = glui->getUIWidthForDevIndepPixelWidth(fixed_size.x);
	else if(sizing_type_x == SizingType::SizingType_FixedSizeUICoords)
		min_dims.x = fixed_size.x;

	if(sizing_type_y == SizingType::SizingType_FixedSizePx)
		min_dims.y = glui->getUIWidthForDevIndepPixelWidth(fixed_size.y);
	else if(sizing_type_y == SizingType::SizingType_FixedSizeUICoords)
		min_dims.y = fixed_size.y;

	assert(min_dims.x > 0.f && min_dims.y > 0.f);
	return min_dims;
}


Vec2f GLUIWidget::computeDims(const Vec2f& old_dims) const
{
	Vec2f dims = old_dims;

	if(this->sizing_type_x == SizingType_FixedSizePx)
		dims.x = glui->getUIWidthForDevIndepPixelWidth(this->fixed_size.x);
	else if(this->sizing_type_x == SizingType_FixedSizeUICoords)
		dims.x = this->fixed_size.x;

	if(this->sizing_type_y == SizingType_FixedSizePx)
		dims.y = glui->getUIWidthForDevIndepPixelWidth(this->fixed_size.y);
	else if(this->sizing_type_y == SizingType_FixedSizeUICoords)
		dims.y = this->fixed_size.y;

	return dims;
}

