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


bool GLUIWidget::handleMouseClick(const Vec2f& coords)
{
	return doHandleMouseClick(coords);
}


bool GLUIWidget::handleMouseMoved(const Vec2f& coords)
{
	return doHandleMouseMoved(coords);
}