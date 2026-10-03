/*=====================================================================
OpenGLUploadThread.cpp
----------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "OpenGLUploadThread.h"


#include "OpenGLEngine.h"
#include "IncludeOpenGL.h"
#include "../graphics/TextureData.h"
#include "../maths/mathstypes.h"
#include <utils/KillThreadMessage.h>
#include <utils/PlatformUtils.h>
#include <tracy/Tracy.hpp>
#include <array>


OpenGLUploadThread::OpenGLUploadThread()
{
	upload_texture_msg_allocator       = new glare::FastPoolAllocator(sizeof(UploadTextureMessage),   /*alignment=*/16, /*block capacity=*/64);
	upload_texture_msg_allocator->name = "OpenGLUploadThread UploadTextureMessage allocator";
	animated_texture_updated_allocator = new glare::FastPoolAllocator(sizeof(AnimatedTextureUpdated), /*alignment=*/16, /*block capacity=*/64);
	animated_texture_updated_allocator->name = "OpenGLUploadThread AnimatedTextureUpdated allocator";
}


// Is this an upload of a newly loaded texture or mesh?  (As opposed to an upload of the next frame of an animated texture.)
static bool isNewResourceUpload(const ThreadMessage* msg)
{
	if(const UploadTextureMessage* upload_tex_msg = dynamic_cast<const UploadTextureMessage*>(msg))
		return !upload_tex_msg->is_animated_texture_update;
	return dynamic_cast<const UploadGeometryMessage*>(msg) != nullptr;
}


void OpenGLUploadThread::enqueueUpload(const Reference<ThreadMessage>& msg)
{
	if(isNewResourceUpload(msg.ptr()))
		num_new_resource_uploads_pending++;
	getMessageQueue().enqueue(msg);
}


struct StagingBuffer
{
	StagingBuffer() : fence_sync_ob(0), used_B(0) {}

	VBORef vbo;
	GLsync fence_sync_ob;
	size_t used_B; // Amount of the buffer used by the current batch.
};


// Different approaches for uploading data:
// 
// glBufferSubData approach is the simplest in terms of code, but relies on the driver behaving nicely and not introducing stutters when copying data into existing large buffers
// used for rendering.
//
// Using a ring of staging buffers allows overlapped uploading of data, and handles large uploads without requiring a large staging buffer, by looping.
//
// Using a single large staging buffer is a little simpler but the staging buffer must be large enough to hold all uploads we need to do.
//
// The ring buffer approach seems just as fast as the large staging buffer, and requires less memory and doesn't limit the upload size, so we will use that approach.
#define USE_GL_BUFFER_SUB_DATA 0
#define USE_STAGING_RING_BUFFERS 1
#define USE_SINGLE_LARGE_STAGING_BUFFER 0


