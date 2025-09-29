/*=====================================================================
PBOAsyncTextureUploader.h
-------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/Vector.h>
#include <utils/ThreadSafeRefCounted.h>
#include <queue>
class OpenGLTexture;
class TextureData;
class PBO;
struct __GLsync;
typedef struct __GLsync *GLsync;


class UploadingTextureUserInfo : public ThreadSafeRefCounted
{
public:
	virtual ~UploadingTextureUserInfo() {}
};


struct PBOAsyncUploadedTextureInfo
{
	Reference<PBO> pbo;
	Reference<OpenGLTexture> opengl_tex;
	Reference<UploadingTextureUserInfo> user_info;
};


/*=====================================================================
PBOAsyncTextureUploader
-----------------------
Loads textures into OpenGL texture objects from PBOs, asynchronously.
=====================================================================*/
class PBOAsyncTextureUploader
{
public:
	PBOAsyncTextureUploader();
	~PBOAsyncTextureUploader();

	void clear();

	void startUploadingTexture(Reference<PBO> pbo, Reference<TextureData> texture_data, Reference<OpenGLTexture> opengl_tex, Reference<OpenGLTexture> dummy_opengl_tex, Reference<PBO> dummy_pbo, uint64 frame_num, 
		Reference<UploadingTextureUserInfo> user_info);

	void checkForUploadedTexture(uint64 frame_num, js::Vector<PBOAsyncUploadedTextureInfo, 16>& uploaded_textures_out);

private:
	struct PBOUploadingTexture
	{
		Reference<PBO> pbo;
		GLsync sync_ob;
		Reference<OpenGLTexture> opengl_tex;
		Reference<TextureData> texture_data;
		uint64 frame_num;

		Reference<UploadingTextureUserInfo> user_info;
	};

	std::queue<PBOUploadingTexture> uploading_textures;
};
