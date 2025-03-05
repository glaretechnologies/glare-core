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
	float OpenGLYScaleForUIYScale(float y_scale);

	void handleMousePress(MouseEvent& event);
	void handleMouseRelease(MouseEvent& event);
	void handleMouseDoubleClick(MouseEvent& event);
	bool handleMouseWheelEvent(const Vec2f& gl_coords, const GLUIMouseWheelEvent& event);
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

	float getUIWidthForDevIndepPixelWidth(float pixel_w);
	float getDevIndepPixelWIdthForUIWidth(float ui_width);

	OpenGLTextureRef makeToolTipTexture(const std::string& text);


	TextRendererFontFace* getFont(int font_size_px, bool emoji);

	TextRendererFontFaceSizeSet* getFonts() { return fonts.ptr(); }
	TextRendererFontFaceSizeSet* getEmojiFonts() { return emoji_fonts.ptr(); }

	GLUIWidgetRef getKeyboardFocusWidget() const { return key_focus_widget; }
	void setKeyboardFocusWidget(GLUIWidgetRef widget);


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

	bool mouse_over_text_input_widget;
};


typedef Reference<GLUI> GLUIRef;


class GLUIMouseWheelEvent
{
public:
	int angle_delta_y;
};


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