void OpenGLUploadThread::doRun()
{
	PlatformUtils::setCurrentThreadName("OpenGLUploadThread");

	Timer overall_run_timer;

	make_gl_context_current_func(gl_context);


	// NOTE: biggest geometry seen in Substrata is Green_Lawn_obj_6978297763328388609_opt3.bmesh, 103 MB.
	// Biggest texture data seen is ~25 MB.

	PBORef pbo = new PBO(64 * 1024 * 1024, /*for upload=*/true, /*create_persistently_mapped_buffer=*/true);
	pbo->map();

#if USE_SINGLE_LARGE_STAGING_BUFFER
	VBORef staging_vbo;
	staging_vbo = new VBO(NULL, 128 * 1024 * 1024, GL_ARRAY_BUFFER, /*usage (not used)=*/GL_STREAM_DRAW, /*create_persistently_mapped_buffer=*/true);
	staging_vbo->map();

	VBORef dummy_vert_vbo = new VBO(nullptr, 1024, GL_ARRAY_BUFFER);
#endif

#if USE_STAGING_RING_BUFFERS
	const int NUM_STAGING_BUFFERS = 4;
	std::array<StagingBuffer, 4> staging_buffers;
	for(size_t i=0; i<NUM_STAGING_BUFFERS; ++i)
	{
		staging_buffers[i].vbo = new VBO(NULL, 8 * 1024 * 1024, GL_ARRAY_BUFFER, /*usage (not used)=*/GL_STREAM_DRAW, /*create_persistently_mapped_buffer=*/true);
		staging_buffers[i].vbo->map();
	}
	int cur_staging_buffer = 0; // Index of the staging buffer being filled.
#endif


	Timer timer;
	uint64 uploaded_in_period_B = 0;
	uint64 total_uploaded_B = 0;
	size_t largest_tex_B = 0;
	size_t largest_geom_B = 0;


	// Uploads are done in batches: the uploads for the queued messages are issued, then we block once until the GPU has completed all of them, then the 'uploaded'
	// messages are sent.  Blocking after each upload instead limits uploading to around one item per frame when the GPU is busy rendering, as each wait has to
	// wait for the frame being rendered.
	const size_t MAX_BATCH_SIZE = 64;
	std::vector<ThreadMessageRef> batch_uploaded_msgs; // Messages to send once the uploads in the current batch have completed.
	std::vector<Reference<OpenGLTexture>> batch_textures_needing_bindless_handles;
	size_t pbo_write_offset = 0; // Where the next texture in the current batch goes in the PBO.

	// Blocks until all the texture and geometry uploads in the current batch have completed, then sends the 'uploaded' messages for them, and starts a new batch.
	auto finishUploadingBatch = [&]()
	{
		if(batch_uploaded_msgs.empty())
			return;

		{
			ZoneScopedN("blocking upload"); // Tracy profiler

			// Block until all the uploads in the batch have completed: the copies from the PBO to the textures, and from the staging buffers to the vertex and index buffers.
			GLsync sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0);
			[[maybe_unused]] const GLenum wait_ret = glClientWaitSync(sync_ob, /*wait flags=*/GL_SYNC_FLUSH_COMMANDS_BIT, /*waitDuration=*/(uint64)1.0e15);
			assert(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED);
			glDeleteSync(sync_ob); // Destroy sync object
		}

#if USE_STAGING_RING_BUFFERS
		// The wait above covers the copies from the staging buffers, so their fences aren't needed any more, and they can all be reused from the start.
		for(size_t i=0; i<NUM_STAGING_BUFFERS; ++i)
		{
			if(staging_buffers[i].fence_sync_ob != 0)
			{
				glDeleteSync(staging_buffers[i].fence_sync_ob);
				staging_buffers[i].fence_sync_ob = 0;
			}
			staging_buffers[i].used_B = 0;
		}
#endif

		// Get the bindless texture handles in this thread as can take a while.
		for(size_t i=0; i<batch_textures_needing_bindless_handles.size(); ++i)
			if(batch_textures_needing_bindless_handles[i]->bindless_tex_handle == 0)
				batch_textures_needing_bindless_handles[i]->createBindlessTextureHandle();
		batch_textures_needing_bindless_handles.clear(); // Release this thread's texture references before sending messages, so that making non-resident from textureRefCountDecreasedToOne only ever happens on the main thread.

		for(size_t i=0; i<batch_uploaded_msgs.size(); ++i)
		{
			const int msg_id = batch_uploaded_msgs[i]->id;
			out_msg_queue->enqueue(batch_uploaded_msgs[i]);

			// A new resource upload is no longer pending once its 'uploaded' message has been sent.  (AnimatedTextureUpdated messages are for animated texture frames, which aren't counted.)
			if(msg_id == OpenGLUploadThreadMessages_TextureUploadedMessage || msg_id == OpenGLUploadThreadMessages_GeometryUploadedMessage)
				num_new_resource_uploads_pending--;
		}
		batch_uploaded_msgs.clear();

		pbo_write_offset = 0;
	};


	while(1)
	{
		// Finish the current batch if there's nothing else to add to it right now (dequeue() would block), or it's big enough.
		if(getMessageQueue().empty() || (batch_uploaded_msgs.size() >= MAX_BATCH_SIZE))
			finishUploadingBatch();

		ThreadMessageRef msg = getMessageQueue().dequeue();

		if(dynamic_cast<KillThreadMessage*>(msg.ptr()))
		{
			finishUploadingBatch();
			break;
		}
		else if(dynamic_cast<UploadTextureMessage*>(msg.ptr()))
		{
			try
			{
				UploadTextureMessage* upload_msg = static_cast<UploadTextureMessage*>(msg.ptr());

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
					const std::string err_msg = "Error while uploading texture to GPU: Trying to upload texture of " + getNiceByteSize(source_data.dataSizeBytes()) + ", max size is " + getNiceByteSize(pbo->getSize()) + ".";
					out_msg_queue->enqueue(new OpenGLUploadErrorMessage(err_msg));
					if(isNewResourceUpload(msg.ptr()))
						num_new_resource_uploads_pending--; // This upload won't happen, so is no longer pending.
					continue; // Just drop this texture for now
				}

				// Put the texture data after the data for the textures already in this batch, if it fits.  Otherwise finish the batch, so the PBO can be reused from the start.
				size_t pbo_offset = Maths::roundUpToMultipleOfPowerOf2<size_t>(pbo_write_offset, 256);
				if(pbo_offset + source_data.dataSizeBytes() > pbo->getSize())
				{
					finishUploadingBatch();
					pbo_offset = 0;
				}
				pbo_write_offset = pbo_offset + source_data.dataSizeBytes();

				{
					ZoneScopedN("memcpy to PBO"); // Tracy profiler
					std::memcpy((uint8*)pbo->getMappedPtr() + pbo_offset, source_data.data(), source_data.size()); // TODO: remove memcpy and build texture data directly into PBO
				}

				{
					ZoneScopedN("flushRange"); // Tracy profiler
					pbo->flushRange(pbo_offset, source_data.size());
				}

				//----------------------------- Free image texture memory now it has been copied to the PBO. -----------------------------
				if(!texture_data->isMultiFrame())
				{
					texture_data->mipmap_data.clearAndFreeMem();
					texture_data->converted_image = nullptr;
				}


				//----------------------------- Work out texture to upload to.  If uploading to an existing texture, use it.  If uploading to a new texture, create it. -----------------------------
				Reference<OpenGLTexture> opengl_tex;
				if(upload_msg->new_tex)
					opengl_tex = upload_msg->new_tex;
				else
				{
					opengl_tex = TextureLoading::createUninitialisedOpenGLTexture(*upload_msg->texture_data, opengl_engine, upload_msg->tex_params);
					opengl_tex->key = upload_msg->/*tex_key*/tex_path;

					// If this is texture data for an animated texture (Gif), then keep it around.
					// We need to keep around animated texture data like this, for now, since during animation different frames will be loaded into the OpenGL texture from the tex data.
					if(texture_data->isMultiFrame())
						opengl_tex->texture_data = texture_data;
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
						ArrayRef<uint8>((const uint8*)(pbo_offset + level_offset), level_size), // tex data: an offset into the bound PBO
						/*bind_needed=*/false
					);
				}

				opengl_tex->unbind();
				pbo->unbind();

				// The bindless texture handle is got once the upload has completed, in finishUploadingBatch().
				if(opengl_engine->use_bindless_textures && (opengl_tex->bindless_tex_handle == 0))
					batch_textures_needing_bindless_handles.push_back(opengl_tex);

				//----------------------------- Queue the message to send back to client code once the batch's uploads have completed -----------------------------
				if(upload_msg->is_animated_texture_update) // If doing animated texture update:
				{
					Reference<AnimatedTextureUpdated> updated_msg = allocAnimatedTextureUpdatedMessage();
					updated_msg->old_tex.takeFrom(upload_msg->old_tex);
					updated_msg->new_tex = opengl_tex;

					upload_msg->new_tex = nullptr;
					opengl_tex = nullptr; // Null out this thread's reference before sending message, so that making non-resident from textureRefCountDecreasedToOne only ever happens on the main thread.

					batch_uploaded_msgs.push_back(updated_msg);
				}
				else
				{
					Reference<TextureUploadedMessage> uploaded_msg = new TextureUploadedMessage();
					uploaded_msg->tex_path = upload_msg->tex_path;
					uploaded_msg->texture_data = upload_msg->texture_data;
					uploaded_msg->opengl_tex = opengl_tex;
					uploaded_msg->user_info = upload_msg->user_info;

					opengl_tex = nullptr; // Null out this thread's reference before sending message, so that making non-resident from textureRefCountDecreasedToOne only ever happens on the main thread.

					batch_uploaded_msgs.push_back(uploaded_msg);
				}

				total_uploaded_B += source_data.size();
				uploaded_in_period_B += source_data.size();
				largest_tex_B = myMax(largest_tex_B, source_data.size());
			}
			catch(glare::Exception& e)
			{
				const std::string err_msg = "Error while uploading texture to GPU: " + e.what();
				out_msg_queue->enqueue(new OpenGLUploadErrorMessage(err_msg));
				if(isNewResourceUpload(msg.ptr()))
					num_new_resource_uploads_pending--; // This upload failed, so is no longer pending.
			}
		}
		else if(dynamic_cast<UploadGeometryMessage*>(msg.ptr()))
		{
			try
			{
				UploadGeometryMessage* upload_msg = static_cast<UploadGeometryMessage*>(msg.ptr());
				Reference<OpenGLMeshRenderData> meshdata = upload_msg->meshdata;
				
				ArrayRef<uint8> vert_data, index_data;
				meshdata->getVertAndIndexArrayRefs(vert_data, index_data);

				// Timer timer2;

#if USE_GL_BUFFER_SUB_DATA   // If Upload with glBufferSubData:

				//----------------------------- Allocate space in vertex and index buffers -----------------------------
				// Note that VAOs can't be shared across contexts, so don't create/get the VAO here.
				meshdata->vbo_handle         = opengl_engine->vert_buf_allocator->allocateVertexDataSpace(meshdata->vertex_spec.vertStride(), /*vert data=*/nullptr, vert_data.dataSizeBytes());
				meshdata->indices_vbo_handle = opengl_engine->vert_buf_allocator->allocateIndexDataSpace(/*index data=*/nullptr, index_data.dataSizeBytes());

				meshdata->vbo_handle.vbo              ->updateData(/*offset=*/meshdata->vbo_handle.offset,         /*data=*/vert_data.data(),  /*data size=*/vert_data.dataSizeBytes());

				meshdata->indices_vbo_handle.index_vbo->updateData(/*offset=*/meshdata->indices_vbo_handle.offset, /*data=*/index_data.data(), /*data size=*/index_data.dataSizeBytes());

				// Free geometry memory now it has been copied.
				meshdata->clearAndFreeGeometryMem();

				//----------------------------- Block until all copies have completed -----------------------------
				// We need to block until the glBufferSubData calls have completed, e.g. until the data has been copied to the destination buffers on the GPU, so
				// that it's safe to start rendering using the data in those GPU buffers. 
				// NOTE: we need to do this before we send a GeometryUploadedMessage to the main thread which starts rendering using the uploaded geometry.
				GLsync sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0);
				GLenum wait_ret = glClientWaitSync(sync_ob, /*wait flags=*/GL_SYNC_FLUSH_COMMANDS_BIT, /*waitDuration=*/(uint64)1.0e15);
				assert(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED);
				glDeleteSync(sync_ob); // Destroy sync object

#elif USE_STAGING_RING_BUFFERS // else upload with staging/temp mem-mapped VBO:
				
				//----------------------------- Allocate space in vertex and index buffers -----------------------------
				// Note that VAOs can't be shared across contexts, so don't create/get the VAO here.
				meshdata->vbo_handle         = opengl_engine->vert_buf_allocator->allocateVertexDataSpace(meshdata->vertex_spec.vertStride(), /*vert data=*/nullptr, vert_data.size());
				meshdata->indices_vbo_handle = opengl_engine->vert_buf_allocator->allocateIndexDataSpace(/*index data=*/nullptr, index_data.size());

				//conPrint("OpenGLUploadThread: uploading vert data to offset: " + toString(meshdata->vbo_handle.offset) + ", size: " + toString(meshdata->vbo_handle.size) + ", index data to " + 
				//	toString(meshdata->indices_vbo_handle.offset) + ", size: " + toString(meshdata->indices_vbo_handle.size));

				const ArrayRef<uint8> datas[2] = { vert_data, index_data };
				for(int i=0; i<2; ++i)
				{
					const ArrayRef<uint8> data = datas[i];

					VBORef dest_vbo              = (i == 0) ? meshdata->vbo_handle.vbo : meshdata->indices_vbo_handle.index_vbo;
					const size_t dest_vbo_offset = (i == 0) ? meshdata->vbo_handle.offset : meshdata->indices_vbo_handle.offset;

					// Copy from data in chunks until completely copied.  Chunks are packed into the current staging buffer until it is full, then we move on to the next one, so that many
					// small meshes can be uploaded in a batch without waiting for a staging buffer to become free.
					for(size_t begin=0; begin<data.size(); )
					{
						StagingBuffer* staging_buffer = &staging_buffers[cur_staging_buffer];
						if(staging_buffer->used_B >= staging_buffer->vbo->getSize()) // If the current staging buffer is full:
						{
							// Create a fence object, which we can query to see if the copies from this staging buffer are done, and it can be reused.
							staging_buffer->fence_sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0); // Returns a non-zero name on success.

							cur_staging_buffer = (cur_staging_buffer + 1) % NUM_STAGING_BUFFERS; // Move on to the next staging buffer
							staging_buffer = &staging_buffers[cur_staging_buffer];

							// Block until this staging buffer has finished being used, if it is currently being used:
							if(staging_buffer->fence_sync_ob != 0) // If the fence exists:
							{
								[[maybe_unused]] GLenum wait_ret = glClientWaitSync(staging_buffer->fence_sync_ob, /*wait flags=*/GL_SYNC_FLUSH_COMMANDS_BIT, /*waitDuration=*/(uint64)1.0e15);
								assert(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED);
								glDeleteSync(staging_buffer->fence_sync_ob); // Destroy sync object
								staging_buffer->fence_sync_ob = 0;
							}
							staging_buffer->used_B = 0;
						}

						// Copy the rest of the data, or as much as fits in the staging buffer, in which case the rest goes in the next one.
						// The staging buffer isn't full here (we moved on above if it was), so chunk_size >= 1.
						const size_t staging_offset = staging_buffer->used_B;
						const size_t chunk_size = myMin(data.size() - begin, staging_buffer->vbo->getSize() - staging_offset);

						// Copy into staging VBO
						std::memcpy((uint8*)staging_buffer->vbo->getMappedPtr() + staging_offset, &data[begin], chunk_size);

						staging_buffer->vbo->flushRange(staging_offset, chunk_size);

						//----------------------------- Do an on-GPU (hopefully) copy of the source data to the new buffer at the allocated position. -----------------------------
						glBindBuffer(GL_COPY_READ_BUFFER,  staging_buffer->vbo->bufferName());
						glBindBuffer(GL_COPY_WRITE_BUFFER, dest_vbo->bufferName());
						glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/staging_offset, /*writeOffset=*/dest_vbo_offset + begin, /*size=*/chunk_size);

						staging_buffer->used_B = myMin(staging_buffer->vbo->getSize(), Maths::roundUpToMultipleOfPowerOf2<size_t>(staging_offset + chunk_size, 16));
						begin += chunk_size;
					}
				}

				// The copies are waited for in finishUploadingBatch(), before the GeometryUploadedMessage is sent to the main thread, which starts rendering using the uploaded geometry.

				// Unbind
				glBindBuffer(GL_COPY_READ_BUFFER, 0);
				glBindBuffer(GL_COPY_WRITE_BUFFER, 0);


				// Free geometry memory now it has been copied to the GPU.
				meshdata->clearAndFreeGeometryMem();

