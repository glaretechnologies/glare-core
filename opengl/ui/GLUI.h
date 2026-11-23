/*=====================================================================
GLUI.h
------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUIEvents.h"
#include "FontCharTexCache.h"
#include "graphics/TextRenderer.h"
#include "utils/Timer.h"
#include "ui/UIEvents.h"
#include <string>
#include <set>


class OpenGLEngine;
struct OverlayObject;
class GLUICallbacks;
class GLUITextView;
class GLUIText;
namespace glare { class StackAllocator; }


/*=====================================================================
GLUI
----
UI coords are x increasing to right, y increasing up the window.
x=-1 = left of window, x = +1 = right of window.

For a square window, y=-1 = bottom of window, y =+1 = top of window.
For a non-square window, y will be != 1 at top of window, rather
y = window height / window width (unlike OpenGL coords).

z=-1 is the near clip plane, z=1 is the far clip plane.
(from Overlay object coords in OpenGLEngine)
=====================================================================*/
class GLUI : public RefCounted
{
public:
	GLUI();
	~GLUI();

	// device_pixel_ratio is basically a scale factor for sizes in pixels.
	void create(Reference<OpenGLEngine>& opengl_engine, float device_pixel_ratio, 
		const TextRendererFontFaceSizeSetRef& fonts, const TextRendererFontFaceSizeSetRef& emoji_fonts,
		glare::StackAllocator* stack_allocator);

	void destroy();

	void think();

	Vec2f UICoordsForWindowPixelCoords(const Vec2f& pixel_coords);
	Vec2f UICoordsForOpenGLCoords(const Vec2f& gl_coords);
	float UIYCoordForOpenGLYCoord(float y_gl);
	Vec2f OpenGLCoordsForUICoords(const Vec2f& ui_coords);
	Rect2f OpenGLRectCoordsForUICoords(const Rect2f& ui_coords);
	float OpenGLYScaleForUIYScale(float y_scale);

	void handleMousePress(MouseEvent& event);
	void cancelMousePress(); // Cancels any current presses: calls handleMousePressCancelled() on the widgets that accepted them, which then don't get the releases.
	void handleMouseRelease(MouseEvent& event);
	void handleMouseDoubleClick(MouseEvent& event);
	bool handleMouseWheelEvent(MouseWheelEvent& event);
	bool handleMouseMoved(MouseEvent& mouse_event);
	void handleKeyPressedEvent(KeyEvent& key_event);
	void handleTextInputEvent(TextInputEvent& text_input_event);

	void handleCutEvent(std::string& clipboard_contents_out);
	void handleCopyEvent(std::string& clipboard_contents_out);

	void viewportResized(int w, int h);

	// To get click events:
	void addWidget(const GLUIWidgetRef& widget) { widgets.insert(widget); }
	void removeWidget(const GLUIWidgetRef& widget);

	void removeAllWidgets();

	Vec2f viewportTopRight() { return Vec2f(1.f, getViewportMinMaxY()); } // Return viewport top right in UI coords.
	float getViewportMinMaxY();
	float getYScale();

	float getUIWidthForDevIndepPixelWidth(float pixel_w);
	Vec2f getUIWidthForDevIndepPixelWidths(Vec2f pixel_w);
	float getDevIndepPixelWidthForUIWidth(float ui_width);

	void hideTooltip();
	void unhideTooltip();

	TextRendererFontFace* getFont(int font_size_px, bool emoji);

	TextRendererFontFaceSizeSet* getFonts() { return fonts.ptr(); }
	TextRendererFontFaceSizeSet* getEmojiFonts() { return emoji_fonts.ptr(); }

	GLUIWidgetRef getKeyboardFocusWidget() const { return key_focus_widget; }
	void setKeyboardFocusWidget(GLUIWidgetRef widget);


	Vec2f getLastMouseUICoords() const { return last_mouse_ui_coords; }

	void setUIScale(float ui_scale_) { ui_scale = ui_scale_; }
	float getUIScale() const { return ui_scale; }

