/*=====================================================================
ShadowMapping.cpp
---------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "ShadowMapping.h"


#include "IncludeOpenGL.h"
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
	const int base_res = 2048;
	dynamic_w = base_res;
	dynamic_h = base_res * numDynamicDepthTextures();

	static_w = base_res;
	static_h = base_res * numStaticDepthTextures();

	// Create frame buffer
	frame_buffer = new FrameBuffer();
	for(int i=0; i<2; ++i)
		static_frame_buffer[i] = new FrameBuffer();

	// Create depth tex
	// We will use a 16-bit depth format as I haven't seen any noticable issues with it so far, compared to a 32-bit format.
	depth_tex = new OpenGLTexture(dynamic_w, dynamic_h, /*opengl_engine=*/NULL,
		ArrayRef<uint8>(NULL, 0),
		OpenGLTexture::Format_Depth_Uint16, // Uint16,
		OpenGLTexture::Filtering_PCF
	);

	// OpenGLTexture::Filtering_Nearest, OpenGLTexture::Filtering_PCF

	cur_static_depth_tex = 0;

	for(int i=0; i<2; ++i)
		static_depth_tex[i] = new OpenGLTexture(static_w, static_h, /*opengl_engine=*/NULL,
			ArrayRef<uint8>(NULL, 0), // 
			OpenGLTexture::Format_Depth_Uint16,
			OpenGLTexture::Filtering_PCF // nearest filtering
		);

	//col_tex = new OpenGLTexture();
	//col_tex->load(w, h, NULL, NULL, 
	//	GL_RGB32F_ARB, // internal format
	//	GL_RGB, // format
	//	GL_FLOAT, // type
	//	true // nearest filtering
	//);

	// Attach depth texture to frame buffer
	frame_buffer->bindForDrawing();

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tex->texture_handle, /*mipmap level=*/0);

	GLenum is_complete = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if(is_complete != GL_FRAMEBUFFER_COMPLETE)
	{
		throw glare::Exception("Error: ShadowMapping framebuffer is not complete: " + toHexString(is_complete));
		//conPrint("Error: framebuffer is not complete.");
		//assert(0);
	}

	FrameBuffer::unbind();

	// Attach static depth textures to static frame buffers
	for(int i=0; i<2; ++i)
	{
		static_frame_buffer[i]->bindForDrawing();

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, static_depth_tex[i]->texture_handle, /*mipmap level=*/0);

		is_complete = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if(is_complete != GL_FRAMEBUFFER_COMPLETE)
		{
			throw glare::Exception("Error: static ShadowMapping framebuffer is not complete.");
			//conPrint("Error: static framebuffer is not complete.");
			//assert(0);
		}

		// Because the static depth textures will take a few frames to be fully filled, and will be used for rendering for a few frames first, clear them so as not to show garbage.
		// Clear texture while it is bound to a framebuffer
		glClearDepthf(1.f);
		glClear(GL_DEPTH_BUFFER_BIT); // NOTE: not affected by current viewport dimensions.

		FrameBuffer::unbind();
	}
}


void ShadowMapping::bindDepthTexFrameBufferAsTarget()
{
	frame_buffer->bindForDrawing();
}


void ShadowMapping::bindStaticDepthTexFrameBufferAsTarget(int index)
{
	assert(index >= 0 && index < 2);

	static_frame_buffer[index]->bindForDrawing();
}


void ShadowMapping::unbindFrameBuffer()
{
	FrameBuffer::unbind();
}