#elif USE_SINGLE_LARGE_STAGING_BUFFER
				//----------------------------- Copy mesh data to mapped VBO -----------------------------
				VBORef& vbo  = staging_vbo;

				const size_t index_data_src_offset_B = Maths::roundUpToMultipleOfPowerOf2<size_t>(vert_data.size(), 16); // Offset in VBO
				const size_t total_geom_size_B = index_data_src_offset_B + index_data.size();

				if(total_geom_size_B > vbo->getSize())
				{
					const std::string err_msg = "Error while uploading geometry to GPU: Trying to upload mesh of " + getNiceByteSize(total_geom_size_B) + ", max size is " + getNiceByteSize(vbo->getSize()) + ".";
					out_msg_queue->enqueue(new OpenGLUploadErrorMessage(err_msg));
					num_new_resource_uploads_pending--; // This upload won't happen, so is no longer pending.
					continue; // Just drop this geometry for now
				}

				// Copy vertex data first
				std::memcpy(vbo->getMappedPtr(), vert_data.data(), vert_data.size());

				// Copy index data, putting in one combined buffer.
				std::memcpy((uint8*)vbo->getMappedPtr() + index_data_src_offset_B, index_data.data(), index_data.size());

				// Timer timer2;

				vbo->flushRange(0, total_geom_size_B);

				// Free geometry memory now it has been copied to the PBO.
				meshdata->clearAndFreeGeometryMem();


				//----------------------------- Do a copy to the dummy VBO to force the upload to the GPU to take place. -----------------------------
				// NOTE: needed?
				glBindBuffer(GL_COPY_READ_BUFFER,  vbo->bufferName());
				glBindBuffer(GL_COPY_WRITE_BUFFER, dummy_vert_vbo->bufferName());
				glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/0, /*writeOffset=*/0, /*size=*/8);
				glBindBuffer(GL_COPY_READ_BUFFER, 0); // Unbind
				glBindBuffer(GL_COPY_WRITE_BUFFER, 0); // Unbind

				GLsync sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0);
				GLenum wait_ret = glClientWaitSync(sync_ob, /*wait flags=*/GL_SYNC_FLUSH_COMMANDS_BIT, /*waitDuration=*/(uint64)1.0e15);
				assert(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED);
				glDeleteSync(sync_ob); // Destroy sync object


				//----------------------------- Allocate space in vertex and index buffers -----------------------------
				// Note that VAOs can't be shared across contexts, so don't create/get the VAO here.
				meshdata->vbo_handle         = opengl_engine->vert_buf_allocator->allocateVertexDataSpace(meshdata->vertex_spec.vertStride(), /*vert data=*/nullptr, vert_data.size());
				meshdata->indices_vbo_handle = opengl_engine->vert_buf_allocator->allocateIndexDataSpace(/*index data=*/nullptr, index_data.size());



				//----------------------------- Do an on-GPU (hopefully) copy of the source vertex data to the new buffer at the allocated position. -----------------------------
				glBindBuffer(GL_COPY_READ_BUFFER,  vbo->bufferName());
				glBindBuffer(GL_COPY_WRITE_BUFFER, upload_msg->meshdata->vbo_handle.vbo->bufferName());
				glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/0, /*writeOffset=*/meshdata->vbo_handle.offset, meshdata->vbo_handle.size);

				//----------------------------- Do an on-GPU (hopefully) copy of the source index data to the new buffer at the allocated position. -----------------------------
				// index_vbo may be null in which case both index and vert data is in vert_vbo.
				glBindBuffer(GL_COPY_READ_BUFFER,  vbo->bufferName());
				glBindBuffer(GL_COPY_WRITE_BUFFER, meshdata->indices_vbo_handle.index_vbo->bufferName());
				glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/index_data_src_offset_B, /*writeOffset=*/meshdata->indices_vbo_handle.offset, meshdata->indices_vbo_handle.size);

				// Unbind
				glBindBuffer(GL_COPY_READ_BUFFER, 0);
				glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

				//----------------------------- Block until all copies have completed -----------------------------
				sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0);
				wait_ret = glClientWaitSync(sync_ob, /*wait flags=*/GL_SYNC_FLUSH_COMMANDS_BIT, /*waitDuration=*/(uint64)1.0e15);
				assert(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED);
				glDeleteSync(sync_ob); // Destroy sync object
