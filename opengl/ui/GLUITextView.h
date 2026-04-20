/*=====================================================================
GLUITextView.h
--------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUIText.h"
#include "../graphics/colour3.h"


class GLUI;


/*=====================================================================
GLUITextView
------------
A widget that displays some text.
Also can draw a background behind the text.
=====================================================================*/
class GLUITextView final : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();

		GLUIWidget::SizingType sizing_type_x; // Will be SizingType_FixedSizePx by default, but fixed_size will be max(fixed_size, computeTextDimsWithPadding())
		GLUIWidget::SizingType sizing_type_y;
		Vec2f fixed_size; // x component used if sizing_type_x == SizingType_FixedSizePx, likewise for y component.

		std::string tooltip;
		Colour3f background_colour; // Linear
		float background_alpha;
		float background_corner_radius_px; // Background uses rounded corners if radius > 0.
		Colour3f text_colour; // Linear
		float text_alpha;
		int padding_px; // Padding around text for background box.  default = 10 pixels.
		int font_size_px; // default = 14 pixels.

		float line_0_x_offset; // X offset of line 0 from botleft. [0, max_width].  Subsequent lines don't have this offset.
		float max_width; // Max width in UI coords.  Text will word-wrap to not be wider than this.

		bool text_selectable; // True by default

		float z;
	};

	GLUITextView(GLUI& glui, const std::string& text, const Vec2f& botleft, const CreateArgs& args);
	~GLUITextView();
	
	void setText(const string_view new_text);

	void setTextColour(const Colour3f& col);
	void setTextAlpha(float alpha);
	void setBackgroundColour(const Colour3f& col);
	void setBackgroundAlpha(float alpha);

	virtual Vec2f getMinDims() const override; // Return the natural or minimum dimensions of the widget.

	virtual void setPos(const Vec2f& botleft) override; // OLD: Sets baseline position of text on first line.  Text descenders will be below this position.  Background quad can extend past this.

	virtual void setAvailableRegionDims(const Vec2f& available_dims) override; // Expanding widgets can resize to fill any of the available space.

	virtual void setZ(float new_z) override;

	float getOffsetToTopLine() const; // For multi-line text, the top line will be some vertical offset above the bottom line.  Compute this offset and return it.

	float getPaddingWidth() const;

	virtual void setClipRegion(const Rect2f& rect) override;

	virtual void setVisible(bool visible) override;
	virtual bool isVisible() override;

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void handleMouseRelease(MouseEvent& event) override;
	virtual void handleMouseDoubleClick(MouseEvent& event) override;
	virtual void doHandleMouseMoved(MouseEvent& event) override;
	virtual void handleLosingKeyboardFocus() override;
	virtual void viewportResized() override;

	virtual void handleCutEvent(std::string& clipboard_contents_out) override;
	virtual void handleCopyEvent(std::string& clipboard_contents_out) override;

	virtual std::string className() const override { return "GLUITextView"; }

private:
	GLARE_DISABLE_COPY(GLUITextView);

	void updateOverlayObTransforms();
	Rect2f recomputeRect() const;
	Vec2f computeTextDimsWithPadding() const;

	OverlayObjectRef background_overlay_ob;
	std::vector<GLUITextRef> glui_texts; // Lines of text.  glui_texts[0] is the top line.

	OverlayObjectRef selection_overlay_ob;

	int selection_start, selection_end;
	size_t selection_start_text_index, selection_end_text_index; // TODO: Do multi-line select

	CreateArgs args;
	std::string text;
	bool visible;

	Vec2f last_rounded_background_dims;

	Rect2f clip_rect;
};


typedef Reference<GLUITextView> GLUITextViewRef;
