/*=====================================================================
GLUI.h
------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "FontCharTexCache.h"
#include "../OpenGLEngine.h"
#include "../graphics/TextRenderer.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <string>
#include <set>

#include "C:/programming\substrata\trunk\gui_client\UIEvents.h" // TEMP

class GLUIMouseWheelEvent;


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
	void create(Reference<OpenGLEngine>& opengl_engine, float device_pixel_ratio, const std::vector<TextRendererFontFaceRef>& fonts, const std::vector<TextRendererFontFaceRef>& emoji_fonts);

	void destroy();

	Vec2f UICoordsForWindowPixelCoords(const Vec2f& pixel_coords);
	Vec2f UICoordsForOpenGLCoords(const Vec2f& gl_coords);

	bool handleMouseClick(const Vec2f& gl_coords); // Returns true if event accepted (e.g. should not be passed on)
	bool handleMouseWheelEvent(const Vec2f& gl_coords, const GLUIMouseWheelEvent& event);
	bool handleMouseMoved(const Vec2f& gl_coords);
	void handleKeyPressedEvent(const KeyEvent& key_event);

	void viewportResized(int w, int h);

	// To get click events:
	void addWidget(const GLUIWidgetRef& widget) { widgets.insert(widget); }
	void removeWidget(const GLUIWidgetRef& widget) { widgets.erase(widget); }


	static float getViewportMinMaxY(Reference<OpenGLEngine>& opengl_engine);

	float getUIWidthForDevIndepPixelWidth(float pixel_w);

	OpenGLTextureRef makeToolTipTexture(const std::string& text);


	TextRendererFontFace* getBestMatchingFont(int font_size_px, bool emoji);

	void setKeyboardFocusWidget(GLUIWidgetRef widget) { key_focus_widget = widget; }

	Reference<OpenGLEngine> opengl_engine;
	std::vector<TextRendererFontFaceRef> fonts; // Should be in ascending font size order
	std::vector<TextRendererFontFaceRef> emoji_fonts; // Should be in ascending font size order

	FontCharTexCacheRef font_char_text_cache;

private:
	GLARE_DISABLE_COPY(GLUI);

	std::set<GLUIWidgetRef> widgets;

	GLUIWidgetRef key_focus_widget;

	OverlayObjectRef tooltip_overlay_ob;

	std::map<std::string, OpenGLTextureRef> tooltip_textures;

	float device_pixel_ratio;
};


typedef Reference<GLUI> GLUIRef;


class GLUIMouseWheelEvent
{
public:
	int angle_delta_y;
};