#endif
				/*{
					const size_t uploaded_size_B = vert_data.size() + index_data.size();
					const double elapsed = timer2.elapsed();
					const double upload_speed = uploaded_size_B / elapsed;
					conPrint(doubleToStringNSigFigs(overall_run_timer.elapsed()) + ": upload_speed: " + doubleToStringNSigFigs(upload_speed * 1.0e-9) + " GB/s    uploaded in period: " + uInt64ToStringCommaSeparated(uploaded_size_B) + " B over " + doubleToStringNSigFigs(1.0e3 * elapsed) + " ms");
				}*/
			

				//----------------------------- Queue GeometryUploadedMessage to send back to client code once the batch's uploads have completed -----------------------------
				Reference<GeometryUploadedMessage> uploaded_msg = new GeometryUploadedMessage();
				uploaded_msg->meshdata = meshdata;
				uploaded_msg->user_info = upload_msg->user_info;

				batch_uploaded_msgs.push_back(uploaded_msg);


				//----------------------------- Compute stats -----------------------------
				if(0)
				{
					total_uploaded_B += vert_data.size() + index_data.size();
					uploaded_in_period_B += vert_data.size() + index_data.size();
					largest_geom_B = myMax(largest_geom_B, vert_data.size() + index_data.size());

					if(vert_data.size() + index_data.size() > 100000000)
						conPrint("big");
			
					if(timer.elapsed() > 0.5)
					{
						const double elapsed = timer.elapsed();
						const double upload_speed = uploaded_in_period_B / elapsed;
						conPrint("uploaded in period: " + uInt64ToStringCommaSeparated(uploaded_in_period_B) + " B over " + doubleToStringNSigFigs(elapsed) + " s");
						conPrint("upload_speed: " + doubleToStringNSigFigs(upload_speed * 1.0e-9) + " GB/s");

						conPrint("largest_tex_B: " + uInt64ToStringCommaSeparated(largest_tex_B));
						conPrint("largest_geom_B: " + uInt64ToStringCommaSeparated(largest_geom_B));
			
						timer.reset();
						uploaded_in_period_B = 0;
					}
				}
			}
			catch(glare::Exception& e)
			{
				const std::string err_msg = "Error while uploading geometry to GPU: " + e.what();
				out_msg_queue->enqueue(new OpenGLUploadErrorMessage(err_msg));
				num_new_resource_uploads_pending--; // This upload failed, so is no longer pending.
			}
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


AnimatedTextureUpdated* OpenGLUploadThread::allocAnimatedTextureUpdatedMessage()
{
	glare::FastPoolAllocator::AllocResult alloc_res = animated_texture_updated_allocator->alloc();

	AnimatedTextureUpdated* msg = new (alloc_res.ptr) AnimatedTextureUpdated(); // Construct with placement new
	msg->allocator = animated_texture_updated_allocator.ptr();
	msg->allocation_index = alloc_res.index;

	return msg;
}
