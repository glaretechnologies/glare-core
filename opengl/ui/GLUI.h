/*=====================================================================
GLUI.h
------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "../OpenGLEngine.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include <string>
#include <set>


class GLUITextRendererCallback;


/*=====================================================================
GLUI
----

=====================================================================*/
class GLUI : public RefCounted
{
public:
	// preprocessor_defines are just inserted directly into the source after the first line.
	GLUI();
	~GLUI();

	void create(Reference<OpenGLEngine>& opengl_engine, GLUITextRendererCallback* text_renderer);

	void destroy();


	bool handleMouseClick(const Vec2f& gl_coords); // Returns true if event accepted (e.g. should not be passed on)
	bool handleMouseMoved(const Vec2f& gl_coords);
	void viewportResized(int w, int h);

	// To get click events:
	void addWidget(const GLUIWidgetRef& widget) { widgets.insert(widget); }
	void removeWidget(const GLUIWidgetRef& widget) { widgets.erase(widget); }


	static float getViewportMinMaxY(Reference<OpenGLEngine>& opengl_engine);


	Reference<OpenGLEngine> opengl_engine;
	GLUITextRendererCallback* text_renderer;

private:
	GLARE_DISABLE_COPY(GLUI);

	std::set<GLUIWidgetRef> widgets;

	OverlayObjectRef tooltip_overlay_ob;

	std::map<std::string, OpenGLTextureRef> tooltip_textures;
};


typedef Reference<GLUI> GLUIRef;


class GLUITextRendererCallback
{
public:
	virtual ~GLUITextRendererCallback() {}

	virtual OpenGLTextureRef makeToolTipTexture(const std::string& text) = 0;
};
