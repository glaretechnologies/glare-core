/*=====================================================================
BufferViewInStream.h
--------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "InStream.h"
#include "ArrayRef.h"


/*=====================================================================
BufferViewInStream
------------------
Input stream that reads from a buffer.
Just holds a pointer to the buffer.
=====================================================================*/
class BufferViewInStream : public InStream
{
public:
	BufferViewInStream(const ArrayRef<uint8> data);
	virtual ~BufferViewInStream();

	virtual int32 readInt32();
	virtual uint32 readUInt32();
	virtual void readData(void* buf, size_t num_bytes);
	virtual bool endOfStream();

	uint16 readUInt16();

	bool canReadNBytes(size_t N) const;
	void setReadIndex(size_t i);
	void advanceReadIndex(size_t n);
	size_t getReadIndex() const { return read_index; }

	size_t size() const { return data.size(); }

	const void* currentReadPtr() const { return data.data() + read_index; }

private:
	ArrayRef<uint8> data;
	size_t read_index;
};
