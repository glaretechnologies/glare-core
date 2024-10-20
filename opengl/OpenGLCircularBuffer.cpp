/*=====================================================================
OpenGLCircularBuffer.cpp
------------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "OpenGLCircularBuffer.h"


#include "IncludeOpenGL.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"


OpenGLCircularBuffer::OpenGLCircularBuffer()
{}


OpenGLCircularBuffer::~OpenGLCircularBuffer()
{}


void OpenGLCircularBuffer::init(void* buffer_, size_t size_)
{
	buffer = buffer_;
	size = size_;
	used_begin = 0;
	used_end = 0;
}


void* OpenGLCircularBuffer::getRangeForCPUWriting(size_t range_size_B)
{
	// See if we have room at the end of the buffer for this range
	assert(used_end + range_size_B <= size);



	// Wait for some regions to be done by GPU, so we don't end up with too many
#if 1
	printVar(buffer_locks.size());
	while(buffer_locks.size() > 32)
	{
		BufferLock& buffer_lock = buffer_locks.front();

		waitForSyncOb(buffer_lock.sync_ob);
		glDeleteSync(buffer_lock.sync_ob);

		// Region is no longer used, add space to free part of buffer.  Do this by advancing the used_begin index
		used_begin += buffer_lock.mRange.mLength;
		assert(used_begin <= size);
		buffer_locks.pop_front();
		if(used_begin == size)
			used_begin = 0;
	}
#endif


	if((used_end < used_begin) || ((used_end == used_begin) && buffer_locks.nonEmpty())) // If used part of buffer is wrapped:
	{
		size_t cur_free_size = used_begin - used_end;
		
		// If not enough free space, we need to wait for some buffer regions to become unused by the GPU.
		while(cur_free_size < range_size_B)
		{
			assert(buffer_locks.size() > 0);
			BufferLock& buffer_lock = buffer_locks.front();

			waitForSyncOb(buffer_lock.sync_ob);
			glDeleteSync(buffer_lock.sync_ob);

			// Region is no longer used, add space to free part of buffer.  Do this by advancing the used_begin index
			cur_free_size += buffer_lock.mRange.mLength;
			used_begin += buffer_lock.mRange.mLength;
			assert(used_begin <= size);
			buffer_locks.pop_front();
			if(used_begin == size)
				used_begin = 0;
		}

		void* write_ptr = (uint8*)buffer + used_end;
		used_end += range_size_B;
		assert(used_end <= size);
		if(used_end == size)
			used_end = 0;
		assert(used_end < size);
		return write_ptr;
	}
	else
	{
		[[maybe_unused]] const size_t free_size_at_end = size - used_end;
		assert(range_size_B <= free_size_at_end);

		void* write_ptr = (uint8*)buffer + used_end;
		used_end += range_size_B;
		assert(used_end <= size);
		if(used_end == size) // TODO: correct?
			used_end = 0;

		return write_ptr;
	}
}


static GLuint64 kOneSecondInNanoSeconds = 1000000000;


void OpenGLCircularBuffer::waitForSyncOb(GLsync sync_ob)
{
	GLbitfield waitFlags = 0;
	GLuint64 waitDuration = 0;

	while(1)
	{
		GLenum wait_ret = glClientWaitSync(sync_ob, /*wait flags=*/waitFlags, waitDuration);
		if(wait_ret == GL_ALREADY_SIGNALED || wait_ret == GL_CONDITION_SATISFIED)
			return;

		if(wait_ret == GL_WAIT_FAILED) {
			assert(!"Not sure what to do here. Probably raise an exception or something.");
			return;
		}

		// After the first time, need to start flushing, and wait for a looong time.
		waitFlags = GL_SYNC_FLUSH_COMMANDS_BIT;
		waitDuration = kOneSecondInNanoSeconds;
	}
}


void OpenGLCircularBuffer::finishedCPUWriting(void* write_ptr, size_t range_size_B)
{
	// Add a region with a fence
	const size_t offset = (uint8*)write_ptr - (uint8*)buffer;
	const BufferRange range({offset, range_size_B});

	BufferLock lock;
	lock.mRange = range;
	lock.sync_ob = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, /*flags=*/0);
	buffer_locks.push_back(lock);
}
