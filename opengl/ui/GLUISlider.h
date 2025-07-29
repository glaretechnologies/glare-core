/*=====================================================================
GLUISlider.h
------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUICallbackHandler.h"


class GLUI;


/*=====================================================================
GLUISlider
----------
Horizontal slider control
=====================================================================*/
class GLUISlider : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();

		double min_value;
		double max_value;
		double initial_value;

		double scroll_speed;

		std::string tooltip;

		Colour3f knob_colour;
		Colour3f grabbed_knob_colour;
		Colour3f mouseover_knob_colour;
		Colour3f track_colour;
	};

	GLUISlider(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const Vec2f& botleft, const Vec2f& dims, const CreateArgs& args);
	~GLUISlider();

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void handleMouseRelease(MouseEvent& event) override;
	virtual void doHandleMouseMoved(MouseEvent& event) override;
	virtual void doHandleMouseWheelEvent(MouseWheelEvent& event) override;

	void setPosAndDims(const Vec2f& botleft, const Vec2f& dims);

	void setVisible(bool visible);
	virtual bool isVisible() override;

	GLUICallbackHandler* handler;

private:
	GLARE_DISABLE_COPY(GLUISlider);
	void updateKnobColour(const Vec2f mouse_ui_coords);
	Rect2f computeKnobRect();

	GLUI* glui;
	Reference<OpenGLEngine> opengl_engine;
	
	OverlayObjectRef track_ob;
	OverlayObjectRef knob_ob;

	CreateArgs args;

	Vec2f m_botleft;
	Vec2f m_dims;

	double cur_value;
	bool knob_grabbed;
	float knob_grab_x; // UI x coord of the mouse pointer when knob grabbed.
	double knob_grab_value; // cur_value when knob was grabbed
};


typedef Reference<GLUISlider> GLUISliderRef;
