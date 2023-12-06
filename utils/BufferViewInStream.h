/*=====================================================================
BufferViewInStream.h
--------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "RandomAccessInStream.h"
#include "ArrayRef.h"


/*=====================================================================
BufferViewInStream
------------------
Input stream that reads from a buffer.
Just holds a pointer to the buffer.
=====================================================================*/
class BufferViewInStream : public RandomAccessInStream
{
public:
	BufferViewInStream(const ArrayRef<uint8> data);
	virtual ~BufferViewInStream();

	virtual int32 readInt32() override;
	virtual uint32 readUInt32() override;
	virtual void readData(void* buf, size_t num_bytes) override;
	virtual bool endOfStream() override;

	uint16 readUInt16();

	// RandomAccessInStream interface:
	virtual bool canReadNBytes(size_t N) const override;
	virtual void setReadIndex(size_t i) override;
	virtual void advanceReadIndex(size_t n) override;
	virtual size_t getReadIndex() const override { return read_index; }

	virtual size_t size() const override { return data.size(); }

	virtual const void* currentReadPtr() const override { return data.data() + read_index; }

private:
	ArrayRef<uint8> data;
	size_t read_index;
};
