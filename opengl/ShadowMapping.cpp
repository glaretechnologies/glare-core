/*=====================================================================
ShadowMapping.cpp
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "ShadowMapping.h"


#include "OpenGLEngine.h"


ShadowMapping::ShadowMapping()
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
	static_frame_buffer = new FrameBuffer();

	//blur_fb = new FrameBuffer();
	
	// Create depth tex
	//depth_tex = new OpenGLTexture();
	//depth_tex->load(w, h, NULL, NULL, 
	//	GL_DEPTH_COMPONENT16, // internal format
	//	GL_DEPTH_COMPONENT, // format
	//	GL_FLOAT, // type
	//	true // nearest filtering
	//);

	depth_tex = new OpenGLTexture();
	depth_tex->load(dynamic_w, dynamic_h, ArrayRef<uint8>(NULL, 0), NULL,
		OpenGLTexture::Format_Depth_Float,
		OpenGLTexture::Filtering_Nearest // nearest filtering
	);

	static_depth_tex = new OpenGLTexture();
	static_depth_tex->load(static_w, static_h, ArrayRef<uint8>(NULL, 0), NULL,
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
	static_frame_buffer->bind();

	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, static_depth_tex->texture_handle, 0);

	glDrawBuffer(GL_NONE); // No color buffer is drawn to.
}


void ShadowMapping::unbindDepthTex()
{
	FrameBuffer::unbind();
}
