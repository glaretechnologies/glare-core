/*=====================================================================
GLUIButton.h
------------
Copyright Glare Technologies Limited 2021 -
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
GLUIButton
----------

=====================================================================*/
class GLUIButton : public GLUIWidget
{
public:
	GLUIButton();
	~GLUIButton();

	void create(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& tex_path, const Vec2f& botleft, const Vec2f& dims,
		const std::string& tooltip);

	void destroy();

	virtual bool doHandleMouseClick(const Vec2f& coords) override;
	virtual bool doHandleMouseMoved(const Vec2f& coords) override;

	void setPosAndDims(const Vec2f& botleft, const Vec2f& dims);

	void setToggled(bool toggled_);

	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef overlay_ob;

	bool toggleable;
	bool toggled;

	GLUICallbackHandler* handler;
private:
	GLARE_DISABLE_COPY(GLUIButton);

	
};


typedef Reference<GLUIButton> GLUIButtonRef;
