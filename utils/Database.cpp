/*=====================================================================
Database.cpp
------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "Database.h"


#include "FileUtils.h"
#include "FileInStream.h"
#include "FileOutStream.h"
#include "Exception.h"
#include "StringUtils.h"
#include "../maths/mathstypes.h"



/*

keys are uint64s

Index (built at load time)
--------------------------
key -> record byte offset



Record layout on disk
----------------------
key (uint64), invalid key if record is empty
record len (uint32)   (len = std::numeric_limits<uint32>::max() if record has been deleted, otherwise have len <= capacity)
record capacity (uint32)
seq_num (uint32)
data  (array of capacity bytes)



Updating a record:
-----------------
arguments: key, and new data buffer (with known size)


Lookup record by key from index.

If record len >= new data buffer size:
	overwrite existing record
else:
	read seq num from existing record
	Find a new empty record with sufficient size
	write into new record with seq_num + 1


Storing a new object:
--------------------
Currently writes a new record at the end of the existing records.

Todo:
Find a new empty record with sufficient size
write new record with seq_num 0


Deleting an object
--------------------
Lookup record by key
Set record len to std::numeric_limits<uint32>::max().
*/


Database::Database()
:	file_in(NULL), file_out(NULL), next_unused_key(0), append_offset(std::numeric_limits<size_t>::max())
{}


Database::~Database()
{
	delete file_in;
	delete file_out;
}


static const uint32 DATABASE_MAGIC_NUMBER = 287173871;
static const uint32 DATABASE_SERIALISATION_VERSION = 1;


void Database::makeOrClearDatabase(const std::string& path)
{
	FileOutStream file(path, std::ios::binary | std::ios::trunc);

	file.writeUInt32(DATABASE_MAGIC_NUMBER);
	file.writeUInt32(DATABASE_SERIALISATION_VERSION);
}


void Database::openAndMakeOrClearDatabase(const std::string& path)
{
	this->db_path = path;

	FileOutStream file(path, std::ios::binary | std::ios::trunc);

	file.writeUInt32(DATABASE_MAGIC_NUMBER);
	file.writeUInt32(DATABASE_SERIALISATION_VERSION);

	this->append_offset = sizeof(uint32)*2;
}


void Database::startReadingFromDisk(const std::string& path)
{
	this->db_path = path;

	try
	{
		file_in = new FileInStream(path);

		// Read magic number
		const uint32 magic_num = file_in->readUInt32();
		if(magic_num != DATABASE_MAGIC_NUMBER)
			throw glare::Exception("invalid magic number: not a valid database file.");
		
		// Read version
		const uint32 version = file_in->readUInt32();
		if(version != DATABASE_SERIALISATION_VERSION)
			throw glare::Exception("invalid version: " + toString(version));

		uint64 max_used_key = 0;
		while(!file_in->endOfStream())
		{
			const size_t record_offset = file_in->getReadIndex();

			// Read key
			DatabaseKey key;
			key.val = file_in->readUInt64();

			// Read record len
			const uint32 len      = file_in->readUInt32();
			const uint32 capacity = file_in->readUInt32();
			const uint32 seq_num  = file_in->readUInt32();

			if(!file_in->canReadNBytes(capacity))
				throw glare::Exception("Invalid file capacity, went past end of file.");

			// Advance past this record data.
			const size_t new_unaligned_index = file_in->getReadIndex() + capacity;
			const size_t new_read_index = Maths::roundUpToMultipleOfPowerOf2(new_unaligned_index, (size_t)4);
			file_in->setReadIndex(new_read_index); // Skip over data

			// Add it to the index
			RecordInfo info;
			info.offset = record_offset;
			info.len = len;
			info.capacity = capacity;
			info.seq_num = seq_num;

			if(len == std::numeric_limits<uint32>::max()) // If an invalid length, this means the record is deleted
			{
				auto res = key_to_info_map.find(key);
				if(res != key_to_info_map.end())
				{
					if(seq_num > res->second.seq_num)
					{
						// This record has a greater sequence num, overwrite in index.  Note that the length will be invalid
						key_to_info_map[key] = info;
					}
				}
			}
			else
			{
				if(len > capacity)
					throw glare::Exception("Invalid length, was > capacity.");

				auto res = key_to_info_map.find(key);
				if(res == key_to_info_map.end()) // If not already present in map:
				{
					key_to_info_map.insert(std::make_pair(key, info));
				}
				else
				{
					if(seq_num > res->second.seq_num)
					{
						// This record has a greater sequence num, overwrite in index.
						key_to_info_map[key] = info;
					}
				}
			}

			max_used_key = myMax(max_used_key, key.val);
		}

		append_offset = file_in->getReadIndex();
		next_unused_key = max_used_key + 1;
	}
	catch(glare::Exception& e)
	{
		throw glare::Exception("Error while reading database from '" + path + "': " + e.what());
	}
}


void Database::finishReadingFromDisk()
{
	delete file_in;
	file_in = NULL;
}


