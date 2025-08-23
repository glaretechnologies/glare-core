/*=====================================================================
AsyncGeometryUploader.cpp
-------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "AsyncGeometryUploader.h"


#include "IncludeOpenGL.h"
#include "OpenGLMeshRenderData.h"
#include "VBO.h"
#include "OpenGLEngine.h"
#include "../utils/FileUtils.h"
#include <tracy/Tracy.hpp>


AsyncGeometryUploader::AsyncGeometryUploader()
{
}


AsyncGeometryUploader::~AsyncGeometryUploader()
{
}


void AsyncGeometryUploader::startUploadingGeometry(OpenGLMeshRenderDataRef meshdata, VBORef source_vbo, VBORef dummy_vbo,
	size_t vert_data_src_offset_B, size_t index_data_src_offset_B, size_t vert_data_size_B, size_t index_data_size_B, size_t total_geom_size_B)
{
	source_vbo->flushWholeBuffer();

	glBindBuffer(GL_COPY_READ_BUFFER,  source_vbo->bufferName());
	glBindBuffer(GL_COPY_WRITE_BUFFER, dummy_vbo->bufferName());
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/0, /*writeOffset=*/0, /*size=*/8/*total_geom_size_B*/);//source_vbo->getSize());

	// Unbind
	glBindBuffer(GL_COPY_READ_BUFFER, 0);
	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

	// Insert fence object into stream. We can query this to see if the upload of the data to the VBO has completed.
	GLsync sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0);

	//conPrint(doubleToStringNDecimalPlaces(Clock::getTimeSinceInit() * 1.0e3, 1) + " ms > Made glFenceSync ob for VBO " + toHexString((uint64)source_vbo.ptr()));

	UploadingGeometry queue_item;
	queue_item.meshdata = meshdata;
	queue_item.source_vbo = source_vbo;
	//queue_item.dummy_vbo = dummy_vbo;
	queue_item.vert_data_src_offset_B = vert_data_src_offset_B;
	queue_item.index_data_src_offset_B = index_data_src_offset_B;
	queue_item.vert_data_size_B = vert_data_size_B;
	queue_item.index_data_size_B = index_data_size_B;
	queue_item.sync_ob = sync_ob;

	uploading_geometry.push(queue_item);
}


void AsyncGeometryUploader::checkForUploadedGeometry(OpenGLEngine* opengl_engine, js::Vector<AsyncUploadedGeometryInfo, 16>& uploaded_geom_out)
{
	ZoneScoped; // Tracy profiler

	uploaded_geom_out.clear();

	while(!uploading_geometry.empty())
	{
		UploadingGeometry& front_item = uploading_geometry.front();

		GLenum wait_ret = glClientWaitSync(front_item.sync_ob, /*wait flags=*/0, /*waitDuration=*/0);
		assert(wait_ret != GL_WAIT_FAILED);
		if(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED)
		{
			//conPrint(doubleToStringNDecimalPlaces(Clock::getTimeSinceInit() * 1.0e3, 1) + " ms > UPLOADING glClientWaitSync finished with e.g. GL_CONDITION_SATISFIED for VBO " + toHexString((uint64)front_item.source_vbo.ptr()));

			//AsyncUploadedGeometryInfo uploaded_info;
			//uploaded_info.meshdata = front_item.meshdata;
			//uploaded_info.vbo = front_item.source_vbo;
			//uploaded_geom_out.push_back(uploaded_info);

			// Destroy sync object
			glDeleteSync(front_item.sync_ob);

			

			// Allocate space in vertex and index buffers
			// Pass null data pointers so no copying is done.
			opengl_engine->vert_buf_allocator->allocateBufferSpaceAndVAO(*front_item.meshdata, front_item.meshdata->vertex_spec, /*vert data=*/nullptr, /*vert data size B=*/front_item.vert_data_size_B,
				/*index data=*/nullptr, /*index data size B=*/front_item.index_data_size_B);

			//Timer timer;
			// Do an on-GPU (hopefully) copy of the source vertex data to the new buffer at the allocated position.
			glBindBuffer(GL_COPY_READ_BUFFER,  front_item.source_vbo->bufferName());
			glBindBuffer(GL_COPY_WRITE_BUFFER, front_item.meshdata->vbo_handle.vbo->bufferName());
			glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/front_item.vert_data_src_offset_B, /*writeOffset=*/front_item.meshdata->vbo_handle.offset, front_item.meshdata->vbo_handle.size);

			//conPrint("vertex buffer copy took " + timer.elapsedStringMSWIthNSigFigs(4));
			//timer.reset();

			// Do an on-GPU (hopefully) copy of the source index data to the new buffer at the allocated position.
			glBindBuffer(GL_COPY_READ_BUFFER,  front_item.source_vbo->bufferName());
			glBindBuffer(GL_COPY_WRITE_BUFFER, front_item.meshdata->indices_vbo_handle.index_vbo->bufferName());
			glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, /*readOffset=*/front_item.index_data_src_offset_B, /*writeOffset=*/front_item.meshdata->indices_vbo_handle.offset, front_item.meshdata->indices_vbo_handle.size);

			// Unbind
			glBindBuffer(GL_COPY_READ_BUFFER, 0);
			glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

			

			// Insert fence object into stream. We can query this to see if the upload of the data to the VBO has completed.
			GLsync copy_sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0);

			//front_item

			front_item.sync_ob = copy_sync_ob;
			copying_geometry.push(front_item);

			//conPrint("index buffer copy took " + timer.elapsedStringMSWIthNSigFigs(4));
			//timer.reset();
			uploading_geometry.pop(); // NOTE: this has to go after all usage of front_item.
			//conPrint("loading_geometry.pop() took " + timer.elapsedStringMSWIthNSigFigs(4));
		}
		else
		{
			// Else geometry upload is not done yet.
			break;
		}
	}


	// Check copying_geometry queue
	if(!copying_geometry.empty())
	{
		UploadingGeometry& front_item = copying_geometry.front();

		GLenum wait_ret = glClientWaitSync(front_item.sync_ob, /*wait flags=*/0, /*waitDuration=*/0);
		assert(wait_ret != GL_WAIT_FAILED);
		if(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED)
		{
			//conPrint(doubleToStringNDecimalPlaces(Clock::getTimeSinceInit() * 1.0e3, 1) + " ms > COPYING glClientWaitSync finished with e.g. GL_CONDITION_SATISFIED for VBO " + toHexString((uint64)front_item.source_vbo.ptr()));

			AsyncUploadedGeometryInfo uploaded_info;
			uploaded_info.meshdata = front_item.meshdata;
			uploaded_info.vbo = front_item.source_vbo;
			uploaded_geom_out.push_back(uploaded_info);

			// Destroy sync object
			glDeleteSync(front_item.sync_ob);

			//conPrint("index buffer copy took " + timer.elapsedStringMSWIthNSigFigs(4));
			//timer.reset();
			copying_geometry.pop(); // NOTE: this has to go after all usage of front_item.
			//conPrint("loading_geometry.pop() took " + timer.elapsedStringMSWIthNSigFigs(4));
		}
	}
}
