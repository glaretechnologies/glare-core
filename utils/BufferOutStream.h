/*=====================================================================
BufferOutStream.h
------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "RandomAccessOutStream.h"
#include "AllocatorVector.h"


/*=====================================================================
BufferOutStream
---------------
Output stream that writes to a buffer.
=====================================================================*/
class BufferOutStream final : public RandomAccessOutStream
{
public:
	BufferOutStream();
	virtual ~BufferOutStream();

	void writeUInt8(uint8 x);

	virtual void writeInt32(int32 x) override;
	virtual void writeUInt32(uint32 x) override;
	virtual void writeData(const void* data, size_t num_bytes) override;


	virtual size_t getWriteIndex() const override { return buf.size(); }
	virtual void* getWritePtrAtIndex(size_t i) override;

	void clear() { buf.resize(0); }

	glare::AllocatorVector<unsigned char, 16> buf;
};
