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


// Hash function for Vert
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



=====================================================================*/
class Database
{
public:
	Database();
	~Database();

	static void makeOrClearDatabase(const std::string& path);

	void openAndMakeOrClearDatabase(const std::string& path);

	void startReadingFromDisk(const std::string& path);

	void finishReadingFromDisk();

	void setPath(const std::string& db_path_) { db_path = db_path_; }

	DatabaseKey allocUnusedKey();

	void updateRecord(const DatabaseKey& key, ArrayRef<uint8> data);

	void deleteRecord(const DatabaseKey& key);

	size_t numRecords() const; // NOTE: linear time on number of records.

	//const js::Vector<RecordInfo, 16>& getInitialRecords() const { return initial_records; }
	//const size_t getInitialNumRecords() const { return initial_records.size(); }
	//const uint8* getInitialRecordData(size_t record_index, uint32& data_len_out) const
	//{
	//	const RecordInfo& info = initial_records[record_index];
	//	data_len_out = info.len;
	//	return (const uint8*)file_in->fileData() + info.offset + 20; // 20 = size of record header
	//}

	struct RecordInfo
	{
		size_t offset;
		uint32 len;
		uint32 capacity;
		uint32 seq_num;

		bool isRecordValid() const { return len != std::numeric_limits<uint32>::max(); }
	};

	const std::unordered_map<DatabaseKey, RecordInfo, DatabaseKeyHash>& getRecordMap() const { return key_to_info_map; }

	const uint8* getInitialRecordData(const RecordInfo& info/*, uint32& data_len_out*/) const
	{
		//data_len_out = info.len;
		return (const uint8*)file_in->fileData() + info.offset + 20; // 20 = size of record header
	}

private:
	void allocRecordSpace(uint32 data_size, size_t& offset_out, uint32& capacity_out);

	//void writeDataToNewRecord(RecordInfo& info, const DatabaseKey& key, ArrayRef<uint8> data);

	std::unordered_map<DatabaseKey, RecordInfo, DatabaseKeyHash> key_to_info_map;

	std::string db_path;

	//js::Vector<RecordInfo, 16> initial_records;

	FileInStream* file_in;
	FileOutStream* file_out;

	js::Vector<uint8, 16> temp_buf;

	size_t append_offset;

	uint64 next_unused_key;
};
