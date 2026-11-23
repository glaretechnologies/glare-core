/*=====================================================================
GLUIImage.h
-----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUICallbackHandler.h"


class OpenGLTexture;
struct OverlayObject;
class GLUI;
class GLUIMouseWheelEvent;


/*=====================================================================
GLUIImage
---------

=====================================================================*/
class GLUIImage final : public GLUIWidget
{
public:
	GLUIImage(GLUI& glui, const std::string& tex_path, const std::string& tooltip, float z = -0.9f);
	~GLUIImage();

	void setColour(Colour3f colour_);
	void setAlpha(float alpha);
	void setMouseOverColour(Colour3f colour_);

	void setTexture(const Reference<OpenGLTexture>& tex);

	void setDims(const Vec2f& dims);
	void setTransform(const Vec2f& botleft, const Vec2f& dims, float rotation, float z = -0.9f);

	virtual void setPos(const Vec2f& botleft) override;
	void setPosAndDims(const Vec2f& botleft, const Vec2f& dims);

	virtual void setClipRegion(const Rect2f& clip_rect) override;

	virtual void viewportResized() override;

	virtual void setVisible(bool visible) override;
	virtual bool isVisible() override;

	virtual void doHandleMouseMoved(GLUIMouseEvent& event) override;
	virtual void doHandleMouseWheelEvent(GLUIMouseWheelEvent& event) override;

	virtual std::string className() const override { return "GLUIImage"; }

	Reference<OverlayObject> overlay_ob;

private:
	void updateOverlayTransform();
	GLARE_DISABLE_COPY(GLUIImage);
	
	Colour3f colour;
	Colour3f mouseover_colour;
	//Vec2f pos; // Position of bottom left of image

	float rotation;
};


typedef Reference<GLUIImage> GLUIImageRef;