	// Drawable pixels per 'device-independent' pixels
	void setCurrentDevicePixelRatio(float new_device_pixel_ratio);
	float getDevicePixelRatio() const { return device_pixel_ratio; }

	void setDebugOverlayEnabled(bool enabled);

	static int getDefaultFontSizePx() { return default_font_size_px; }


	Reference<OpenGLEngine> opengl_engine;
	

	GLUICallbacks* callbacks;

	FontCharTexCacheRef font_char_text_cache;

	glare::StackAllocator* stack_allocator;
private:
	GLARE_DISABLE_COPY(GLUI);

	void removeKeyboardFocusForWidgetBeingRemoved();
	void updateTooltip();

	TextRendererFontFaceSizeSetRef fonts;
	TextRendererFontFaceSizeSetRef emoji_fonts;

	std::set<GLUIWidgetRef> widgets;

	GLUIWidgetRef key_focus_widget;

	// For each mouse button, the widget that accepted the last press or double-click of that button, which receives the following release of the button.  May be null.
	// Indexed by mouseButtonIndex(button).  Tracked per button, so that e.g. a widget dragged with the left button and then also pressed with the middle button gets both releases.
	static const int NUM_MOUSE_BUTTONS = 5;
	GLUIWidgetRef pressed_widgets[NUM_MOUSE_BUTTONS];
	void setPressedWidgetForMouseButton(MouseButton button, const GLUIWidgetRef& widget);

	float device_pixel_ratio; // Drawable pixels per 'device-independent' pixels.
	float ui_scale;

	bool mouse_over_text_input_widget;

	Vec2f last_mouse_ui_coords;
	Vec2f last_mouse_gl_coords;


	Reference<GLUIText> tooltip_text_ob; // The tooltip text graphics.
	Reference<OverlayObject> tooltip_overlay_ob; // Draws the solid background quad behind the tooltip text.
	std::string hovered_tooltip; // Tooltip of the widget the mouse is over, or empty if none.
	const GLUIWidget* hovered_tooltip_widget; // The widget with hovered_tooltip, or null.  Only compared with, never dereferenced, and cleared when the widget is removed.
	Timer hovered_tooltip_timer; // Time since the mouse moved onto hovered_tooltip_widget.
	bool tooltip_shown; // Is the tooltip currently being drawn?  Set by updateTooltip().
	bool tooltip_suppressed_by_press; // Set when a mouse button is pressed, until the mouse moves onto a different widget with a tooltip, or off all widgets with tooltips.  (Unlike tooltip_hidden, which hideTooltip() sets until unhideTooltip() is called.)
	bool tooltip_hidden; // Set by hideTooltip() and unhideTooltip(), used when the context menu pops up.

	std::vector<GLUIWidgetRef> temp_widgets;

	Reference<GLUITextView> debug_overlay;

	static int default_font_size_px;
};


typedef Reference<GLUI> GLUIRef;


class GLUICallbacks
{
public:
	virtual void startTextInput() = 0;
	virtual void stopTextInput() = 0;

	enum MouseCursor
	{
		MouseCursor_Arrow,
		MouseCursor_IBeam
	};
	virtual void setMouseCursor(MouseCursor cursor) = 0;
};


// Removes a widget from the GLUI if the widget reference is non-null.
// Sets widget reference to null also.
template <class WidgetTypeRef>
inline void checkRemoveAndDeleteWidget(GLUIRef& gl_ui, WidgetTypeRef& widget)
{
	if(widget)
	{
		if(gl_ui)
			gl_ui->removeWidget(widget);
		widget = nullptr;
	}
}

// Removes a widget from the GLUI if the widget reference is non-null.
// Sets widget reference to null also.
template <class WidgetTypeRef>
inline void checkRemoveAndDeleteWidget(GLUI* gl_ui, WidgetTypeRef& widget)
{
	if(widget)
	{
		if(gl_ui)
			gl_ui->removeWidget(widget);
		widget = nullptr;
	}
}
