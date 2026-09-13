/*=====================================================================
VertexBufferAllocator.cpp
-------------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "VertexBufferAllocator.h"


#include "IncludeOpenGL.h"
#include "OpenGLMeshRenderData.h"
#include "../graphics/BatchedMesh.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"
#include "../utils/RuntimeCheck.h"
#include "../utils/ConPrint.h"
#include "../utils/Vector.h"
#include "../utils/Lock.h"
#include "../maths/mathstypes.h"
#include <tracy/Tracy.hpp>


BlockHandle::~BlockHandle()
{
	vert_allocator->freeBlock(block);
}


// The maximum number of frames worth of fences we keep around.  If the GPU gets further behind than this we do a
// blocking wait on the oldest fence, rather than letting the quarantine (and the fence list) grow without bound.
static const size_t MAX_IN_FLIGHT_FRAME_FENCES = 8;

// Upper bound on how long we will block waiting for a fence, in nanoseconds.  We should never come close to this; it's
// just here so that a lost or never-flushed fence degrades into (safe) blocks staying quarantined, rather than a hang.
static const uint64 MAX_FENCE_WAIT_NS = 1000000000ull; // 1s


VertexBufferAllocator::VertexBufferAllocator(bool use_grouped_vbo_allocator_)
:	quarantined_size_B(0),
	current_frame_num(1), // Start at 1 so that last_retired_frame_num = 0 means 'no frame has been retired yet'.
	last_retired_frame_num(0),
	use_VBO_size_B(64 * 1024 * 1024), // Default VBO size to use, will be overridden in OpenGLEngine::initialise() though.
	use_grouped_vbo_allocator(use_grouped_vbo_allocator_)
{
	//conPrint("VertexBufferAllocator::VertexBufferAllocator()");
}


VertexBufferAllocator::~VertexBufferAllocator()
{
	//conPrint("VertexBufferAllocator::~VertexBufferAllocator()");

	// Return any still-quarantined blocks to their allocators, so that ~BestFitAllocator doesn't consider them to still
	// be in use.  Nothing is reading from the buffers any more at this point.
	// NOTE: we deliberately don't call glDeleteSync() on the remaining fences here, since the OpenGL context may already
	// be gone.  Destroying the context frees them.
	while(pending_free_blocks.nonEmpty())
	{
		glare::BestFitAllocator::BlockInfo* block = pending_free_blocks.front().block;

		glare::BestFitAllocator* allocator = block->allocator;
		if(allocator)
			allocator->free(block);

		pending_free_blocks.pop_front();
	}
	quarantined_size_B = 0;
}


void VertexBufferAllocator::allocateBufferSpaceAndVAO(OpenGLMeshRenderData& mesh_data_in_out, const VertexSpec& vertex_spec, const void* vert_data, size_t vert_data_size_B, 
	const void* index_data, size_t index_data_size_B)
{
	ZoneScoped; // Tracy profiler
	Lock lock(mutex);

	mesh_data_in_out.vbo_handle = allocateVertexDataSpace(vertex_spec.vertStride(), vert_data, vert_data_size_B);

	mesh_data_in_out.indices_vbo_handle = allocateIndexDataSpace(index_data, index_data_size_B);

	getOrCreateAndAssignVAOForMesh(mesh_data_in_out, vertex_spec);
}


void VertexBufferAllocator::freeBlock(glare::BestFitAllocator::BlockInfo* block)
{
	ZoneScoped; // Tracy profiler
	Lock lock(mutex);

	assert(block->allocator); // Should be non-null unless the BestFitAllocator has been destroyed (internal logic error)

	// Don't return the block to the allocator yet: the GPU may not have executed writes to, or reads from, this region
	// yet.  See the comment in VertexBufferAllocator.h.
	// NOTE: current_frame_num only ever increases, and is only changed in frameEnded() with the mutex held, so appending
	// here keeps pending_free_blocks sorted by freed_in_frame.  releaseRetiredBlocks() relies on that.
	assert(pending_free_blocks.empty() || (pending_free_blocks.back().freed_in_frame <= current_frame_num));

	PendingFree pending_free;
	pending_free.block = block;
	pending_free.block_size = block->size;
	pending_free.freed_in_frame = current_frame_num;
	pending_free_blocks.push_back(pending_free);

	quarantined_size_B += pending_free.block_size;
}


// pending_free_blocks is sorted by freed_in_frame (see freeBlock()), so the blocks we can release are always at the
// front of the queue, and we can stop as soon as we reach one whose frame hasn't been retired yet.
// mutex must be held.
void VertexBufferAllocator::releaseRetiredBlocks()
{
	while(pending_free_blocks.nonEmpty() && (pending_free_blocks.front().freed_in_frame <= last_retired_frame_num))
	{
		const PendingFree pending_free = pending_free_blocks.front();

		glare::BestFitAllocator* allocator = pending_free.block->allocator;
		assert(allocator); // Should be non-null unless the BestFitAllocator has been destroyed (internal logic error)
		if(allocator)
			allocator->free(pending_free.block); // NOTE: may destroy the BlockInfo, so don't touch pending_free.block after this.

		assert(quarantined_size_B >= pending_free.block_size);
		quarantined_size_B -= pending_free.block_size;

		pending_free_blocks.pop_front();
	}
}


// Blocks until the oldest fence has signalled, then removes it, updating last_retired_frame_num.
// mutex must be held.
void VertexBufferAllocator::waitForAndRemoveOldestFrameFence()
{
	ZoneScoped; // Tracy profiler
	assert(frame_fences.nonEmpty());

	const FrameFence oldest = frame_fences.front();

	const GLenum wait_ret = glClientWaitSync((GLsync)oldest.sync_ob, /*wait flags=*/GL_SYNC_FLUSH_COMMANDS_BIT, /*waitDuration=*/MAX_FENCE_WAIT_NS);
	if(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED)
	{
		last_retired_frame_num = oldest.frame_num;
		glDeleteSync((GLsync)oldest.sync_ob);
		frame_fences.pop_front();
	}
	// Else the wait timed out or failed: leave the fence in place and try again next frame.  The blocks it covers just
	// stay quarantined for longer, which is safe.
}


