/*=====================================================================
GLUI.h
------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "../OpenGLEngine.h"
#include "../graphics/TextRenderer.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <string>
#include <set>


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
	void create(Reference<OpenGLEngine>& opengl_engine, float device_pixel_ratio, TextRendererFontFaceRef text_renderer_font);

	void destroy();

	Vec2f UICoordsForWindowPixelCoords(const Vec2f& pixel_coords);
	Vec2f UICoordsForOpenGLCoords(const Vec2f& gl_coords);

	bool handleMouseClick(const Vec2f& gl_coords); // Returns true if event accepted (e.g. should not be passed on)
	bool handleMouseWheelEvent(const Vec2f& gl_coords, const GLUIMouseWheelEvent& event);
	bool handleMouseMoved(const Vec2f& gl_coords);
	void viewportResized(int w, int h);

	// To get click events:
	void addWidget(const GLUIWidgetRef& widget) { widgets.insert(widget); }
	void removeWidget(const GLUIWidgetRef& widget) { widgets.erase(widget); }


	static float getViewportMinMaxY(Reference<OpenGLEngine>& opengl_engine);

	float getUIWidthForDevIndepPixelWidth(float pixel_w);

	OpenGLTextureRef makeToolTipTexture(const std::string& text);


	Reference<OpenGLEngine> opengl_engine;
	TextRendererFontFaceRef text_renderer_font;

private:
	GLARE_DISABLE_COPY(GLUI);

	std::set<GLUIWidgetRef> widgets;

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
