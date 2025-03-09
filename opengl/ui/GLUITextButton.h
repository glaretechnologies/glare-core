/*=====================================================================
GLUITextButton.h
----------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUICallbackHandler.h"
#include "GLUITextView.h"
#include "../OpenGLEngine.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/Rect2.h"
#include <string>


/*=====================================================================
GLUITextButton
--------------

=====================================================================*/
class GLUITextButton : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();

		std::string tooltip;
		int font_size_px; // default = 14 pixels.

		Colour3f background_colour;
		Colour3f text_colour;

		Colour3f mouseover_background_colour;
		Colour3f mouseover_text_colour;
	};

	GLUITextButton(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& button_text, const Vec2f& botleft, const Vec2f& dims, const CreateArgs& args);
	~GLUITextButton();

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void doHandleMouseMoved(MouseEvent& event) override;
	virtual void updateGLTransform(GLUI& glui) override; // Called when e.g. the viewport changes size

	void setPos(const Vec2f& botleft);

	void setVisible(bool visible);

	virtual bool isVisible() override;

	GLUICallbackHandler* handler;
private:
	GLARE_DISABLE_COPY(GLUITextButton);

	GLUI* glui;
	Reference<OpenGLEngine> opengl_engine;

	GLUITextViewRef text_view;

	CreateArgs args;
};


typedef Reference<GLUITextButton> GLUITextButtonRef;
