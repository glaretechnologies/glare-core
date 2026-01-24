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
	struct CreateArgs
	{
		CreateArgs();

		std::string tooltip;

		// For toggleable buttons:
		Colour3f toggled_colour;
		Colour3f untoggled_colour;

		Colour3f mouseover_toggled_colour;
		Colour3f mouseover_untoggled_colour;

		// For non-toggleable buttons:
		Colour3f button_colour;
		Colour3f mouseover_button_colour;

		Colour3f pressed_colour;
	};

	GLUIButton(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& tex_path, const Vec2f& botleft, const Vec2f& dims, const CreateArgs& args);
	~GLUIButton();

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void handleMouseRelease(MouseEvent& event) override;
	virtual void doHandleMouseMoved(MouseEvent& event) override;

	void setPosAndDims(const Vec2f& botleft, const Vec2f& dims);

	virtual void setClipRegion(const Rect2f& clip_rect) override;

	void setToggled(bool toggled_);

	virtual void setVisible(bool visible) override;
	virtual bool isVisible() override;

	GLUICallbackHandler* handler;

	bool toggleable;
	bool toggled;
	bool pressed;

private:
	GLARE_DISABLE_COPY(GLUIButton);

	void updateButtonColour(const Vec2f mouse_ui_coords);

	GLUI* glui;
	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef overlay_ob;

	CreateArgs args;
};


typedef Reference<GLUIButton> GLUIButtonRef;
