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
class GLUITextButton final : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();

		std::string tooltip;
		int font_size_px; // default = 14 pixels.

		Colour3f background_colour; // Colour3f(1.f);
		Colour3f text_colour; // toLinearSRGB(Colour3f(0.1f)

		Colour3f toggled_background_colour;
		Colour3f toggled_text_colour;

		Colour3f mouseover_background_colour;
		Colour3f mouseover_text_colour;

		float z;
	};

	GLUITextButton(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& button_text, const Vec2f& botleft, const CreateArgs& args);
	~GLUITextButton();

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void doHandleMouseMoved(MouseEvent& event) override;
	virtual void updateGLTransform() override; // Called when e.g. the viewport changes size

	void rebuild();

	virtual void setPos(const Vec2f& botleft) override;

	void setZ(float new_z) override;

	virtual void setPosAndDims(const Vec2f& botleft, const Vec2f& dims) override;
	virtual void setClipRegion(const Rect2f& clip_rect) override;

	bool isToggled() const { return toggled; }
	void setToggled(bool toggled_);

	virtual void setVisible(bool visible) override;
	virtual bool isVisible() override;

	GLUICallbackHandler* handler;
private:
	GLARE_DISABLE_COPY(GLUITextButton);

	GLUI* glui;
	Reference<OpenGLEngine> opengl_engine;

	GLUITextViewRef text_view;

	CreateArgs args;
	Vec2f m_botleft;
	std::string button_text;

	bool toggled;
};


typedef Reference<GLUITextButton> GLUITextButtonRef;
