/*=====================================================================
PBOAsyncTextureUploader.h
-------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/Vector.h>
#include <queue>
class OpenGLTexture;
class TextureData;
class PBO;
struct __GLsync;
typedef struct __GLsync *GLsync;


struct PBOAsyncUploadedTextureInfo
{
	Reference<PBO> pbo;
	Reference<OpenGLTexture> opengl_tex;
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


	void startUploadingTexture(Reference<PBO> pbo, Reference<TextureData> texture_data, Reference<OpenGLTexture> opengl_tex);

	void checkForUploadedTexture(js::Vector<PBOAsyncUploadedTextureInfo, 16>& uploaded_textures_out);

private:
	struct PBOUploadingTexture
	{
		Reference<PBO> pbo;
		GLsync sync_ob;
		Reference<OpenGLTexture> opengl_tex;
	};

	std::queue<PBOUploadingTexture> uploading_textures;
};
