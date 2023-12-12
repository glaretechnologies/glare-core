/*=====================================================================
GLUIImage.h
-----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUICallbackHandler.h"
#include "../OpenGLEngine.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/Rect2.h"
#include <string>


class GLUI;
class GLUIMouseWheelEvent;


/*=====================================================================
GLUIImage
---------

=====================================================================*/
class GLUIImage : public GLUIWidget
{
public:
	GLUIImage();
	~GLUIImage();

	void create(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& tex_path, const Vec2f& botleft, const Vec2f& dims,
		const std::string& tooltip, float z = -0.9f);

	void setColour(Colour3f colour_);
	void setMouseOverColour(Colour3f colour_);

	void destroy();

	const Vec2f& getPos() const { return pos; }

	void setDims(const Vec2f& dims);
	void setPosAndDims(const Vec2f& botleft, const Vec2f& dims, float z = -0.9f);
	void setTransform(const Vec2f& botleft, const Vec2f& dims, float rotation, float z = -0.9f);

	void setVisible(bool visible);

	virtual bool doHandleMouseMoved(const Vec2f& coords) override;
	virtual bool doHandleMouseWheelEvent(const Vec2f& coords, const GLUIMouseWheelEvent& event) override;


	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef overlay_ob;

	GLUICallbackHandler* handler;
private:
	GLARE_DISABLE_COPY(GLUIImage);
	
	Colour3f colour;
	Colour3f mouseover_colour;
	Vec2f pos; // Position of bottom left of image
	float z;
};


typedef Reference<GLUIImage> GLUIImageRef;
