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

		GLUIWidget::SizingType sizing_type_x;
		GLUIWidget::SizingType sizing_type_y;
		Vec2f fixed_size; // x component used if sizing_type_x == SizingType_FixedSizePx, likewise for y component.

		std::string tooltip;
		int font_size_px; // default = 14 pixels.

		Colour3f background_colour; // Colour3f(1.f);
		Colour3f text_colour; // toLinearSRGB(Colour3f(0.1f)

		Colour3f toggled_background_colour;
		Colour3f toggled_text_colour;

		Colour3f mouseover_background_colour;
		Colour3f mouseover_text_colour;

		Colour3f pressed_background_colour; // Used while the mouse button is held down after pressing on the button.
		Colour3f pressed_text_colour;

		float corner_radius_px; // Radius of the rounded corners of the background, outline and shadow.  Square corners if zero.

		float border_width_px; // Width of the outline around the button, which lies outside of the button rect.  No outline if zero.
		Colour3f border_colour;
		Colour3f mouseover_border_colour;

		// The drop shadow is drawn as shadow_num_layers translucent black shapes, each 1 pixel bigger than the previous,
		// which add up to a soft edge.  No shadow if shadow_num_layers is zero.
		int shadow_num_layers;
		float shadow_offset_px; // How far the shadow is offset downwards from the button.
		float shadow_layer_alpha; // Opacity of each shadow layer.

		float z;
	};

	GLUITextButton(GLUI& glui, const std::string& button_text, const CreateArgs& args);
	~GLUITextButton();

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void handleMouseRelease(MouseEvent& event) override;
	virtual void handleMouseDoubleClick(MouseEvent& event) override;
	virtual void doHandleMouseMoved(MouseEvent& event) override;
	virtual void viewportResized() override;

	void rebuild();

	void setText(const std::string& new_text); // The button size may change, so the containing layout should be updated afterwards.

	virtual Vec2f getMinDims() const override; // Return the natural or minimum dimensions of the widget.

	virtual void setPos(const Vec2f& botleft) override;

	virtual void setAvailableRegionDims(const Vec2f& available_dims) override;

	void setZ(float new_z) override;

	virtual void setClipRegion(const Rect2f& clip_rect) override;

	bool isToggled() const { return toggled; }
	void setToggled(bool toggled_);

	virtual void setVisible(bool visible) override;
	virtual bool isVisible() override;

	virtual std::string className() const override { return "GLUITextButton"; }

	GLUICallbackHandler* handler;
	std::function<void(GLUICallbackEvent&)> handler_func;
private:
	GLARE_DISABLE_COPY(GLUITextButton);

	void updateColours(bool mouse_in_rect);
	void makeTextView(bool visible);
	void addDecoration(const Colour3f& colour, float alpha, float expand_px, const Vec2f& offset_px, float z_offset);
	void updateDecorationTransforms();

	enum ColourState
	{
		ColourState_Normal,
		ColourState_MouseOver,
		ColourState_Toggled,
		ColourState_Pressed
	};

	GLUITextViewRef text_view;

	// A shape drawn behind the text view background, for the outline and drop shadow layers.
	struct Decoration
	{
		OverlayObjectRef ob;
		float expand_px; // How far the shape extends past the text view background on each side.
		Vec2f offset_px; // Offset of the shape from the text view background.  +y is up.
		float z_offset; // z relative to the button z.  The text view background is at +0.001, larger values are further back.
		Vec2f last_mesh_dims; // Dims of the current rounded-corner mesh, so it is only rebuilt when the dims change.
	};
	std::vector<Decoration> decorations;
	int border_decoration_index; // Index of the outline in decorations, or -1 if there is no outline.

	CreateArgs args;
	Vec2f m_botleft;
	std::string button_text;

	bool toggled;
	bool pressed; // True between a mouse press on the button and the following mouse release.
	ColourState colour_state; // Which set of colours the text view currently has.
};


typedef Reference<GLUITextButton> GLUITextButtonRef;
