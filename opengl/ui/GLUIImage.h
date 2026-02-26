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
class GLUIImage final : public GLUIWidget
{
public:
	GLUIImage(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& tex_path, const std::string& tooltip, float z = -0.9f);
	~GLUIImage();

	void setColour(Colour3f colour_);
	void setAlpha(float alpha);
	void setMouseOverColour(Colour3f colour_);

	void setDims(const Vec2f& dims);
	void setTransform(const Vec2f& botleft, const Vec2f& dims, float rotation, float z = -0.9f);

	virtual void setPos(const Vec2f& botleft) override;
	virtual void setPosAndDims(const Vec2f& botleft, const Vec2f& dims) override;

	virtual void setClipRegion(const Rect2f& clip_rect) override;

	virtual void updateGLTransform() override;

	virtual void setVisible(bool visible) override;
	virtual bool isVisible() override;

	virtual void doHandleMouseMoved(MouseEvent& event) override;
	virtual void doHandleMouseWheelEvent(MouseWheelEvent& event) override;

	OverlayObjectRef overlay_ob;

	GLUICallbackHandler* handler;
private:
	void updateOverlayTransform();
	GLARE_DISABLE_COPY(GLUIImage);
	
	GLUI* glui;
	Reference<OpenGLEngine> opengl_engine;

	Colour3f colour;
	Colour3f mouseover_colour;
	//Vec2f pos; // Position of bottom left of image

	float rotation;
};


typedef Reference<GLUIImage> GLUIImageRef;
