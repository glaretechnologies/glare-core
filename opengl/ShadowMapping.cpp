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


void ShadowMapping::init(OpenGLEngine* opengl_engine)
{
	cur_static_depth_tex = 0;

	int initial_base_res = 2048;
	if(opengl_engine->settings.shadow_mapping_detail == OpenGLEngineSettings::ShadowMappingDetail_low)
		initial_base_res = 1024;
	else if(opengl_engine->settings.shadow_mapping_detail == OpenGLEngineSettings::ShadowMappingDetail_medium)
		initial_base_res = 2048;
	else if(opengl_engine->settings.shadow_mapping_detail == OpenGLEngineSettings::ShadowMappingDetail_high)
		initial_base_res = 8192;
	else
	{
		assert(0);
	}

	dynamic_w = initial_base_res;
	dynamic_h = initial_base_res * numDynamicDepthTextures();

	// Halve texture size until its dimensions are <= max_texture_size
	for(int i=0; i<10; ++i)
	{
		if(dynamic_h > opengl_engine->max_texture_size)
		{
			dynamic_w /= 2;
			dynamic_h /= 2;
		}
	}

	static_w = initial_base_res;
	static_h = initial_base_res * numStaticDepthTextures();

	for(int i=0; i<10; ++i)
	{
		if(static_h > opengl_engine->max_texture_size)
		{
			static_w /= 2;
			static_h /= 2;
		}
	}

	conPrint("Shadow map res: dynamic: " + toString(dynamic_w) + " x " + toString(dynamic_h) + ", static: " + toString(static_w) + " x " + toString(static_h));


	// Create depth textures
	// We will use a 16-bit depth format as I haven't seen any noticeable issues with it so far, compared to a 32-bit format.
	dynamic_depth_tex = new OpenGLTexture(dynamic_w, dynamic_h, /*opengl_engine=*/NULL,
		ArrayRef<uint8>(),
		OpenGLTextureFormat::Format_Depth_Uint16,
		OpenGLTexture::Filtering_PCF
	);

	for(int i=0; i<2; ++i)
		static_depth_tex[i] = new OpenGLTexture(static_w, static_h, /*opengl_engine=*/NULL,
			ArrayRef<uint8>(),
			OpenGLTextureFormat::Format_Depth_Uint16,
			OpenGLTexture::Filtering_PCF
		);

	// Create dynamic frame buffer
	{
		dynamic_frame_buffer = new FrameBuffer();

		// Attach depth texture to frame buffer
		dynamic_frame_buffer->attachTexture(*dynamic_depth_tex, GL_DEPTH_ATTACHMENT);
	
		const GLenum is_complete = dynamic_frame_buffer->checkCompletenessStatus();
		if(is_complete != GL_FRAMEBUFFER_COMPLETE)
			throw glare::Exception("Error: dynamic ShadowMapping framebuffer is not complete: " + toHexString(is_complete));
	}

	// Attach static depth textures to static frame buffers
	for(int i=0; i<2; ++i)
	{
		static_frame_buffer[i] = new FrameBuffer();
		static_frame_buffer[i]->attachTexture(*static_depth_tex[i], GL_DEPTH_ATTACHMENT);

		const GLenum is_complete = static_frame_buffer[i]->checkCompletenessStatus();
		if(is_complete != GL_FRAMEBUFFER_COMPLETE)
			throw glare::Exception("Error: static ShadowMapping framebuffer is not complete: " + toHexString(is_complete));

		// Because the static depth textures will take a few frames to be fully filled, and will be used for rendering for a few frames first, clear them so as not to show garbage.
		// Clear texture while it is bound to a framebuffer
		glClearDepthf(1.f);
		glClear(GL_DEPTH_BUFFER_BIT); // NOTE: not affected by current viewport dimensions.
	}

	FrameBuffer::unbind();
}


void ShadowMapping::bindDepthTexFrameBufferAsTarget()
{
	dynamic_frame_buffer->bindForDrawing();
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
