/*=====================================================================
GLUISpinBox.h
-------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUICallbackHandler.h"
#include "GLUILineEdit.h"
#include "../OpenGLEngine.h"
#include "../../utils/RefCounted.h"
#include "../../utils/Reference.h"
#include "../../maths/Rect2.h"
#include "../../graphics/colour3.h"
#include <string>


class GLUI;


/*=====================================================================
GLUISpinBox
-----------
A numeric spin box that holds a double value.

Layout:
  [ value text ][ ↑ ]
                [ ↓ ]

The up/down arrow buttons are stacked on the right side of the value field.
Clicking the ↓ button decrements and ↑ increments the value by step.
Mouse wheel over the widget also changes the value.
=====================================================================*/
class GLUISpinBox final : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();

		double min_value;
		double max_value;
		double initial_value;
		double step;

		int num_decimal_places; // Number of decimal places shown in the value text. Default 3.

		int font_size_px; // Default 14

		GLUIWidget::SizingType sizing_type_x;
		GLUIWidget::SizingType sizing_type_y;
		Vec2f fixed_size; // In device-independent pixels. Used when sizing_type_x/y == SizingType_FixedSizePx.

		std::string tooltip;

		Colour3f background_colour;
		Colour3f button_colour;
		Colour3f mouseover_button_colour;
		Colour3f pressed_button_colour;
		Colour3f text_colour;
	};

	GLUISpinBox(GLUI& glui, const CreateArgs& args);
	~GLUISpinBox();

	double getValue() const { return cur_value; }
	void setValue(double new_val);        // Clamps, updates display, fires handler.
	void setValueNoEvent(double new_val); // Clamps and updates display, no handler call.

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void handleMouseRelease(MouseEvent& event) override;
	virtual void handleMouseDoubleClick(MouseEvent& event) override;
	virtual void doHandleMouseMoved(MouseEvent& event) override;
	virtual void doHandleMouseWheelEvent(MouseWheelEvent& event) override;
	virtual void handleLosingKeyboardFocus() override;

	virtual void setPos(const Vec2f& botleft) override;
	virtual void setClipRegion(const Rect2f& clip_rect) override;
	virtual void setZ(float new_z) override;
	virtual void setVisible(bool visible) override;
	virtual bool isVisible() override;
	virtual void viewportResized() override;

	virtual std::string className() const override { return "GLUISpinBox"; }

	std::function<void(GLUISpinBoxValueChangedEvent&)> handler_func;
private:
	GLARE_DISABLE_COPY(GLUISpinBox);

	void updateOverlayTransforms();
	void updateValueText();
	void changeValue(double delta);
	Rect2f decrementButtonRect() const;
	Rect2f incrementButtonRect() const;
	void updateButtonColours(const Vec2f& mouse_ui_coords);

	OverlayObjectRef background_ob;
	OverlayObjectRef decrement_ob;
	OverlayObjectRef increment_ob;
	GLUILineEditRef line_edit;

	CreateArgs args;
	double cur_value;

	bool decrement_pressed;
	bool increment_pressed;
};


typedef Reference<GLUISpinBox> GLUISpinBoxRef;
