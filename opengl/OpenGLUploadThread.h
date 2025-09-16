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


class UploadingUserInfo : public ThreadSafeRefCounted
{
public:
	virtual ~UploadingUserInfo() {}
};


class UploadTextureMessage : public ThreadMessage
{
public:
	UploadTextureMessage() : frame_i(0) {}
	std::string tex_path;
	TextureParams tex_params;
	Reference<TextureData> texture_data;
	Reference<OpenGLTexture> existing_opengl_tex; // When uploading a frame of an animated texture, upload into this already existing texture.  Otherwise create a new texture
	int frame_i;
	Reference<UploadingUserInfo> user_info;
};


class TextureUploadedMessage : public ThreadMessage
{
public:
	std::string tex_path;
	TextureParams tex_params;
	Reference<TextureData> texture_data;
	Reference<OpenGLTexture> opengl_tex;
	Reference<UploadingUserInfo> user_info;
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
	virtual void doRun() override;


	void* gl_context;
	std::function<void (void* gl_context)> make_gl_context_current_func;
	OpenGLEngine* opengl_engine;
	ThreadSafeQueue<Reference<ThreadMessage> >* out_msg_queue;

private:

};
