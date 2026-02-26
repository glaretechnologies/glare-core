/*=====================================================================
GLUI.h
------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "FontCharTexCache.h"
#include "../OpenGLEngine.h"
#include "graphics/TextRenderer.h"
#include "utils/RefCounted.h"
#include "utils/Reference.h"
#include "ui/UIEvents.h"
#include <string>
#include <set>


class GLUIMouseWheelEvent;
class GLUICallbacks;
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
	Vec2f OpenGLCoordsForUICoords(const Vec2f& ui_coords);
	Rect2f OpenGLRectCoordsForUICoords(const Rect2f& ui_coords);
	float OpenGLYScaleForUIYScale(float y_scale);

	void handleMousePress(MouseEvent& event);
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
	void removeWidget(const GLUIWidgetRef& widget) { widgets.erase(widget); }


	float getViewportMinMaxY();
	float getYScale();

	float getUIWidthForDevIndepPixelWidth(float pixel_w);
	Vec2f getUIWidthForDevIndepPixelWidths(Vec2f pixel_w);
	float getDevIndepPixelWidthForUIWidth(float ui_width);

	OpenGLTextureRef makeToolTipTexture(const std::string& text);
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

	void setCurrentDevicePixelRatio(float new_device_pixel_ratio);
	float getDevicePixelRatio() const { return device_pixel_ratio; }


	Reference<OpenGLEngine> opengl_engine;
	

	GLUICallbacks* callbacks;

	FontCharTexCacheRef font_char_text_cache;

	glare::StackAllocator* stack_allocator;
private:
	GLARE_DISABLE_COPY(GLUI);

	TextRendererFontFaceSizeSetRef fonts;
	TextRendererFontFaceSizeSetRef emoji_fonts;

	std::set<GLUIWidgetRef> widgets;

	GLUIWidgetRef key_focus_widget;

	OverlayObjectRef tooltip_overlay_ob;

	std::map<std::string, OpenGLTextureRef> tooltip_textures;

	float device_pixel_ratio;
	float ui_scale;

	bool mouse_over_text_input_widget;

	Vec2f last_mouse_ui_coords;

	std::vector<GLUIWidget*> temp_widgets;
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
inline void checkRemoveAndDeleteWidget(GLUIRef gl_ui, WidgetTypeRef& widget)
{
	if(widget)
	{
		gl_ui->removeWidget(widget);
		widget = nullptr;
	}
}
