/*=====================================================================
ShadowMapping.cpp
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "ShadowMapping.h"


#include "OpenGLEngine.h"


ShadowMapping::ShadowMapping()
:	cur_static_depth_tex(0)
{
}


ShadowMapping::~ShadowMapping()
{
}


void ShadowMapping::init()
{
	dynamic_w = 2048;
	dynamic_h = 2048 * numDynamicDepthTextures();

	static_w = 2048;
	static_h = 2048 * numStaticDepthTextures();

	// Create frame buffer
	frame_buffer = new FrameBuffer();
	for(int i=0; i<2; ++i)
		static_frame_buffer[i] = new FrameBuffer();

	//blur_fb = new FrameBuffer();
	
	// Create depth tex
	//depth_tex = new OpenGLTexture();
	//depth_tex->load(w, h, NULL, NULL, 
	//	GL_DEPTH_COMPONENT16, // internal format
	//	GL_DEPTH_COMPONENT, // format
	//	GL_FLOAT, // type
	//	true // nearest filtering
	//);

	depth_tex = new OpenGLTexture(dynamic_w, dynamic_h, NULL,
		OpenGLTexture::Format_Depth_Float,
		OpenGLTexture::Filtering_Nearest // nearest filtering
	);

	cur_static_depth_tex = 0;

	for(int i=0; i<2; ++i)
		static_depth_tex[i] = new OpenGLTexture(static_w, static_h, NULL,
			OpenGLTexture::Format_Depth_Float,
			OpenGLTexture::Filtering_Nearest // nearest filtering
		);

	//col_tex = new OpenGLTexture();
	//col_tex->load(w, h, NULL, NULL, 
	//	GL_RGB32F_ARB, // internal format
	//	GL_RGB, // format
	//	GL_FLOAT, // type
	//	true // nearest filtering
	//);
}


void ShadowMapping::bindDepthTexAsTarget()
{
	frame_buffer->bind();

	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_tex->texture_handle, 0);
	//glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, col_tex->texture_handle, 0);

	glDrawBuffer(GL_NONE); // No color buffer is drawn to.
}


void ShadowMapping::bindStaticDepthTexAsTarget()
{
	static_frame_buffer[cur_static_depth_tex]->bind();

	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, static_depth_tex[cur_static_depth_tex]->texture_handle, 0);

	glDrawBuffer(GL_NONE); // No color buffer is drawn to.
}


void ShadowMapping::unbindDepthTex()
{
	FrameBuffer::unbind();
}
