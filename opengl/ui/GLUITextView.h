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
	struct CreateArgs
	{
		CreateArgs();
		std::string tooltip;
		Colour3f background_colour; // Linear
		float background_alpha;
		Colour3f text_colour; // Linear
		float text_alpha;
		int padding_px;
		int font_size_px; // default = 14
	};

	GLUITextView(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& text, const Vec2f& botleft, const CreateArgs& args);
	~GLUITextView();

	void destroy(); // Called by destructor

	void setText(GLUI& glui, const std::string& new_text);

	void setColour(const Colour3f& col);

	void setPos(GLUI& glui, const Vec2f& botleft);

	void setVisible(bool visible);

	virtual bool isVisible() override;

	const Vec2f getDims() const;

	const Rect2f getRect() const;

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void handleMouseRelease(MouseEvent& event) override;
	virtual void handleMouseDoubleClick(MouseEvent& event) override;
	virtual void doHandleMouseMoved(MouseEvent& event) override;
	virtual void handleLosingKeyboardFocus() override;
	virtual void updateGLTransform(GLUI& glui) override; // Called when e.g. the viewport changes size

	virtual void handleCutEvent(std::string& clipboard_contents_out) override;
	virtual void handleCopyEvent(std::string& clipboard_contents_out) override;

	GLUI* glui;
	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef background_overlay_ob;
	GLUITextRef glui_text;

private:
	GLARE_DISABLE_COPY(GLUITextView);

	void updateOverlayObTransforms();

	OverlayObjectRef selection_overlay_ob;

	int selection_start, selection_end;

	CreateArgs args;
	std::string text;

	Vec2f botleft; // in GL UI coords
};


typedef Reference<GLUITextView> GLUITextViewRef;
