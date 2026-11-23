/*=====================================================================
GLUIDropDownList.h
------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUICallbackHandler.h"
#include "GLUILineEdit.h"
#include "GLUIGridContainer.h"
#include "GLUITextView.h"
#include "../../graphics/colour3.h"


struct OverlayObject;
class GLUI;


/*=====================================================================
GLUIDropDownList
----------------

=====================================================================*/
class GLUIDropDownList final : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();

		std::vector<std::string> options;
		std::string initial_value;

		int font_size_px; // Default 14

		GLUIWidget::SizingType sizing_type_x;
		GLUIWidget::SizingType sizing_type_y;
		Vec2f fixed_size; // In device-independent pixels. Used when sizing_type_x/y == SizingType_FixedSizePx.

		float z;

		std::string tooltip;

		Colour3f background_colour;
		Colour3f mouseover_background_colour;
		Colour3f text_colour;
	};

	GLUIDropDownList(GLUI& glui, const CreateArgs& args);
	~GLUIDropDownList();

	std::string getCurrentValue() const { return cur_index < args.options.size() ? args.options[cur_index] : std::string(); }
	const std::vector<std::string>& getOptions() const { return args.options; }
	void setValue(const std::string& new_val);        // fires handler.
	void setValueNoEvent(const std::string& new_val); // no handler call.  Does nothing if new_val is not one of the options.

	virtual void handleMousePress(GLUIMouseEvent& event) override;
	virtual void handleMouseRelease(GLUIMouseEvent& event) override;
	virtual void handleMouseDoubleClick(GLUIMouseEvent& event) override;
	virtual void doHandleMouseMoved(GLUIMouseEvent& event) override;
	virtual void doHandleMouseWheelEvent(GLUIMouseWheelEvent& event) override;
	virtual void doHandleKeyPressedEvent(KeyEvent& key_event) override;

	virtual void handleLosingKeyboardFocus() override;

	virtual Vec2f getMinDims() const override; // Return the natural or minimum dimensions of the widget.

	virtual void setPos(const Vec2f& botleft) override;
	virtual void setClipRegion(const Rect2f& clip_rect) override;
	virtual void setZ(float new_z) override;
	virtual void setVisible(bool visible) override;
	virtual bool isVisible() override;
	virtual void viewportResized() override;

	virtual std::string className() const override { return "GLUIDropDownList"; }

	std::function<void(GLUIDropDownListValueChangedEvent&)> handler_func;

private:
	GLARE_DISABLE_COPY(GLUIDropDownList);

	void updateWidgetTransforms();
	void updateButtonColours(const Vec2f& mouse_ui_coords);
	void openButtonPressed();
	void optionButtonPressed(GLUICallbackEvent& event);
	GLUIWidget* getOptionButton(size_t option_index);
	void selectOption(size_t option_index);
	void setNewCurrentIndex(size_t new_cur_index);

	Reference<OverlayObject> open_button_overlay_ob;
	GLUITextViewRef text;
	//GLUILineEditRef text;
	GLUIGridContainerRef options_grid;

	CreateArgs args;
	size_t cur_index;
};


typedef Reference<GLUIDropDownList> GLUIDropDownListRef;
