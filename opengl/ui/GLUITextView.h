/*=====================================================================
GLUITextView.h
-------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUICallbackHandler.h"
#include "GLUIText.h"
#include "../OpenGLEngine.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/Rect2.h"
#include "../graphics/colour3.h"
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

	struct GLUITextViewCreateArgs
	{
		GLUITextViewCreateArgs();
		std::string tooltip;
		Colour3f background_colour; // Linear
		float background_alpha;
		Colour3f text_colour; // Linear
		float text_alpha;
		int padding_px;
		int font_size_px; // default = 20
	};
	void create(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& text, const Vec2f& botleft, const GLUITextViewCreateArgs& args);

	void destroy();

	void setText(GLUI& glui, const std::string& new_text);

	void setColour(const Colour3f& col);

	void setPos(GLUI& glui, const Vec2f& botleft);

	void setVisible(bool visible);

	const Vec2f getDims() const;

	// Called when e.g. the viewport changes size
	virtual void updateGLTransform(GLUI& glui) override;

	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef background_overlay_ob;
	GLUITextRef glui_text;

private:
	GLARE_DISABLE_COPY(GLUITextView);

	void updateBackgroundOverlayObTransform(GLUI& glui);

	GLUITextViewCreateArgs args;
	std::string text;

	Vec2f botleft; // in GL UI coords
};


typedef Reference<GLUITextView> GLUITextViewRef;
