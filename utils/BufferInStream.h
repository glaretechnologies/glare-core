/*=====================================================================
BufferInStream.h
----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "RandomAccessInStream.h"
#include "Vector.h"
#include "ArrayRef.h"
#include <vector>


/*=====================================================================
BufferInStream
--------------
Input stream that reads from a buffer
=====================================================================*/
class BufferInStream : public RandomAccessInStream
{
public:
	BufferInStream();
	BufferInStream(const std::string& s);
	BufferInStream(const std::vector<unsigned char>& buf);
	BufferInStream(const ArrayRef<unsigned char>& data);
	virtual ~BufferInStream();

	void clear(); // Resizes buffer to zero, resets read_index to zero.

	virtual int32 readInt32();
	virtual uint32 readUInt32();
	virtual void readData(void* buf, size_t num_bytes);
	virtual bool endOfStream();

	virtual bool canReadNBytes(size_t N) const;
	virtual void setReadIndex(size_t i);
	virtual void advanceReadIndex(size_t n);
	virtual size_t getReadIndex() const { return read_index; }

	virtual size_t size() const { return buf.size(); }

	              void* currentReadPtr()       { return buf.data() + read_index; }
	virtual const void* currentReadPtr() const { return buf.data() + read_index; }

	js::Vector<unsigned char, 16> buf;
	size_t read_index;
};