// Sees how far the GPU has got, without blocking.  Fences signal in the order they were created, so we can stop at the
// first one that hasn't signalled yet.
// May be called from a thread whose OpenGL context is not the one the fences were created in - sync objects are shared
// between contexts in a share group, so polling them from another context is fine.
// mutex must be held.
void VertexBufferAllocator::pollFrameFences()
{
	while(frame_fences.nonEmpty())
	{
		const FrameFence oldest = frame_fences.front();

		const GLenum wait_ret = glClientWaitSync((GLsync)oldest.sync_ob, /*wait flags=*/0, /*waitDuration=*/0); // Just poll, don't block.
		if(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED)
		{
			last_retired_frame_num = oldest.frame_num;
			glDeleteSync((GLsync)oldest.sync_ob);
			frame_fences.pop_front();
		}
		else
			break;
	}
}


void VertexBufferAllocator::frameEnded()
{
	ZoneScoped; // Tracy profiler
	Lock lock(mutex);

	pollFrameFences();

	// If the GPU has fallen a long way behind, block rather than letting the quarantine grow without bound.
	while(frame_fences.size() >= MAX_IN_FLIGHT_FRAME_FENCES)
	{
		const uint64 prev_retired_frame_num = last_retired_frame_num;

		waitForAndRemoveOldestFrameFence();

		if(last_retired_frame_num == prev_retired_frame_num) // If the wait timed out, don't spin.
			break;
	}

	releaseRetiredBlocks();

	// Insert a fence covering everything issued in this context during the frame that has just ended, including any
	// writes to the blocks that were freed during it.
	// There's no point doing this if nothing is quarantined.  Note that a block freed during a frame we skipped is still
	// handled correctly: it just waits for the next fence we do create, which has a higher frame number and covers the
	// earlier frame's commands as well.
	if(pending_free_blocks.nonEmpty())
	{
		FrameFence new_fence;
		new_fence.frame_num = current_frame_num;
		new_fence.sync_ob = (void*)glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0); // Returns a non-zero name on success.
		if(new_fence.sync_ob)
			frame_fences.push_back(new_fence);
	}

	current_frame_num++;
}


