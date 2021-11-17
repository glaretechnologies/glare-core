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
#include "PCG32.h"
#include "Timer.h"


#if BUILD_TESTS


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
			db.updateRecord(DatabaseKey(123), ArrayRef<uint8>(data.data(), data.size() * sizeof(uint8)));
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
			db.updateRecord(DatabaseKey(123), ArrayRef<uint8>(data.data(), data.size() * sizeof(uint8)));
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
			db.updateRecord(DatabaseKey(200), ArrayRef<uint8>(data.data(), data.size() * sizeof(uint8)));
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
			db.updateRecord(DatabaseKey(123), ArrayRef<uint8>(data.data(), data.size() * sizeof(uint8)));
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
					db.updateRecord(DatabaseKey(i), ArrayRef<uint8>(data.data(), data.size() * sizeof(uint8)));
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

	conPrint("DatabaseTests::test() done");
}


#endif // BUILD_TESTS
