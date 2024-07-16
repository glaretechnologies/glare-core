/*=====================================================================
FileInStream.h
--------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "RandomAccessInStream.h"
#include "MemMappedFile.h"
#include <string>


/*=====================================================================
FileInStream
------------

=====================================================================*/
class FileInStream final : public RandomAccessInStream
{
public:
	FileInStream(const std::string& path); // Throws glare::Exception on failure.
	~FileInStream();

	virtual int32 readInt32();
	virtual uint32 readUInt32();
	virtual void readData(void* buf, size_t num_bytes);
	virtual bool endOfStream();

	uint16 readUInt16();

	const void* fileData() const { return file.fileData(); } // Returns pointer to file data.  NOTE: This pointer will be the null pointer if the file size is zero.
	size_t fileSize() const { return file.fileSize(); }

	virtual size_t size() const { return file.fileSize(); }

	virtual bool canReadNBytes(size_t N) const;
	virtual void setReadIndex(size_t i);
	virtual size_t getReadIndex() const { return read_index; }
	virtual void advanceReadIndex(size_t n);

	virtual const void* currentReadPtr() const { return (const uint8*)file.fileData() + read_index; }

private:
	MemMappedFile file;
	size_t read_index;
};
