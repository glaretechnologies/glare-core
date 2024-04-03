/*=====================================================================
ShadowMapping.h
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "FrameBuffer.h"
#include "OpenGLTexture.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/Matrix4f.h"
class OpenGLShader;
class OpenGLEngine;


/*=====================================================================
FrameBuffer
---------
See http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-16-shadow-mapping/

For 'static' shadow maps (wider view, updated less often), objects are randomly divided into 4 sets, with each set being draw onto the depth map on different frames.
There are two copies of the static shadow map depth map - one is rendered with while the other is being drawn to, then they are swapped every N frames.
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

	void init(OpenGLEngine* opengl_engine);

	static const int NUM_DYNAMIC_DEPTH_TEXTURES = 2;
	static int numDynamicDepthTextures() { return NUM_DYNAMIC_DEPTH_TEXTURES; }
	float getDynamicDepthTextureScaleMultiplier() { return 6.0; }

	static const int NUM_STATIC_DEPTH_TEXTURES = 3;
	static int numStaticDepthTextures() { return NUM_STATIC_DEPTH_TEXTURES; }
	float getStaticDepthTextureScaleMultiplier() { return 6.0; }

	void bindDepthTexFrameBufferAsTarget();
	void bindStaticDepthTexFrameBufferAsTarget(int index);
	void unbindFrameBuffer();

	void setCurStaticDepthTex(int i) { cur_static_depth_tex = i; }

private:
	GLARE_DISABLE_COPY(ShadowMapping);
public:
	// Shadow texture matrices to be set as uniforms in shaders
	Matrix4f dynamic_tex_matrix[NUM_DYNAMIC_DEPTH_TEXTURES];
	Matrix4f static_tex_matrix[NUM_STATIC_DEPTH_TEXTURES * 2];
	
	int dynamic_w, dynamic_h;
	int static_w, static_h;
	Reference<FrameBuffer> frame_buffer;
	Reference<OpenGLTexture> depth_tex;
	
	// There are 2 static depth maps: One is accumulated to over multiple frames, while the other is used for lookups.
	int cur_static_depth_tex; // 0 or 1
	Reference<FrameBuffer> static_frame_buffer[2];
	Reference<OpenGLTexture> static_depth_tex[2];

	Vec4f vol_centres[NUM_STATIC_DEPTH_TEXTURES * 2];

	//Reference<OpenGLTexture> col_tex;

	//Reference<FrameBuffer> blur_fb;
};

#ifdef _WIN32
#pragma warning(pop)
#endif
