/*=====================================================================
OpenGLUploadThread.h
--------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "OpenGLTexture.h"
#include "PBO.h"
#include "VBO.h"
#include <utils/ThreadSafeRefCounted.h>
#include <utils/Reference.h>
#include <utils/MessageableThread.h>
#include <string>
#include <map>
#include <functional>
class OpenGLEngine;
class OpenGLMeshRenderData;
struct GLObject;


class UploadingUserInfo : public ThreadSafeRefCounted
{
public:
	virtual ~UploadingUserInfo() {}
};


class UploadTextureMessage : public ThreadMessage
{
public:
	UploadTextureMessage() : is_animated_texture_update(false), frame_i(0) {}

	bool is_animated_texture_update;
	std::string tex_path;
	TextureParams tex_params;
	Reference<TextureData> texture_data;

	Reference<OpenGLTexture> old_tex; // When uploading a frame of an animated texture, upload into this already existing texture.  Otherwise create a new texture
	Reference<OpenGLTexture> new_tex; // When uploading the animated tex frame is done, swap existing_opengl_tex and swap_with_tex on the object

	int frame_i;
	Reference<UploadingUserInfo> user_info;
};

// Template specialisation of destroyAndFreeOb for UploadTextureMessage.  Forwards to destroyAndFreeOb for ThreadMessage, which will free from upload_texture_msg_allocator.
template <> inline void destroyAndFreeOb<UploadTextureMessage>(UploadTextureMessage* ob) { destroyAndFreeOb<ThreadMessage>(ob); }


class TextureUploadedMessage : public ThreadMessage
{
public:
	std::string tex_path;
	Reference<TextureData> texture_data;
	Reference<OpenGLTexture> opengl_tex;
	Reference<UploadingUserInfo> user_info;
};


class AnimatedTextureUpdated : public ThreadMessage
{
public:
	Reference<OpenGLTexture> old_tex; // When uploading a frame of an animated texture, upload into this already existing texture.  Otherwise create a new texture
	Reference<OpenGLTexture> new_tex; // When uploading the animated tex frame is done, swap existing_opengl_tex and swap_with_tex on the object
};

// Template specialisation of destroyAndFreeOb for UploadTextureMessage.  Forwards to destroyAndFreeOb for ThreadMessage, which will free from upload_texture_msg_allocator.
template <> inline void destroyAndFreeOb<AnimatedTextureUpdated>(AnimatedTextureUpdated* ob) { destroyAndFreeOb<ThreadMessage>(ob); }


class UnusedTextureUpdated : public ThreadMessage
{
public:
	Reference<OpenGLTexture> opengl_tex;
};


class UploadGeometryMessage : public ThreadMessage
{
public:
	Reference<OpenGLMeshRenderData> meshdata;
	
	// vert data offset = 0
	size_t index_data_src_offset_B;
	size_t total_geom_size_B;
	size_t vert_data_size_B; // in source VBO
	size_t index_data_size_B; // in source VBO

	Reference<UploadingUserInfo> user_info;
};


class GeometryUploadedMessage : public ThreadMessage
{
public:
	Reference<OpenGLMeshRenderData> meshdata;

	Reference<UploadingUserInfo> user_info;
};


/*=====================================================================
OpenGLUploadThread
------------------

=====================================================================*/
class OpenGLUploadThread : public MessageableThread
{
public:
	OpenGLUploadThread();

	virtual void doRun() override;

	UploadTextureMessage*   allocUploadTextureMessage();
	AnimatedTextureUpdated* allocAnimatedTextureUpdatedMessage();

	void* gl_context;
	std::function<void (void* gl_context)> make_gl_context_current_func;
	OpenGLEngine* opengl_engine;
	ThreadSafeQueue<Reference<ThreadMessage> >* out_msg_queue;

	Reference<glare::FastPoolAllocator> upload_texture_msg_allocator;
	Reference<glare::FastPoolAllocator> animated_texture_updated_allocator;
};
