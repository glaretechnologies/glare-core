/*=====================================================================
Database.h
----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "DatabaseKey.h"
#include "ArrayRef.h"
#include "Platform.h"
#include "HashMapInsertOnly2.h"
#include "Vector.h"
#include "FileInStream.h"
#include <string>
#include <unordered_map>
class FileInStream;
class FileOutStream;


// Hash function for DatabaseKey
class DatabaseKeyHash
{
public:
	inline size_t operator()(const DatabaseKey& v) const
	{	
		return hashBytes((const uint8*)&v.val, sizeof(uint64));
	}
};


/*=====================================================================
Database
--------

To read an existing DB:

Database database;
database.startReadingFromDisk(path);
for(auto it = database.getRecordMap().begin(); it != database.getRecordMap().end(); ++it)
{
	const Database::RecordInfo& record = it->second;
	if(record.isRecordValid())
	{
		 const uint8* data = database.getInitialRecordData(record);
		 // Do something with data and record.len
	}
}
database.finishReadingFromDisk();



Tests are in DatabaseTests.
=====================================================================*/
class Database
{
public:
	Database();
	~Database();

	static void makeOrClearDatabase(const std::string& path);

	void openAndMakeOrClearDatabase(const std::string& path);

	void startReadingFromDisk(const std::string& path);

	void removeOldRecordsOnDisk(const std::string& path); // Removes deleted records, removes old records.  Database can't have had any updates made to it since opening. (so that file_out is NULL)

	struct RecordInfo;

	// Call only after startReadingFromDisk() and before finishReadingFromDisk().
	const uint8* getInitialRecordData(const RecordInfo& info) const
	{
		return (const uint8*)file_in->fileData() + info.offset + 20; // 20 = size of record header
	}

	void finishReadingFromDisk();

	DatabaseKey allocUnusedKey();

	void updateRecord(const DatabaseKey& key, ArrayRef<uint8> data);

	void deleteRecord(const DatabaseKey& key);

	void flush();

	size_t numRecords() const; // Get number of valid records. NOTE: linear time on number of records.

	struct RecordInfo
	{
		size_t offset; // Offset in bytes from start of file
		uint32 len; // Length of record data in bytes.  Has a sentinel value which indicates this is not a valid record, e.g. the record has been deleted.
		uint32 capacity; // Capacity of record in bytes.  >= len.
		uint32 seq_num; // Sequence number.  Whenever a record is written to a new location, the sequence number is incremented.

		bool isRecordValid() const { return len != std::numeric_limits<uint32>::max(); }
	};

	const std::unordered_map<DatabaseKey, RecordInfo, DatabaseKeyHash>& getRecordMap() const { return key_to_info_map; }

private:
	void allocRecordSpace(uint32 data_size, size_t& offset_out, uint32& capacity_out);

	std::unordered_map<DatabaseKey, RecordInfo, DatabaseKeyHash> key_to_info_map;

	std::string db_path;

	FileInStream* file_in;
	FileOutStream* file_out;

	js::Vector<uint8, 16> temp_buf;

	size_t append_offset;

	uint64 next_unused_key;
};
