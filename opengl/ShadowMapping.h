/*=====================================================================
ShadowMapping.h
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "IncludeOpenGL.h"
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
#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4324) // Disable 'structure was padded due to __declspec(align())' warning.
#endif

class ShadowMapping : public RefCounted
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	ShadowMapping();
	~ShadowMapping();

	void init();

	int numDynamicDepthTextures() { return 2; }
	float getDynamicDepthTextureScaleMultiplier() { return 6.0; }

	int numStaticDepthTextures() { return 3; }
	float getStaticDepthTextureScaleMultiplier() { return 6.0; }

	void bindDepthTexAsTarget();
	void bindStaticDepthTexAsTarget();
	void unbindDepthTex();

private:
	INDIGO_DISABLE_COPY(ShadowMapping);
public:
	Matrix4f shadow_tex_matrix[8];
	
	int dynamic_w, dynamic_h;
	int static_w, static_h;
	Reference<FrameBuffer> frame_buffer;
	Reference<OpenGLTexture> depth_tex;
	
	Reference<FrameBuffer> static_frame_buffer;
	Reference<OpenGLTexture> static_depth_tex;

	//Reference<OpenGLTexture> col_tex;

	//Reference<FrameBuffer> blur_fb;
};

#ifdef _WIN32
#pragma warning(pop)
#endif