void Database::removeOldRecordsOnDisk(const std::string& path)
{
	assert(file_in == NULL);
	assert(file_out == NULL);

	const std::string temp_path = path + "_temp";
	{
		// Write compacted database to a temp file
		FileOutStream temp_file_out(temp_path, std::ios::binary | std::ios::out);

		// Write database header
		temp_file_out.writeUInt32(DATABASE_MAGIC_NUMBER);
		temp_file_out.writeUInt32(DATABASE_SERIALISATION_VERSION);

		startReadingFromDisk(path);
		for(auto it = getRecordMap().begin(); it != getRecordMap().end(); ++it)
		{
			const Database::RecordInfo& record = it->second;
			if(record.isRecordValid())
			{
				const uint8* data = getInitialRecordData(record);
				const DatabaseKey key = it->first;
				const uint32 use_capacity = Maths::roundUpToMultipleOfPowerOf2<uint32>(record.len + 64, 4);
				const uint32 use_seq_num = 0;

				temp_buf.resizeNoCopy(sizeof(uint64) + sizeof(uint32) * 3 + use_capacity); // Resize temp_buf to make room for record header and capacity

				std::memcpy(&temp_buf[0], &key.val, sizeof(uint64));
				std::memcpy(&temp_buf[8], &record.len, sizeof(uint32)); // len
				std::memcpy(&temp_buf[12], &use_capacity, sizeof(uint32)); // capacity
				std::memcpy(&temp_buf[16], &use_seq_num, sizeof(uint32)); // seq_num

				if(record.len > 0)
					std::memcpy(&temp_buf[20], data, record.len);
				std::memset(&temp_buf[20] + record.len, 0, use_capacity - record.len); // Zero out unused part of buffer.

				temp_file_out.writeData(temp_buf.data(), temp_buf.size());
			}
		}
		finishReadingFromDisk();
	}

	// Replace main database file with our temp file
	FileUtils::moveFile(/*src path=*/temp_path, /* dest path=*/path);
}


DatabaseKey Database::allocUnusedKey()
{
	return DatabaseKey(next_unused_key++);
}


void Database::allocRecordSpace(uint32 data_size, size_t& offset_out, uint32& capacity_out)
{
	//TEMP: just alloc at end of DB for now.
	offset_out = this->append_offset;

	const uint32 capacity = Maths::roundUpToMultipleOfPowerOf2(data_size + 64, (uint32)4);
	capacity_out = capacity;

	const size_t record_size = sizeof(uint64) + sizeof(uint32) * 3 + capacity; // Record size including header.

	this->append_offset = this->append_offset + record_size;
}

//
//void Database::writeDataToNewRecord(RecordInfo& info, const DatabaseKey& key, ArrayRef<uint8> data)
//{
//	size_t new_record_offset;
//	uint32 new_record_capacity;
//	allocRecordSpace((uint32)data.size(), new_record_offset, new_record_capacity);
//
//	info.offset = new_record_offset;
//	info.len = (uint32)data.size();
//	info.capacity = new_record_capacity;
//	info.seq_num++;
//
//	temp_buf.resizeNoCopy(sizeof(uint64) + sizeof(uint32) * 3 + new_record_capacity); // Resize temp_buf to make room for record header and capacity
//
//	std::memcpy(&temp_buf[0], &key.val, sizeof(uint64));
//	std::memcpy(&temp_buf[8], &info.len, sizeof(uint32)); // len
//	std::memcpy(&temp_buf[12], &info.capacity, sizeof(uint32)); // capacity
//	std::memcpy(&temp_buf[16], &info.seq_num, sizeof(uint32)); // seq_num
//
//	std::memcpy(&temp_buf[20], data.data(), data.size());
//	std::memset(&temp_buf[20] + data.size(), 0, new_record_capacity - data.size()); // Zero out unused part of buffer.
//
//	// Update the record in the DB file
//	FileOutStream file(db_path, std::ios::binary | std::ios::in | std::ios::out);
//	file.seek(new_record_offset); // Skip past key, len, capacity, seq_num
//	file.writeData(temp_buf.data(), temp_buf.size());
//}


