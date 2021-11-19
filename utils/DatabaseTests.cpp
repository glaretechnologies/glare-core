/*=====================================================================
DatabaseTests.cpp
-----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "DatabaseTests.h"


#include "Database.h"
#include "FileUtils.h"
#include "FileInStream.h"
#include "FileOutStream.h"
#include "Exception.h"
#include "StringUtils.h"
#include "TestUtils.h"
#include "ConPrint.h"
#include "PlatformUtils.h"
#include "Timer.h"
#include "../maths/PCG32.h"
#include <map>


#if BUILD_TESTS



static void doRandomTests(uint64 seed)
{
	PCG32 rng(seed);

	std::map<DatabaseKey, std::vector<uint8> > ref_map;

	const std::string db_path = "./test.db";
	//Database::makeOrClearDatabase(db_path);

	Database* db = new Database();
	db->openAndMakeOrClearDatabase(db_path);
	const int N = 5000;
	for(int i=0; i<N; ++i)
	{
		if(i % 1000)
			conPrint("Iter " + toString(i) + "/" + toString(N));

		float ur = rng.unitRandom();
		if(ur < 0.5)
		{
			// Add item
			std::vector<uint8> data;
			const uint32 len = (uint32)(std::exp(10 * rng.unitRandom()) - 1.0);
			data.resize(len);

			for(size_t z=0; z<data.size(); ++z)
				data[z] = (uint8)rng.nextUInt(256);

			const DatabaseKey key(rng.nextUInt(200));

			//conPrint("Adding key " + toString(key.val) + ", data len: " + toString(len));

			db->updateRecord(key, data);

			// Update in ref map
			ref_map[key] = data;
		}
		else if(ur < 0.9)
		{
			if(ref_map.size() > 0)
			{
				// Delete one of the items in the map
				const uint32 target_i = rng.nextUInt((uint32)ref_map.size());

				uint32 z = 0;
				for(auto it = ref_map.begin(); it != ref_map.end(); ++it)
				{
					if(z == target_i)
					{
						//conPrint("deleting key " + toString(it->first.val));

						db->deleteRecord(it->first);
						ref_map.erase(it);
						break;
					}
					z++;
				}
			}
		}
		else// if(ur < 0.7)
		{
			conPrint("reopening DB, cur size: " + toString(ref_map.size()));

			// close and re-open DB
			delete db;
			db = new Database();
			db->startReadingFromDisk(db_path);

			testAssert(db->numRecords() == ref_map.size());
			for(auto it = db->getRecordMap().begin(); it != db->getRecordMap().end(); ++it)
			{
				const Database::RecordInfo& record = it->second;
				if(record.isRecordValid())
				{
					const uint8* data = db->getInitialRecordData(record);
					
					// Check data against our reference
					auto res = ref_map.find(it->first);
					testAssert(res != ref_map.end());

					const std::vector<uint8>& ref_data = res->second;
					testAssert(record.len == ref_data.size());
					for(size_t z=0; z<record.len; ++z)
						testAssert(data[z] == ref_data[z]);
				}
			}

			db->finishReadingFromDisk();
		}
	}

	delete db;
}




void DatabaseTests::test()
{
	conPrint("DatabaseTests::test()");

	{
		// Create DB
		conPrint(PlatformUtils::getCurrentWorkingDirPath());
		const std::string db_path = "./test.db";
		Database::makeOrClearDatabase(db_path);

		// Add a record
		{
			Database db;
			db.startReadingFromDisk(db_path);
			db.finishReadingFromDisk();

			std::vector<uint8> data;
			data.push_back(1);
			data.push_back(2);
			data.push_back(3);
			db.updateRecord(DatabaseKey(123), data);
		}

		// Open the DB, check the record is there
		{
			Database db;
			db.startReadingFromDisk(db_path);
			testAssert(db.getRecordMap().size() == 1);
			testAssert(db.getRecordMap().count(DatabaseKey(123)) == 1);
			{
				auto res = db.getRecordMap().find(DatabaseKey(123));
				testAssert(res != db.getRecordMap().end());
				const uint32 record_data_len = res->second.len;
				const uint8* record_data = db.getInitialRecordData(res->second);
				testAssert(record_data_len == 3);
				testAssert(record_data[0] == 1);
				testAssert(record_data[1] == 2);
				testAssert(record_data[2] == 3);
			}
			db.finishReadingFromDisk();
		}

		// Update the record
		{
			Database db;
			db.startReadingFromDisk(db_path);
			db.finishReadingFromDisk();

			std::vector<uint8> data;
			data.push_back(4);
			data.push_back(5);
			data.push_back(6);
			data.push_back(7);
			db.updateRecord(DatabaseKey(123), data);
		}
		
		// Open the DB, check the updated record is there
		{
			Database db;
			db.startReadingFromDisk(db_path);
			testAssert(db.getRecordMap().size() == 1);
			{
				auto res = db.getRecordMap().find(DatabaseKey(123));
				testAssert(res != db.getRecordMap().end());
				const uint32 record_data_len = res->second.len;
				const uint8* record_data = db.getInitialRecordData(res->second);
				testAssert(record_data_len == 4);
				testAssert(record_data[0] == 4);
				testAssert(record_data[1] == 5);
				testAssert(record_data[2] == 6);
				testAssert(record_data[3] == 7);
			}
			db.finishReadingFromDisk();
		}

		// Add another record
		{
			Database db;
			db.startReadingFromDisk(db_path);
			db.finishReadingFromDisk();

			std::vector<uint8> data;
			data.push_back(200);
			db.updateRecord(DatabaseKey(200), data);
		}

		// Open the DB, check the updated records are there
		{
			Database db;
			db.startReadingFromDisk(db_path);
			testAssert(db.getRecordMap().size() == 2);
			{
				auto res = db.getRecordMap().find(DatabaseKey(123));
				testAssert(res != db.getRecordMap().end());
				const uint32 record_data_len = res->second.len;
				const uint8* record_data = db.getInitialRecordData(res->second);
				testAssert(record_data_len == 4);
				testAssert(record_data[0] == 4);
				testAssert(record_data[1] == 5);
				testAssert(record_data[2] == 6);
				testAssert(record_data[3] == 7);
			}
			{
				auto res = db.getRecordMap().find(DatabaseKey(200));
				testAssert(res != db.getRecordMap().end());
				const uint32 record_data_len = res->second.len;
				const uint8* record_data = db.getInitialRecordData(res->second);
				testAssert(record_data_len == 1);
				testAssert(record_data[0] == 200);
			}
			db.finishReadingFromDisk();
		}

		// Update the record with much longer data
		{
			Database db;
			db.startReadingFromDisk(db_path);
			db.finishReadingFromDisk();

			std::vector<uint8> data(100);
			for(int i=0; i<100; ++i)
				data[i] = (uint8)i;
			db.updateRecord(DatabaseKey(123), data);
		}

		{
			Database db;
			db.startReadingFromDisk(db_path);
			testAssert(db.getRecordMap().size() == 2);
			{
				auto res = db.getRecordMap().find(DatabaseKey(123));
				testAssert(res != db.getRecordMap().end());
				const uint32 record_data_len = res->second.len;
				const uint8* record_data = db.getInitialRecordData(res->second);
				testAssert(record_data_len == 100);
				for(int i=0; i<100; ++i)
					testAssert(record_data[i] == i);
			}
			{
				auto res = db.getRecordMap().find(DatabaseKey(200));
				testAssert(res != db.getRecordMap().end());
				const uint32 record_data_len = res->second.len;
				const uint8* record_data = db.getInitialRecordData(res->second);
				testAssert(record_data_len == 1);
				testAssert(record_data[0] == 200);
			}
			db.finishReadingFromDisk();
		}

		// Delete a record
		{
			Database db;
			db.startReadingFromDisk(db_path);
			db.finishReadingFromDisk();

			db.deleteRecord(DatabaseKey(123));
		}
		{
			Database db;
			db.startReadingFromDisk(db_path);
			testAssert(db.numRecords() == 1);
			{
				auto res = db.getRecordMap().find(DatabaseKey(200));
				testAssert(res != db.getRecordMap().end());
				const uint32 record_data_len = res->second.len;
				const uint8* record_data = db.getInitialRecordData(res->second);
				testAssert(record_data_len == 1);
				testAssert(record_data[0] == 200);
			}
			db.finishReadingFromDisk();
		}

		// Add back record
		{
			Database db;
			db.startReadingFromDisk(db_path);
			db.finishReadingFromDisk();

			std::vector<uint8> data;
			data.push_back(123);
			db.updateRecord(DatabaseKey(123), data);
		}

		// Check record with key 123 is back
		{
			Database db;
			db.startReadingFromDisk(db_path);
			testAssert(db.numRecords() == 2);
			{
				auto res = db.getRecordMap().find(DatabaseKey(123));
				testAssert(res != db.getRecordMap().end());
				const uint32 record_data_len = res->second.len;
				const uint8* record_data = db.getInitialRecordData(res->second);
				testAssert(record_data_len == 1);
				testAssert(record_data[0] == 123);
			}
			{
				auto res = db.getRecordMap().find(DatabaseKey(200));
				testAssert(res != db.getRecordMap().end());
				const uint32 record_data_len = res->second.len;
				const uint8* record_data = db.getInitialRecordData(res->second);
				testAssert(record_data_len == 1);
				testAssert(record_data[0] == 200);
			}
			db.finishReadingFromDisk();
		}
	}


	// Do a speed test
	{
		const std::string db_path = "./test.db";
		const int N = 10000;
		
		{
			PCG32 rng(1);
		
			Database::makeOrClearDatabase(db_path);

			Timer timer;
			{
				Database db;
				db.startReadingFromDisk(db_path);
				db.finishReadingFromDisk();

				
				for(int i=0; i<N; ++i)
				{
					std::vector<uint8> data;
					data.push_back(i % 256);
					data.push_back(2);
					data.push_back(3);
					db.updateRecord(DatabaseKey(i), data);
				}
			}
			conPrint("Writes / s: " + doubleToStringNSigFigs(N / timer.elapsed(), 4));
		}
		

		{
			Timer timer;

			Database db;
			db.startReadingFromDisk(db_path);
			
			testAssert(db.getRecordMap().size() == N);

			for(int i=0; i<N; ++i)
			{
				auto res = db.getRecordMap().find(DatabaseKey(i));
				testAssert(res != db.getRecordMap().end());
				
				const uint32 record_data_len = res->second.len;
				const uint8* record_data = db.getInitialRecordData(res->second);
				testAssert(record_data_len == 3);
				testAssert(record_data[0] == i % 256);
				testAssert(record_data[1] == 2);
				testAssert(record_data[2] == 3);
			}
			db.finishReadingFromDisk();

			conPrint("Initial read speed: " + doubleToStringNSigFigs(N / timer.elapsed(), 4) + " reads / s");
		}
	}


	// Test deleteRecord
	{
		const std::string db_path = "./test.db";
		{
			Database db;
			db.openAndMakeOrClearDatabase(db_path);

			// Test deleteRecord on an empty DB
			{
				testAssert(db.numRecords() == 0);

				db.deleteRecord(DatabaseKey(123));

				testAssert(db.numRecords() == 0);
			}
		}

		// Re-load DB, check num (valid) records is zero.
		{
			Database db;
			db.startReadingFromDisk(db_path);
			db.finishReadingFromDisk();
			testAssert(db.numRecords() == 0);
		}

		// Test repeatedly adding a single record then deleting it, opening and closing the DB each time
		for(int i=0; i<100; ++i)
		{
			Database db;
			db.openAndMakeOrClearDatabase(db_path);

			testAssert(db.numRecords() == 0);

			std::vector<uint8> data(100, 123);
			db.updateRecord(DatabaseKey(123), data);

			testAssert(db.numRecords() == 1);

			db.deleteRecord(DatabaseKey(123));
		}

		// Test repeatedly adding a single record then deleting it, keeping the DB open
		{
			Database db;
			db.openAndMakeOrClearDatabase(db_path);

			for(int i=0; i<100; ++i)
			{
				testAssert(db.numRecords() == 0);

				std::vector<uint8> data(100, 123);
				db.updateRecord(DatabaseKey(123), data);

				testAssert(db.numRecords() == 1);

				db.deleteRecord(DatabaseKey(123));
			}
		}

		// Add a record, update it with a bigger record to force a new record location and incremented seq num, then delete, then re-add, and make sure it's still there on re-open.
		{
			Database db;
			db.openAndMakeOrClearDatabase(db_path);

			std::vector<uint8> data(/*count=*/1, 123);
			db.updateRecord(DatabaseKey(123), data);

			std::vector<uint8> data2(/*count=*/200, 123);
			db.updateRecord(DatabaseKey(123), data2); // update with a bigger record to force a new record location

			db.deleteRecord(DatabaseKey(123));

			std::vector<uint8> data3(/*count=*/200, 123);
			db.updateRecord(DatabaseKey(123), data3); // re-add
		}
		{
			Database db;
			db.startReadingFromDisk(db_path);
			db.finishReadingFromDisk();
			testAssert(db.numRecords() == 1);
		}


		// Do the same as previously, but close and re-open the DB after deleting the record
		{
			Database db;
			db.openAndMakeOrClearDatabase(db_path);

			std::vector<uint8> data(/*count=*/1, 123);
			db.updateRecord(DatabaseKey(123), data);

			//std::vector<uint8> data2(/*count=*/200, 123);
			//db.updateRecord(DatabaseKey(123), data2); // update with a bigger record to force a new record location

			db.deleteRecord(DatabaseKey(123));
		}
		{
			Database db;
			db.startReadingFromDisk(db_path);
			db.finishReadingFromDisk();

			testAssert(db.numRecords() == 0);

			std::vector<uint8> data3(/*count=*/200, 123);
			db.updateRecord(DatabaseKey(123), data3); // re-add

			testAssert(db.numRecords() == 1);
		}
		{
			Database db;
			db.startReadingFromDisk(db_path);
			db.finishReadingFromDisk();
			testAssert(db.numRecords() == 1);
		}
	}

	// Test adding zero-sized records
	{
		const std::string db_path = "./test.db";
		{
			Database db;
			
			db.openAndMakeOrClearDatabase(db_path);
			testAssert(db.numRecords() == 0);

			std::vector<uint8> data3;
			db.updateRecord(DatabaseKey(123), data3);

			testAssert(db.numRecords() == 1);

			db.updateRecord(DatabaseKey(456), data3);

			testAssert(db.numRecords() == 2);
		}

		{
			Database db;
			db.startReadingFromDisk(db_path);
			db.finishReadingFromDisk();

			testAssert(db.numRecords() == 2);

			testAssert(db.getRecordMap().count(DatabaseKey(123)) == 1);
			testAssert(db.getRecordMap().find(DatabaseKey(123))->second.len == 0);

			testAssert(db.getRecordMap().count(DatabaseKey(456)) == 1);
			testAssert(db.getRecordMap().find(DatabaseKey(456))->second.len == 0);

			db.deleteRecord(DatabaseKey(123));

			testAssert(db.numRecords() == 1);
		}
	}


	doRandomTests(1);

	conPrint("DatabaseTests::test() done");
}


#endif // BUILD_TESTS
