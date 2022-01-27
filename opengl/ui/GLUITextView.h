/*=====================================================================
GLUITextView.h
-------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUICallbackHandler.h"
#include "../OpenGLEngine.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/Rect2.h"
#include <string>


class GLUI;


/*=====================================================================
GLUITextView
------------
A widget that displays some text
=====================================================================*/
class GLUITextView : public GLUIWidget
{
public:
	GLUITextView();
	~GLUITextView();

	void create(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& text, const Vec2f& botleft, const Vec2f& dims,
		const std::string& tooltip);

	void destroy();

	void setText(GLUI& glui, const std::string& new_text);
	Vec2f getTextureDimensions() const;

	virtual bool doHandleMouseClick(const Vec2f& coords) override;
	virtual bool doHandleMouseMoved(const Vec2f& coords) override;

	void setPosAndDims(const Vec2f& botleft, const Vec2f& dims);


	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef overlay_ob;

	GLUICallbackHandler* handler;
private:
	GLARE_DISABLE_COPY(GLUITextView);

	std::string text;
};


typedef Reference<GLUITextView> GLUITextViewRef;