void Database::updateRecord(const DatabaseKey& key, ArrayRef<uint8> data)
{
	if(data.size() >= (size_t)std::numeric_limits<uint32>::max() - 1) // Data size must be 32-bit, and also != std::numeric_limits<uint32>::max(), which we use as a sentinel value.
		throw glare::Exception("data too large.");

	if(file_out == NULL)
		file_out = new FileOutStream(db_path, std::ios::binary | std::ios::in | std::ios::out); // Although we are not actually doing reads, std::ios::in seems to be necessary or we end up with zeros in the file after updating.

	auto res = key_to_info_map.find(key);
	if(res != key_to_info_map.end())
	{
		// If a record for this key is already present in the DB:
		RecordInfo& info = res->second;

		// If there is enough capacity to store it in the current record:
		if(info.capacity >= data.size())
		{
			// Overwrite existing record
			info.len = (uint32)data.size();

			temp_buf.resizeNoCopy(sizeof(uint64) + sizeof(uint32) * 3 + data.size()); // Resize temp_buf to make room for record header and data

			std::memcpy(&temp_buf[0], &key.val, sizeof(uint64));
			std::memcpy(&temp_buf[8], &info.len, sizeof(uint32)); // len
			std::memcpy(&temp_buf[12], &info.capacity, sizeof(uint32)); // capacity
			std::memcpy(&temp_buf[16], &info.seq_num, sizeof(uint32)); // seq_num

			if(data.size() > 0)
				std::memcpy(&temp_buf[20], data.data(), data.size());

			// Update the record in the DB file
			file_out->seek(info.offset);
			file_out->writeData(temp_buf.data(), temp_buf.size());

			// TODO: loop and try a few times on write failure?
		}
		else
		{
			// Else not enough capacity in existing record, store in new record

			size_t new_record_offset;
			uint32 new_record_capacity;
			allocRecordSpace((uint32)data.size(), new_record_offset, new_record_capacity);
			
			info.offset = new_record_offset;
			info.len = (uint32)data.size();
			info.capacity = new_record_capacity;
			info.seq_num++;

			temp_buf.resizeNoCopy(sizeof(uint64) + sizeof(uint32) * 3 + new_record_capacity); // Resize temp_buf to make room for record header and capacity

			std::memcpy(&temp_buf[0], &key.val, sizeof(uint64));
			std::memcpy(&temp_buf[8], &info.len, sizeof(uint32)); // len
			std::memcpy(&temp_buf[12], &info.capacity, sizeof(uint32)); // capacity
			std::memcpy(&temp_buf[16], &info.seq_num, sizeof(uint32)); // seq_num

			if(data.size() > 0)
				std::memcpy(&temp_buf[20], data.data(), data.size());
			std::memset(&temp_buf[20] + data.size(), 0, new_record_capacity - data.size()); // Zero out unused part of buffer.

			// Update the record in the DB file
			file_out->seek(new_record_offset);
			file_out->writeData(temp_buf.data(), temp_buf.size());
		}
	}
	else
	{
		// Else there was no record for the key present in DB:

		size_t new_record_offset;
		uint32 new_record_capacity;
		allocRecordSpace((uint32)data.size(), new_record_offset, new_record_capacity);

		RecordInfo info;
		info.offset = new_record_offset;
		info.len = (uint32)data.size();
		info.capacity = new_record_capacity;
		info.seq_num = 0;
		key_to_info_map.insert(std::make_pair(key, info));

		temp_buf.resizeNoCopy(sizeof(uint64) + sizeof(uint32) * 3 + new_record_capacity); // Resize temp_buf to make room for record header and capacity
		std::memcpy(&temp_buf[0], &key.val, sizeof(uint64));
		std::memcpy(&temp_buf[8], &info.len, sizeof(uint32)); // len
		std::memcpy(&temp_buf[12], &info.capacity, sizeof(uint32)); // capacity
		std::memcpy(&temp_buf[16], &info.seq_num, sizeof(uint32)); // seq_num

		if(data.size() > 0)
			std::memcpy(&temp_buf[20], data.data(), data.size());
		std::memset(&temp_buf[20] + data.size(), 0, new_record_capacity - data.size()); // Zero out unused part of buffer.

		// Update the record in the DB file
		file_out->seek(new_record_offset);
		file_out->writeData(temp_buf.data(), temp_buf.size());
	}
}


void Database::deleteRecord(const DatabaseKey& key)
{
	if(file_out == NULL)
		file_out = new FileOutStream(db_path, std::ios::binary | std::ios::in | std::ios::out);

	auto res = key_to_info_map.find(key);

	if(res != key_to_info_map.end())
	{
		RecordInfo& info = res->second;

		// Update the record header on disk with an invalid length field, to mark the record as deleted.
		// We will leave the capacity field as it was, so the record can be skipped over.

		const uint32 invalid_len = std::numeric_limits<uint32>::max();

		temp_buf.resizeNoCopy(sizeof(uint64) + sizeof(uint32) * 3);
		std::memcpy(&temp_buf[0], &key.val, sizeof(uint64));
		std::memcpy(&temp_buf[8], &invalid_len, sizeof(uint32)); // len
		std::memcpy(&temp_buf[12], &info.capacity, sizeof(uint32)); // capacity
		std::memcpy(&temp_buf[16], &info.seq_num, sizeof(uint32)); // seq_num

		// Update the record in the DB file
		file_out->seek(info.offset);
		file_out->writeData(temp_buf.data(), temp_buf.size());

		// If seq_num > 0, We need to keep in key_to_info_map, so that we have the seq number for the key handy.
		// That way if an item with the same key gets re-added, the record with the new item will have a sufficiently high seq num.
		if(info.seq_num == 0)
		{
			// If the seq num is zero, then we know there are no other records using this key in the file.  So we can remove this info from the key_to_info_map.
			key_to_info_map.erase(res);
		}
		else
		{
			info.len = invalid_len;
		}
	}
}


void Database::flush()
{
	if(file_out)
		file_out->flush();
}


size_t Database::numRecords() const
{
	size_t num = 0;
	for(auto it = key_to_info_map.begin(); it != key_to_info_map.end(); ++it)
	{
		const RecordInfo& info = it->second;
		if(info.isRecordValid())
			num++;
	}
	return num;
}
