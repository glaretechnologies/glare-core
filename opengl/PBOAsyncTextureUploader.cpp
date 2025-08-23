/*=====================================================================
PBOAsyncTextureUploader.cpp
---------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "PBOAsyncTextureUploader.h"


#include "IncludeOpenGL.h"
#include "PBO.h"
#include "OpenGLEngine.h"
#include "../utils/FileUtils.h"
#include <tracy/Tracy.hpp>


PBOAsyncTextureUploader::PBOAsyncTextureUploader()
{}


PBOAsyncTextureUploader::~PBOAsyncTextureUploader()
{}


void PBOAsyncTextureUploader::startUploadingTexture(PBORef pbo, TextureDataRef texture_data, Reference<OpenGLTexture> opengl_tex)
{
	ZoneScoped; // Tracy profiler

	pbo->flushWholeBuffer();

	pbo->bind(); // Bind the PBO.  glTexSubImage2D etc. will read from this PBO.
	opengl_tex->bind();

	const size_t num_mip_levels_to_load = myMin((size_t)opengl_tex->getNumMipMapLevelsAllocated(), texture_data->numMipLevels());
	for(size_t k=0; k<num_mip_levels_to_load; ++k)
	{
		const size_t W = texture_data->W;
		const size_t H = texture_data->H;
		const size_t level_W = myMax((size_t)1, W / ((size_t)1 << k));
		const size_t level_H = myMax((size_t)1, H / ((size_t)1 << k));
		const size_t level_D = texture_data->D;

		const size_t level_offset = texture_data->level_offsets[k].offset;
		const size_t level_size   = texture_data->level_offsets[k].level_size;

		runtimeCheck(level_H > 0);
		const size_t row_stride_B = level_size / level_H; // not used for compressed textures.  Assume packed.

		opengl_tex->loadRegionIntoExistingTexture(/*mipmap level=*/(int)k, /*x=*/0, /*y=*/0, /*z=*/0, /*region_w=*/level_W, /*region_h=*/level_H, /*region depth=*/level_D, 
			row_stride_B, // not used for compressed textures
			ArrayRef<uint8>((const uint8*)level_offset, level_size), // tex data
			/*bind_needed=*/false
		);
	}

	opengl_tex->unbind();
	pbo->unbind();

	// Insert fence object into stream. We can query this to see if the copy from the PBO to the texture has completed.
	GLsync sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0);

	PBOUploadingTexture queue_item;
	queue_item.opengl_tex = opengl_tex;
	queue_item.pbo = pbo;
	queue_item.sync_ob = sync_ob;

	uploading_textures.push(queue_item);
}


void PBOAsyncTextureUploader::checkForUploadedTexture(js::Vector<PBOAsyncUploadedTextureInfo, 16>& uploaded_textures_out)
{
	ZoneScoped; // Tracy profiler

	uploaded_textures_out.clear();

	if(!uploading_textures.empty())
	{
		PBOUploadingTexture& front_item = uploading_textures.front();
		
		const GLenum wait_ret = glClientWaitSync(front_item.sync_ob, /*wait flags=*/0, /*waitDuration=*/0);
		assert(wait_ret != GL_WAIT_FAILED);
		if(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED)
		{
			// This texture has loaded from the PBO!
			
			PBOAsyncUploadedTextureInfo loaded_info;
			loaded_info.opengl_tex = front_item.opengl_tex;
			loaded_info.pbo = front_item.pbo;
			uploaded_textures_out.push_back(loaded_info);

			// Destroy sync objet
			glDeleteSync(front_item.sync_ob);

			uploading_textures.pop(); // NOTE: this has to go after all usage of front_item.
		}
	}
}
