/*=====================================================================
FileInStream.h
--------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "InStream.h"
#include "MemMappedFile.h"
#include <string>


/*=====================================================================
FileInStream
------------

=====================================================================*/
class FileInStream : public InStream
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

	bool canReadNBytes(size_t N) const;
	void setReadIndex(size_t i);
	size_t getReadIndex() const { return read_index; }
	void advanceReadIndex(size_t n);

	const void* currentReadPtr() const { return (const uint8*)file.fileData() + read_index; }

private:
	MemMappedFile file;
	size_t read_index;
};