void VertexBufferAllocator::waitForGPUAndReleaseAllPendingFrees()
{
	ZoneScoped; // Tracy profiler
	Lock lock(mutex);

	if(pending_free_blocks.empty())
		return;

	// Blocks freed during the current frame aren't covered by any fence yet, so insert one now.  Since we're on the main
	// thread, this covers every command issued in the main context so far, including the writes to those blocks.
	const GLsync sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0); // Returns a non-zero name on success.
	if(sync_ob)
	{
		FrameFence new_fence;
		new_fence.frame_num = current_frame_num; // frameEnded() hasn't run yet this frame, so existing fences are all older than this.
		new_fence.sync_ob = (void*)sync_ob;
		frame_fences.push_back(new_fence);
	}

	// Wait for all outstanding fences, oldest first.
	while(frame_fences.nonEmpty())
	{
		const uint64 prev_retired_frame_num = last_retired_frame_num;

		waitForAndRemoveOldestFrameFence();

		if(last_retired_frame_num == prev_retired_frame_num) // If the wait timed out, give up rather than spinning.
			break;
	}

	releaseRetiredBlocks();
}


void VertexBufferAllocator::getOrCreateAndAssignVAOForMesh(OpenGLMeshRenderData& mesh_data_in_out, const VertexSpec& vertex_spec)
{
	ZoneScoped; // Tracy profiler
	Lock lock(mutex);

	vertex_spec.checkValid();

#if DO_INDIVIDUAL_VAO_ALLOC
	mesh_data_in_out.individual_vao = new VAO(mesh_data_in_out.vbo_handle.vbo, mesh_data_in_out.indices_vbo_handle.index_vbo, vertex_spec);
#else

	VAOKey key;
	key.vertex_spec = vertex_spec;

	auto res = vao_map.find(key);
	size_t use_vao_data_index;
	if(res == vao_map.end())
	{
		// This is a new vertex specification we don't have a VAO for.

		use_vao_data_index = vao_data.size();

		VAOData new_data;
		new_data.vao = new VAO(vertex_spec); // Allocate new VAO
		vao_data.push_back(new_data);

		vao_map.insert(std::make_pair(key, (int)use_vao_data_index));
	}
	else
		use_vao_data_index = res->second;

	assert(vao_data[use_vao_data_index].vao->vertex_spec == vertex_spec);

	mesh_data_in_out.vao_data_index = (uint32)use_vao_data_index;

#endif // end if !DO_INDIVIDUAL_VAO_ALLOC
}


