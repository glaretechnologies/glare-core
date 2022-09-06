

#include "ListenerThread.h"
#include "Escaping.h"
#include "WebsiteExcep.h"
#include "DataStore.h"
#include <ThreadManager.h>
#include <platformutils.h>
#include <networking.h>
#include <Clock.h>
#include <fileutils.h>
#include <ConPrint.h>
#include <Parser.h>
#include <networking.h>
#include <SocketTests.h>



int main(int argc, char **argv)
{
	Clock::init();
	Networking::createInstance();
	PlatformUtils::ignoreUnixSignals();

	// TEMP: run tests
	if(false)
	{
#if BUILD_TESTS
		Escaping::test();
		Parser::doUnitTests();
		//IPAddress::test();
		Networking::getInstance().doDNSLookup("localhost");
		SocketTests::test();
#endif
		return 666;//TEMP
	}

	try
	{

		Reference<DataStore> data_store = new DataStore("website.data");

		if(FileUtils::fileExists(data_store->path))
			data_store->loadFromDisk();

		ThreadManager thread_manager;
		thread_manager.addThread(new ListenerThread(8080, data_store.getPointer()));//TEMP

		while(1)
		{
			PlatformUtils::Sleep(10000);
		}
	}
	catch(WebsiteExcep& e)
	{
		conPrint("WebsiteExcep: " + e.what());
	}

	Networking::destroyInstance();
	return 0;
}

