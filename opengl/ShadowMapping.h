/*=====================================================================
ShadowMapping.h
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include <QtOpenGL/QGLWidget>
#include "FrameBuffer.h"
#include "OpenGLTexture.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/Matrix4f.h"
#include <string>
class OpenGLShader;


/*=====================================================================
FrameBuffer
---------
See http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-16-shadow-mapping/
=====================================================================*/
class ShadowMapping : public RefCounted
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	ShadowMapping();
	~ShadowMapping();

	void init(const int w, const int h);

	void bindDepthTexAsTarget();
	void unbindDepthTex();

private:
	INDIGO_DISABLE_COPY(ShadowMapping);
public:
	Matrix4f shadow_tex_matrix;
	
	int w, h;
	Reference<FrameBuffer> frame_buffer;
	Reference<OpenGLTexture> depth_tex;
	//Reference<OpenGLTexture> col_tex;

	Reference<FrameBuffer> blur_fb;
};
