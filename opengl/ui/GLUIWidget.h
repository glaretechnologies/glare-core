/*=====================================================================
GLUIWidget.h
------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../OpenGLEngine.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/vec2.h"
#include "../maths/Rect2.h"
#include <string>


class GLUI;
class MouseWheelEvent;
class KeyEvent;
class MouseEvent;
class TextInputEvent;


/*=====================================================================
GLUIWidget
----------

=====================================================================*/
class GLUIWidget : public RefCounted
{
public:
	GLUIWidget();
	virtual ~GLUIWidget();

	void handleMouseMoved(MouseEvent& mouse_event);
	virtual void handleMousePress(MouseEvent& /*event*/) {}
	virtual void handleMouseRelease(MouseEvent& /*event*/) {}
	virtual void handleMouseDoubleClick(MouseEvent& /*event*/) {}
	void handleMouseWheelEvent(MouseWheelEvent& event);
	void handleKeyPressedEvent(KeyEvent& key_event);
	void handleTextInputEvent(TextInputEvent& text_input_event);
	virtual void handleLosingKeyboardFocus() {}

	virtual void handleCutEvent(std::string& /*clipboard_contents_out*/) {}
	virtual void handleCopyEvent(std::string& /*clipboard_contents_out*/) {}

	virtual void doHandleMouseMoved(MouseEvent& /*mouse_event*/) {}
	virtual void doHandleMouseWheelEvent(MouseWheelEvent& /*event*/) {}
	virtual void doHandleKeyPressedEvent(KeyEvent& /*key_event*/) {}
	virtual void doHandleTextInputEvent(TextInputEvent& /*text_input_event*/) {}

	virtual bool isVisible() = 0;
	virtual void setVisible(bool visible) = 0;

	// Return the natural or minimum dimensions of the widget.
	// Default implementation:
	// For fixed size widgets, returns the fixed size.
	// For expanding widgets, returns zero.
	virtual Vec2f getMinDims() const;

	virtual void setPos(const Vec2f& botleft) = 0;

	virtual void setAvailableRegionDims(const Vec2f& /*available_dims*/) {} // Expanding widgets can resize to fill any of the available space.

	virtual void setClipRegion(const Rect2f& clip_rect) = 0; // clip_rect is in UI coords

	virtual void viewportResized(); // Called when e.g. the viewport changes size

	virtual void recomputeLayout() {} // For containers - call recursively on contained widgets and then place each contained widget at final location.

	virtual void think(GLUI& /*glui*/) {}

	virtual bool acceptsTextInput() { return false; } // If true, the mouse cursor will change to an I shape when the cursor is over the widget.

	virtual void containedWidgetChangedSize() {} // For containers - a widget in the container has changed size (e.g. group box collapsed or expanded), so a relayout is probably needed.

	virtual std::string className() const { return "unknown"; }

	inline Rect2f getRect() const { return rect; }
	inline Vec2f getDims() const { return rect.getWidths(); }
	inline float getZ() const { return m_z; }
	
	virtual void setZ(float new_z) { m_z = new_z; } // Subclasses should override this if they need to set z explicitly on anything.

	void setFixedWidthPx (float x_px);
	void setFixedHeightPx(float y_px);
	void setFixedDimsPx(const Vec2f& dims_px);
	void setFixedDimsUICoords(const Vec2f& dims_px) { sizing_type_x = SizingType_FixedSizeUICoords; sizing_type_y = SizingType_FixedSizeUICoords; fixed_size = dims_px; }

	// Returns old_dims or recomputes new dims if sizing type is SizingType_FixedSizeUICoords or SizingType_FixedSizePx
	Vec2f computeDims(const Vec2f& old_dims) const;

	void setParent(GLUIWidget* parent_) { m_parent = parent_; }
	GLUIWidget* getParent() const { return m_parent; }

	void setGLUI(GLUI* new_glui) { glui = new_glui; } // Set on unremoved widgets when gl_ui is about to be destroyed, so that glui is not a dangling pointer to gl_ui.
	void setOpenGLEngine(OpenGLEngine* new_opengl_engine) { opengl_engine = new_opengl_engine; } // Set on unremoved widgets when gl_ui is about to be destroyed, so we don't have a dangling pointer to opengl_engine.

	void setTooltip(const std::string& new_tooltip) { tooltip = new_tooltip; }

	std::string client_data;

	std::string tooltip;

	Rect2f rect;
	float m_z; // z=-1 is the near clip plane, z=1 is the far clip plane.


	// NOTE: TextView widgets are SizingType_FixedSizePx by default.
	enum SizingType
	{
		SizingType_FixedSizeUICoords,
		SizingType_FixedSizePx,
		SizingType_Expanding
	};
	SizingType sizing_type_x;
	SizingType sizing_type_y;

	Vec2f fixed_size; // x component used if sizing_type_x == SizingType_FixedSizePx, likewise for y component.

	std::string debug_name;
private:
	GLARE_DISABLE_COPY(GLUIWidget);

protected:
	GLUIWidget* m_parent; // so containers can be re-laid out when a widget size changes, via containedWidgetChangedSize() calls to parents.

	GLUI* glui;
	OpenGLEngine* opengl_engine;
};


typedef Reference<GLUIWidget> GLUIWidgetRef;
