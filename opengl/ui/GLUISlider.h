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
class GLUISlider final : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();

		GLUIWidget::SizingType sizing_type_x;
		GLUIWidget::SizingType sizing_type_y;
		Vec2f fixed_size; // x component used if sizing_type_x == SizingType_FixedSizePx, likewise for y component.

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

	GLUISlider(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const CreateArgs& args);
	~GLUISlider();

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void handleMouseRelease(MouseEvent& event) override;
	virtual void handleMouseDoubleClick(MouseEvent& event) override;
	virtual void doHandleMouseMoved(MouseEvent& event) override;
	virtual void doHandleMouseWheelEvent(MouseWheelEvent& event) override;

	double getValue() const { return cur_value; }
	void setValue(double new_val); // Set value and emit a value changed event.
	void setValueNoEvent(double new_val); // Set value and don't emit a value changed event.

	// Called when e.g. the viewport changes size
	virtual void updateGLTransform() override;

	virtual void setPos(const Vec2f& botleft) override;
	virtual void setPosAndDims(const Vec2f& botleft, const Vec2f& dims) override;
	virtual void setClipRegion(const Rect2f& clip_rect) override;

	virtual void setZ(float new_z) override;

	virtual void setVisible(bool visible) override;
	virtual bool isVisible() override;

	GLUICallbackHandler* handler;

private:
	GLARE_DISABLE_COPY(GLUISlider);
	void updateOverlayTransforms();
	void updateKnobColour(const Vec2f mouse_ui_coords);
	Rect2f computeKnobRect();
	void handleClickOnTrack(const Vec2f mouse_ui_coords);

	GLUI* glui;
	Reference<OpenGLEngine> opengl_engine;
	
	OverlayObjectRef track_ob;
	OverlayObjectRef knob_ob;

	CreateArgs args;

	double cur_value;
	bool knob_grabbed;
	float knob_grab_x; // UI x coord of the mouse pointer when knob grabbed.
	double knob_grab_value; // cur_value when knob was grabbed
};


typedef Reference<GLUISlider> GLUISliderRef;
