/*=====================================================================
GLUILineEdit.h
--------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUIText.h"
#include "../../graphics/colour3.h"


/*=====================================================================
GLUILineEdit
------------
A single-line text edit field.
=====================================================================*/
class GLUILineEdit final : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();

		GLUIWidget::SizingType sizing_type_x;
		GLUIWidget::SizingType sizing_type_y;
		Vec2f fixed_size; // x component used if sizing_type_x == SizingType_FixedSizePx, likewise for y component.

		std::string tooltip;
		Colour3f background_colour; // Linear
		Colour3f mouseover_background_colour;
		float background_alpha;
		Colour3f text_colour; // Linear
		float text_alpha;
		int padding_px; // default = 10
		int font_size_px; // default = 14
		float rounded_corner_radius_px;
		float z;
	};

	GLUILineEdit(GLUI& glui, const Vec2f& botleft, const CreateArgs& args);
	~GLUILineEdit(); // Removed overlay objects from opengl engine.

	virtual void think(GLUI& glui) override;

	void setText(const std::string& new_text);
	const std::string& getText() const;

	void clear();

	virtual void setVisible(bool visible) override;
	virtual bool isVisible() override;

	virtual Vec2f getMinDims() const override; // Return the natural or minimum dimensions of the widget.

	virtual void setPos(const Vec2f& botleft) override;

	virtual void setAvailableRegionDims(const Vec2f& available_dims) override; // Expanding widgets can resize to fill any of the available space.

	virtual void setClipRegion(const Rect2f& clip_rect) override;

	virtual void setZ(float new_z) override;

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void handleMouseRelease(MouseEvent& event) override;
	virtual void handleMouseDoubleClick(MouseEvent& event) override;
	virtual void doHandleMouseMoved(MouseEvent& event) override;
	virtual void doHandleKeyPressedEvent(KeyEvent& key_event) override;

	virtual void doHandleTextInputEvent(TextInputEvent& text_input_event) override;
	virtual void handleLosingKeyboardFocus() override;

	virtual void handleCutEvent(std::string& clipboard_contents_out) override;
	virtual void handleCopyEvent(std::string& clipboard_contents_out) override;

	virtual void viewportResized() override;

	virtual bool acceptsTextInput() override { return true; }

	virtual std::string className() const override { return "GLUILineEdit"; }

	std::function<void()> on_enter_pressed;
	std::function<void()> on_losing_keyboard_focus;
	std::function<void()> on_text_changed;

private:
	GLARE_DISABLE_COPY(GLUILineEdit);

	void updateOverlayObTransforms();
	void recreateTextWidget();
	void updateTextTransform();
	void deleteSelectedTextAndClearSelection();

	OverlayObjectRef background_overlay_ob;
	Vec2i last_viewport_dims;

	OverlayObjectRef cursor_overlay_ob;
	OverlayObjectRef selection_overlay_ob;

	GLUITextRef glui_text;

	CreateArgs args;
	std::string text;
	int cursor_pos; // Index of unicode character/code point that cursor is to the left of.

	int selection_start; // cursor position of selection start, or -1 if no selection.
	int selection_end; // cursor position of selection end.  May be < selection_start

	Vec2f botleft; // in GL UI coords

	float height_px;

	double last_cursor_update_time;
};


typedef Reference<GLUILineEdit> GLUILineEditRef;
