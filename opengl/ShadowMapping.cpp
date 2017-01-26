/*=====================================================================
ShadowMapping.cpp
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "IncludeOpenGL.h"
#include "ShadowMapping.h"


#include "OpenGLEngine.h"


ShadowMapping::ShadowMapping()
{
}


ShadowMapping::~ShadowMapping()
{
}


void ShadowMapping::init(const int w_, const int h_)
{
	w = w_;
	h = h_;

	// Create frame buffer
	frame_buffer = new FrameBuffer();

	blur_fb = new FrameBuffer();
	
	// Create depth tex
	//depth_tex = new OpenGLTexture();
	//depth_tex->load(w, h, NULL, NULL, 
	//	GL_DEPTH_COMPONENT16, // internal format
	//	GL_DEPTH_COMPONENT, // format
	//	GL_FLOAT, // type
	//	true // nearest filtering
	//);

	depth_tex = new OpenGLTexture();
	depth_tex->load(w, h, NULL, NULL, 
		GL_DEPTH_COMPONENT, // internal format
		GL_DEPTH_COMPONENT, // format
		GL_FLOAT, // type
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


void ShadowMapping::unbindDepthTex()
{
	frame_buffer->unbind();
}