VertBufAllocationHandle VertexBufferAllocator::allocateVertexDataSpace(size_t vert_stride, const void* vbo_data, size_t size)
{
	ZoneScoped; // Tracy profiler
	Lock lock(mutex);

#if DO_INDIVIDUAL_VAO_ALLOC
	// This is for the Mac, that can't easily do VAO sharing due to having to use glVertexAttribPointer().
	VertBufAllocationHandle handle;
	handle.vbo = new VBO(vbo_data, size);
	handle.vbo_id = 0;
	handle.offset = 0;
	handle.size = size;
	handle.base_vertex = 0;
	return handle;

#else // else if !DO_INDIVIDUAL_VAO_ALLOC:

	if(!use_grouped_vbo_allocator)
	{
		// Not using best-fit allocator, just allocate a new VBO for each individual vertex data buffer:
		VertBufAllocationHandle handle;
		handle.vbo = new VBO(vbo_data, size);
		handle.vbo_id = 0;
		handle.offset = 0;
		handle.size = size;
		handle.base_vertex = 0;
		return handle;
	}
	else
	{
		//----------------------------- Allocate from VBO -------------------------------
		// Iterate over existing VBOs to see if we can allocate in that VBO.
		// If that fails on the first attempt, reclaim any quarantined blocks the GPU has finished with and try again,
		// before falling back to creating another large VBO.
		VBORef used_vbo;
		size_t used_vbo_id = 0;
		glare::BestFitAllocator::BlockInfo* used_block = NULL;
		for(int attempt=0; attempt<2; ++attempt)
		{
			for(size_t i=0; i<vert_vbos.size(); ++i)
			{
				glare::BestFitAllocator::BlockInfo* block = vert_vbos[i].allocator->alloc(size, vert_stride);
				if(block)
				{
					used_vbo = vert_vbos[i].vbo;
					used_vbo_id = i;
					used_block = block;
					break;
				}
			}

			if(used_vbo.nonNull())
				break;

			// Reclaim any quarantined blocks the GPU has already finished with, and try once more before falling back to
			// creating another large VBO.
			// NOTE: this deliberately only polls - it must not block waiting for the GPU, since this can be running on the
			// main thread, where a sync would show up as a stutter.  So blocks freed in the last frame or two just stay
			// quarantined, and we allocate a new VBO rather than stalling, exactly as we did before the quarantine existed.
			if(attempt == 0)
			{
				pollFrameFences();
				releaseRetiredBlocks();
			}
		}

		if(used_vbo.isNull()) // If failed to allocate from existing VBOs:
		{
			// conPrint("============================Creating new VBO! (size: " + toString(this->use_VBO_size_B) + " B)");

			// Create a new VBO and allocator, add to list of VBOs
			VBOAndAllocator vbo_and_alloc;
			vbo_and_alloc.vbo = new VBO(NULL, this->use_VBO_size_B, /*buffer_type=*/GL_ARRAY_BUFFER, /*usage=*/GL_DYNAMIC_DRAW); 
			// Since we will be updating chunks of the VBO, we will make it GL_DYNAMIC_DRAW instead of GL_STATIC_DRAW, otherwise will get OpenGL error/warning messages about updating a GL_STATIC_DRAW buffer.
			vbo_and_alloc.allocator = new glare::BestFitAllocator(this->use_VBO_size_B);
			vbo_and_alloc.allocator->name = "VBO allocator";
			vert_vbos.push_back(vbo_and_alloc);

			// Allocate from the new VBO
			glare::BestFitAllocator::BlockInfo* block = vbo_and_alloc.allocator->alloc(size, vert_stride);
			if(block)
			{
				used_vbo = vbo_and_alloc.vbo;
				used_vbo_id = vert_vbos.size() - 1;
				used_block = block;
			}
			else
				throw glare::Exception("Failed to allocate mem block of size " + toString(size) + " B from new VBO.");
		}
		//-------------------------------------------------------------------------------


		if(vbo_data)
			used_vbo->updateData(used_block->aligned_offset, vbo_data, size);


		runtimeCheck(used_block->aligned_offset % vert_stride == 0);
		const int base_vertex = (int)(used_block->aligned_offset / vert_stride);
		runtimeCheck((int64)base_vertex * (int64)vert_stride == (int64)used_block->aligned_offset);

		VertBufAllocationHandle handle;
		handle.block_handle = new BlockHandle(this, used_block);
		handle.vbo = used_vbo;
		handle.vbo_id = used_vbo_id;
		handle.offset = used_block->aligned_offset;
		handle.size = size;
		handle.base_vertex = base_vertex;

		return handle;
	}
#endif // end if !DO_INDIVIDUAL_VAO_ALLOC
}


