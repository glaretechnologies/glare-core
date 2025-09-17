/*=====================================================================
OpenGLUploadThread.cpp
----------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "OpenGLUploadThread.h"


#include "OpenGLEngine.h"
#include "IncludeOpenGL.h"
#include "../graphics/TextureData.h"
#include <utils/KillThreadMessage.h>


OpenGLUploadThread::OpenGLUploadThread()
{
	upload_texture_msg_allocator = new glare::FastPoolAllocator(sizeof(UploadTextureMessage), /*alignment=*/16, /*block capacity=*/64);
}


void OpenGLUploadThread::doRun()
{
	make_gl_context_current_func(gl_context);

	int next_pbo_index = 0;
	int next_vbo_index = 0;

	std::vector<PBORef> pbos(2);
	for(size_t i=0; i<pbos.size(); ++i)
	{
		pbos[i] = new PBO(64 * 1024 * 1024, /*for upload=*/true, /*create_persistent_buffer*/true);
		pbos[i]->map();
	}

	
	std::vector<VBORef> vert_vbos(2);
	for(size_t i=0; i<vert_vbos.size(); ++i)
	{
		vert_vbos[i] = new VBO(NULL, 64 * 1024 * 1024, GL_ARRAY_BUFFER, /*usage (not used)=*/GL_STREAM_DRAW, /*create_persistent_buffer*/true);
		vert_vbos[i]->map();
	}

	VBORef dummy_vert_vbo = new VBO(nullptr, 1024, GL_ARRAY_BUFFER);

	while(1)
	{
		ThreadMessageRef msg = getMessageQueue().dequeue();
		if(dynamic_cast<KillThreadMessage*>(msg.ptr()))
			break;
		else if(dynamic_cast<UploadTextureMessage*>(msg.ptr()))
		{
			UploadTextureMessage* upload_msg = dynamic_cast<UploadTextureMessage*>(msg.ptr());

			PBORef pbo  = pbos[next_pbo_index];
			next_pbo_index = (next_pbo_index + 1) % (int)pbos.size();

			//----------------------------- Copy texture data to PBO -----------------------------
			Reference<TextureData> texture_data = upload_msg->texture_data;
			ArrayRef<uint8> source_data = upload_msg->texture_data->getDataArrayRef();

			// Just upload a single frame
			if(texture_data->isMultiFrame())
			{
				//conPrint("OpenGLUploadThread: Uploading frame " + toString(upload_msg->frame_i));
				runtimeCheck(texture_data->frame_size_B * upload_msg->frame_i + texture_data->frame_size_B <= source_data.size());
				source_data = source_data.getSlice(/*offset=*/texture_data->frame_size_B * upload_msg->frame_i, /*slice len=*/texture_data->frame_size_B);
			}

			if(source_data.dataSizeBytes() > pbo->getSize())
			{
				conPrint("Texture is too big for PBO!!!");
				continue; // Just drop this texture for now
			}

			std::memcpy(pbo->getMappedPtr(), source_data.data(), source_data.size()); // TODO: remove memcpy and build texture data directly into PBO

			pbo->flushRange(0, source_data.size());

			//----------------------------- Free image texture memory now it has been copied to the PBO. -----------------------------
			if(!texture_data->isMultiFrame())
			{
				texture_data->mipmap_data.clearAndFreeMem();
				if(texture_data->converted_image)
					texture_data->converted_image = nullptr;
			}


			//----------------------------- Work out texture to upload to.  If uploading to an existing texture, use it.  If uploading to a new texture, create it. -----------------------------
			Reference<OpenGLTexture> opengl_tex;
			if(upload_msg->existing_opengl_tex)
				opengl_tex = upload_msg->existing_opengl_tex;
			else
			{
				opengl_tex = TextureLoading::createUninitialisedOpenGLTexture(*upload_msg->texture_data, opengl_engine, upload_msg->tex_params);
				opengl_tex->key = upload_msg->/*tex_key*/tex_path;

				if(texture_data->isMultiFrame())
				{
					// If this is texture data for an animated texture (Gif), then keep it around.
					// We need to keep around animated texture data like this, for now, since during animation different frames will be loaded into the OpenGL texture from the tex data.
					opengl_tex->texture_data = texture_data;
				}
			}

			//----------------------------- Copy from the PBO to the OpenGL texture. -----------------------------
			// Note that this copy needs to go before the fence is inserted, so that when/if the PBO is reused we know the GPU is finished with it and we can overwrite its content.
			pbo->bind(); // Bind the PBO.  glTexSubImage2D etc. will read from this PBO.
			opengl_tex->bind();

			const size_t num_mip_levels_to_load = myMin((size_t)opengl_tex->getNumMipMapLevelsAllocated(), texture_data->numMipLevels());
			for(size_t k=0; k<num_mip_levels_to_load; ++k)
			{
				const size_t level_W = myMax((size_t)1, texture_data->W / ((size_t)1 << k));
				const size_t level_H = myMax((size_t)1, texture_data->H / ((size_t)1 << k));

				const size_t level_offset = texture_data->level_offsets[k].offset;
				const size_t level_size   = texture_data->level_offsets[k].level_size;

				opengl_tex->loadRegionIntoExistingTexture(/*mipmap level=*/(int)k, /*x=*/0, /*y=*/0, /*z=*/0, /*region_w=*/level_W, /*region_h=*/level_H, /*region depth=*/texture_data->D, 
					/*row_stride_B=*/level_size / level_H, // not used for compressed textures  Assume packed.
					ArrayRef<uint8>((const uint8*)level_offset, level_size), // tex data
					/*bind_needed=*/false
				);
			}

			opengl_tex->unbind();
			pbo->unbind();

			if(!upload_msg->existing_opengl_tex)
			{
				//----------------------------- Block until PBO upload and copy to OpenGL texture have fully completed -----------------------------
				// Insert fence object into stream. We can query this to see if the copy from the PBO to the texture has completed.
				GLsync sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0);
				[[maybe_unused]] const GLenum wait_ret = glClientWaitSync(sync_ob, /*wait flags=*/GL_SYNC_FLUSH_COMMANDS_BIT, /*waitDuration=*/(uint64)1.0e15);
				assert(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED);
				glDeleteSync(sync_ob); // Destroy sync object

				//----------------------------- Get the bindless texture handle in this thread as can take a while. -----------------------------
				opengl_tex->createBindlessTextureHandle();

				//----------------------------- Send TextureUploadedMessage back to client code -----------------------------
				Reference<TextureUploadedMessage> uploaded_msg = new TextureUploadedMessage();
				uploaded_msg->tex_path = upload_msg->tex_path;
				uploaded_msg->tex_params = upload_msg->tex_params;
				uploaded_msg->texture_data = upload_msg->texture_data;
				uploaded_msg->opengl_tex = opengl_tex;
				uploaded_msg->user_info = upload_msg->user_info;

				out_msg_queue->enqueue(uploaded_msg);
			}
			else
				glFlush(); // Flush command buffers so existing texture is updated ASAP.  Needed for timely updates of animated textures.

		}
		else if(dynamic_cast<UploadGeometryMessage*>(msg.ptr()))
		{
			UploadGeometryMessage* upload_msg = dynamic_cast<UploadGeometryMessage*>(msg.ptr());
			Reference<OpenGLMeshRenderData> meshdata = upload_msg->meshdata;

			VBORef vert_vbo  = vert_vbos[next_vbo_index];
			next_vbo_index = (next_vbo_index + 1) % (int)vert_vbos.size();

			VBORef index_vbo = NULL;


			//----------------------------- Copy mesh data to mapped VBO -----------------------------
			ArrayRef<uint8> vert_data, index_data;
			meshdata->getVertAndIndexArrayRefs(vert_data, index_data);

			if(upload_msg->total_geom_size_B > vert_vbo->getSize())
			{
				conPrint("Geometry is too big for VBO!!!");
				continue; // Just drop this geometry for now
			}

			// Copy vertex data first
			std::memcpy(vert_vbo->getMappedPtr(), vert_data.data(), vert_data.size());

			// Copy index data  TEMP putting in one combined buffer.
			std::memcpy((uint8*)vert_vbo->getMappedPtr() + upload_msg->index_data_src_offset_B, index_data.data(), index_data.size());

			vert_vbo->flushRange(0, upload_msg->total_geom_size_B);

			// Free geometry memory now it has been copied to the PBO.
			meshdata->clearAndFreeGeometryMem();


			//----------------------------- Do a copy to the dummy VBO to force the upload to the GPU to take place. -----------------------------
			// NOTE: needed?
			glBindBuffer(GL_COPY_READ_BUFFER,  vert_vbo->bufferName());
			glBindBuffer(GL_COPY_WRITE_BUFFER, dummy_vert_vbo->bufferName());
			glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/0, /*writeOffset=*/0, /*size=*/8);
			glBindBuffer(GL_COPY_READ_BUFFER, 0); // Unbind
			glBindBuffer(GL_COPY_WRITE_BUFFER, 0); // Unbind

			//if(index_vbo)
			//{
			//	glBindBuffer(GL_COPY_READ_BUFFER,  index_vbo->bufferName());
			//	glBindBuffer(GL_COPY_WRITE_BUFFER, dummy_index_vbo->bufferName());
			//	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/0, /*writeOffset=*/0, /*size=*/8);
			//	glBindBuffer(GL_COPY_READ_BUFFER, 0); // Unbind
			//	glBindBuffer(GL_COPY_WRITE_BUFFER, 0); // Unbind
			//}

			GLsync sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0);
			GLenum wait_ret = glClientWaitSync(sync_ob, /*wait flags=*/GL_SYNC_FLUSH_COMMANDS_BIT, /*waitDuration=*/(uint64)1.0e15);
			assert(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED);
			glDeleteSync(sync_ob); // Destroy sync object


			//----------------------------- Allocate space in vertex and index buffers -----------------------------
			// Note that VAOs can't be shared across contexts, so don't create/get the VAO here.
			meshdata->vbo_handle         = opengl_engine->vert_buf_allocator->allocateVertexDataSpace(meshdata->vertex_spec.vertStride(), /*vert data=*/nullptr, upload_msg->vert_data_size_B);
			meshdata->indices_vbo_handle = opengl_engine->vert_buf_allocator->allocateIndexDataSpace(/*index data=*/nullptr, upload_msg->index_data_size_B);



			//----------------------------- Do an on-GPU (hopefully) copy of the source vertex data to the new buffer at the allocated position. -----------------------------
			glBindBuffer(GL_COPY_READ_BUFFER,  vert_vbo->bufferName());
			glBindBuffer(GL_COPY_WRITE_BUFFER, upload_msg->meshdata->vbo_handle.vbo->bufferName());
			glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/0, /*writeOffset=*/meshdata->vbo_handle.offset, meshdata->vbo_handle.size);

			//----------------------------- Do an on-GPU (hopefully) copy of the source index data to the new buffer at the allocated position. -----------------------------
			// index_vbo may be null in which case both index and vert data is in vert_vbo.
			glBindBuffer(GL_COPY_READ_BUFFER,  (index_vbo ? index_vbo.ptr() : vert_vbo.ptr())->bufferName());
			glBindBuffer(GL_COPY_WRITE_BUFFER, meshdata->indices_vbo_handle.index_vbo->bufferName());
			glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/upload_msg->index_data_src_offset_B, /*writeOffset=*/meshdata->indices_vbo_handle.offset, meshdata->indices_vbo_handle.size);

			// Unbind
			glBindBuffer(GL_COPY_READ_BUFFER, 0);
			glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

			//----------------------------- Block until all copies have completed -----------------------------
			sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0);
			wait_ret = glClientWaitSync(sync_ob, /*wait flags=*/GL_SYNC_FLUSH_COMMANDS_BIT, /*waitDuration=*/(uint64)1.0e15);
			assert(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED);
			glDeleteSync(sync_ob); // Destroy sync object

			//----------------------------- Send GeometryUploadedMessage back to client code -----------------------------
			Reference<GeometryUploadedMessage> uploaded_msg = new GeometryUploadedMessage();
			uploaded_msg->meshdata = meshdata;
			uploaded_msg->user_info = upload_msg->user_info;

			out_msg_queue->enqueue(uploaded_msg);
		}
		else
		{
			assert(0);
		}
	}
}


UploadTextureMessage* OpenGLUploadThread::allocUploadTextureMessage()
{
	glare::FastPoolAllocator::AllocResult alloc_res = upload_texture_msg_allocator->alloc();

	UploadTextureMessage* msg = new (alloc_res.ptr) UploadTextureMessage(); // Construct with placement new
	msg->allocator = upload_texture_msg_allocator.ptr();
	msg->allocation_index = alloc_res.index;

	return msg;
}
