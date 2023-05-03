/*=====================================================================
OpenGLCircularBuffer.h
----------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "BasicOpenGLTypes.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/CircularBuffer.h"


struct __GLsync;
typedef struct __GLsync *GLsync;

struct BufferRange
{
	size_t mStartOffset;
	size_t mLength;

	bool overlaps(const BufferRange& _rhs) const {
		return mStartOffset < (_rhs.mStartOffset + _rhs.mLength)
			&& _rhs.mStartOffset < (mStartOffset + mLength);
	}
};


struct BufferLock
{
	BufferRange mRange;
	GLsync sync_ob;
};


/*=====================================================================
OpenGLCircularBuffer
--------------------
A kind of circular view, with memory fencing/locking on an underlying 
mapped OpenGL Buffer.
Loosly based on some code from https://github.com/nvMcJohn/apitest
=====================================================================*/
class OpenGLCircularBuffer
{
public:
	OpenGLCircularBuffer();
	~OpenGLCircularBuffer();

	void init(void* buffer, size_t size);

	void* getRangeForCPUWriting(size_t range_size_B);
	void finishedCPUWriting(void* write_ptr, size_t range_size_B);

	size_t contiguousBytesRemaining() const { return size - used_end; }

private:
	GLARE_DISABLE_COPY(OpenGLCircularBuffer);
	void waitForSyncOb(GLsync sync_ob);
public:
	size_t used_begin;
	size_t used_end;
	void* buffer;
	size_t size;

	CircularBuffer<BufferLock> buffer_locks;
};