IndexBufAllocationHandle VertexBufferAllocator::allocateIndexDataSpace(const void* data, size_t size)
{
	ZoneScoped; // Tracy profiler
	Lock lock(mutex);

#if DO_INDIVIDUAL_VAO_ALLOC

	IndexBufAllocationHandle handle;
	handle.index_vbo = new VBO(data, size, GL_ELEMENT_ARRAY_BUFFER);
	handle.vbo_id = 0;
	handle.offset = 0;
	handle.size = size;
	return handle;

#else
	if(!use_grouped_vbo_allocator)
	{
		// Not using best-fit allocator, just allocate a new VBO for each individual vertex data buffer:
		IndexBufAllocationHandle handle;
		handle.offset = 0;
		handle.size = size;
		handle.index_vbo = new VBO(data, size, GL_ELEMENT_ARRAY_BUFFER);
		handle.vbo_id = 0;
		return handle;
	}
	else
	{
		//----------------------------- Allocate from VBO -------------------------------
		// Iterate over existing VBOs to see if we can allocate in that VBO.
		// If that fails on the first attempt, reclaim any quarantined blocks the GPU has finished with and try again,
		// before falling back to creating another large VBO.
		VBORef used_vbo;
		size_t used_vbo_id = 0;
		glare::BestFitAllocator::BlockInfo* used_block = NULL;
		for(int attempt=0; attempt<2; ++attempt)
		{
			for(size_t i=0; i<index_vbos.size(); ++i)
			{
				glare::BestFitAllocator::BlockInfo* block = index_vbos[i].allocator->alloc(size, 4);
				if(block)
				{
					used_vbo = index_vbos[i].vbo;
					used_vbo_id = i;
					used_block = block;
					break;
				}
			}

			if(used_vbo.nonNull())
				break;

			// Reclaim any quarantined blocks the GPU has already finished with, and try once more before falling back to
			// creating another large VBO.
			// NOTE: this deliberately only polls - it must not block waiting for the GPU, since this can be running on the
			// main thread, where a sync would show up as a stutter.  So blocks freed in the last frame or two just stay
			// quarantined, and we allocate a new VBO rather than stalling, exactly as we did before the quarantine existed.
			if(attempt == 0)
			{
				pollFrameFences();
				releaseRetiredBlocks();
			}
		}

		if(used_vbo.isNull()) // If failed to allocate from existing VBOs:
		{
			// conPrint("============================Creating new index VBO!  (size: " + toString(this->use_VBO_size_B) + " B)");

			// Create a new VBO and allocator, add to list of VBOs
			VBOAndAllocator vbo_and_alloc;
			vbo_and_alloc.vbo = new VBO(NULL, this->use_VBO_size_B, GL_ELEMENT_ARRAY_BUFFER, /*usage=*/GL_DYNAMIC_DRAW);
			vbo_and_alloc.allocator = new glare::BestFitAllocator(this->use_VBO_size_B);
			vbo_and_alloc.allocator->name = "index VBO allocator";
			index_vbos.push_back(vbo_and_alloc);

			// Allocate from the new VBO
			glare::BestFitAllocator::BlockInfo* block = vbo_and_alloc.allocator->alloc(size, 4);
			if(block)
			{
				used_vbo = vbo_and_alloc.vbo;
				used_vbo_id = index_vbos.size() - 1;
				used_block = block;
			}
			else
				throw glare::Exception("Failed to allocate mem block of size " + toString(size) + " B from new index VBO");
		}
		//-------------------------------------------------------------------------------


		IndexBufAllocationHandle handle;
		handle.block_handle = new BlockHandle(this, used_block);
		runtimeCheck(handle.block_handle->block->aligned_offset % 4 == 0);
		handle.offset = handle.block_handle->block->aligned_offset;
		handle.size = size;
		handle.index_vbo = used_vbo;// indices_vbo;
		handle.vbo_id = used_vbo_id;

		if(data != NULL)
			used_vbo->updateData(handle.block_handle->block->aligned_offset, data, size);

		return handle;
	}
#endif
}


std::string VertexBufferAllocator::getDiagnostics() const
{
	Lock lock(mutex);

	std::string s;
	s += "VAOs: " + toString(vao_data.size()) + "\n";
	s += "use_VBO_size: " + getNiceByteSize(use_VBO_size_B) + "\n";
	s += "Vert VBOs: " + toString(vert_vbos.size()) + " (" + getNiceByteSize(use_VBO_size_B * vert_vbos.size()) + ")\n";
	s += "Index VBOs: " + toString(index_vbos.size()) + " (" + getNiceByteSize(use_VBO_size_B * index_vbos.size()) + ")\n";

	s += "Quarantined blocks awaiting GPU: " + toString(pending_free_blocks.size()) + " (" + getNiceByteSize(quarantined_size_B) + "), frames in flight: " + toString(frame_fences.size()) + "\n";

	return s;
}
