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
		const std::string& tooltip);

	void destroy();

	void setPosAndDims(const Vec2f& botleft, const Vec2f& dims, float z = -0.9f);

	virtual bool doHandleMouseMoved(const Vec2f& coords) override;

	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef overlay_ob;
private:
	GLARE_DISABLE_COPY(GLUIImage);

	
};


typedef Reference<GLUIImage> GLUIImageRef;
