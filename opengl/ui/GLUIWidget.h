/*=====================================================================
GLUIWidget.h
------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../OpenGLEngine.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/Rect2.h"
#include <string>


class GLUIMouseWheelEvent;


/*=====================================================================
GLUIButton
----------

=====================================================================*/
class GLUIWidget : public RefCounted
{
public:
	GLUIWidget();
	virtual ~GLUIWidget();

	bool handleMouseClick(const Vec2f& coords);
	bool handleMouseMoved(const Vec2f& coords);
	bool handleMouseWheelEvent(const Vec2f& coords, const GLUIMouseWheelEvent& event);

	virtual bool doHandleMouseClick(const Vec2f& coords) { return false; } // Returns true if event accepted (e.g. should not be passed on)
	virtual bool doHandleMouseMoved(const Vec2f& coords) { return false; } // Returns true if event accepted (e.g. should not be passed on)
	virtual bool doHandleMouseWheelEvent(const Vec2f& coords, const GLUIMouseWheelEvent& event) { return false; } // Returns true if event accepted (e.g. should not be passed on)

	std::string client_data;

	std::string tooltip;

	Rect2f rect;
private:
	GLARE_DISABLE_COPY(GLUIWidget);
};


typedef Reference<GLUIWidget> GLUIWidgetRef;
