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
	GLUIImage(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& tex_path, const Vec2f& botleft, const Vec2f& dims,
		const std::string& tooltip, float z = -0.9f);
	~GLUIImage();

	void setColour(Colour3f colour_);
	void setAlpha(float alpha);
	void setMouseOverColour(Colour3f colour_);

	const Vec2f& getPos() const { return pos; }

	void setDims(const Vec2f& dims);
	void setTransform(const Vec2f& botleft, const Vec2f& dims, float rotation, float z = -0.9f);

	virtual void setPosAndDims(const Vec2f& botleft, const Vec2f& dims) override;

	virtual void setClipRegion(const Rect2f& clip_rect) override;

	virtual void updateGLTransform() override;

	virtual void setVisible(bool visible) override;
	virtual bool isVisible() override;

	virtual void doHandleMouseMoved(MouseEvent& event) override;
	virtual void doHandleMouseWheelEvent(MouseWheelEvent& event) override;

	OverlayObjectRef overlay_ob;

	GLUICallbackHandler* handler;

	bool immutable_dims; // If true, don't change dimensions in setPosAndDims (which is called by GridContainer layout)
	Vec2f dims_px;
private:
	GLARE_DISABLE_COPY(GLUIImage);
	
	GLUI* glui;
	Reference<OpenGLEngine> opengl_engine;

	Colour3f colour;
	Colour3f mouseover_colour;
	Vec2f pos; // Position of bottom left of image
};


typedef Reference<GLUIImage> GLUIImageRef;
