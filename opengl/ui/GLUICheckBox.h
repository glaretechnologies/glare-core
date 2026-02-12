/*=====================================================================
GLUICheckBox.h
--------------
Copyright Glare Technologies Limited 2026 -
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
GLUICheckBox
------------

=====================================================================*/
class GLUICheckBox : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();

		std::string tooltip;

		Colour3f tick_colour;
		Colour3f box_colour;
		Colour3f mouseover_box_colour;

		Colour3f pressed_colour;

		bool checked; // Initially checked?  False by default.
	};

	GLUICheckBox(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& tick_texture_path, const Vec2f& botleft, const Vec2f& dims, const CreateArgs& args);
	~GLUICheckBox();

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void handleMouseDoubleClick(MouseEvent& event) override;
	virtual void handleMouseRelease(MouseEvent& event) override;
	virtual void doHandleMouseMoved(MouseEvent& event) override;

	void setDims(const Vec2f& dims);
	virtual void setPosAndDims(const Vec2f& botleft, const Vec2f& dims) override;

	virtual void setClipRegion(const Rect2f& clip_rect) override;

	void setChecked(bool checked_);
	bool isChecked() const { return checked; }

	virtual void setVisible(bool visible) override;
	virtual bool isVisible() override;

	GLUICallbackHandler* handler;

	bool checked;
	bool pressed;
	bool immutable_dims; // If true, don't change dimensions in setPosAndDims (which is called by GridContainer layout)

private:
	GLARE_DISABLE_COPY(GLUICheckBox);

	void updateColour(const Vec2f mouse_ui_coords);
	void updateTransforms();

	GLUI* glui;
	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef tick_overlay_ob;
	OverlayObjectRef box_overlay_ob;

	CreateArgs args;
};


typedef Reference<GLUICheckBox> GLUICheckBoxRef;
