

#pragma warning(disable : 4786)//disable long debug name warning




#include <iostream>
#include "networking.h"
#include "nonthreadedserver.h"
#include "simpleserverstore.h"
#include "simpleclientstore.h"
#include "clientserversys.h"
#include "testdistob.h"
#include "clientserverstreamsys.h"
#include "streamsysserver.h"
#include "../utils/random.h"
#include "../utils/stringutils.h"
#include "testchatclient.h"
#include "testchatserver.h"
#include "distobstore.h"
#include "permissivehandler.h"
#include "intobuidfactory.h"
#include "../utils/timer.h"

using namespace std;

int main()
{
	Random::seedWithTime();

	Networking::createInstance(new Networking());

	const int LISTEN_PORT = 5555;

	const int TESTDIST_OB_STREAM_UID = 6;

	Timer timer;

	while(1)
	{
		cout << "(s)erver or (c)lient?" << endl;

		string reply;
		cin >> reply;

		if(reply == "s")//server
		{
			try
			{
				//int port;
				//cout << "enter port to listen on" << endl;
				//cin >> port;

				StreamSysServer stream_sys_server(LISTEN_PORT, NULL);

				//TestChatServer chatserver(&stream_sys_server);


				//SimpleServerStore store;

				DistObStore<TestDistOb> store;

				PermissiveHandler handler;

				IntObUIDFactory intobuid_factory;

				NonThreadedServer server(&stream_sys_server, &handler, TESTDIST_OB_STREAM_UID, 
					&store, 0.1f, &intobuid_factory);

				//store.setServer(&server);//TEMP HACK DELAY

			
				while(1)
				{
					stream_sys_server.think();

					server.think(timer.getSecondsElapsed());

					Sleep(10);
				}
			}
			catch(DistObServerExcep& e)
			{
				cout << "DistObServerExcep: " << e.what() << endl;
			}


			break;
		}
		else if(reply == "c")//client
		{
			
			string ipstring = "127.0.0.1";
			//cout << "enter server ip" << endl;
			//cin >> ipstring;



			//int port;
			//cout << "enter port" << endl;
			//cin >> port;

		
			try
			{
				ClientServerStreamSys cs_stream_sys;
				cs_stream_sys.connect(ipstring, LISTEN_PORT);

		
				//TestChatClient chatclient(&cs_stream_sys);

				
				
				//SimpleClientStore clientstore;
				DistObStore<TestDistOb> clientstore;
				DistObStore<TestDistOb> client_update_store;
			
				IntObUIDFactory intobuid_factory;

				ClientServerSys client(NULL, &cs_stream_sys, TESTDIST_OB_STREAM_UID, &client_update_store,
					&clientstore, &intobuid_factory, 0.1f);


				//------------------------------------------------------------------------
				//create a random object
				//------------------------------------------------------------------------
				TestDistOb newob;// = new TestDistOb();
				newob.setPayload("random payload blah" + ::toString(rand()) );

				cout << "trying to create test object, uid: " << newob.getUID()->toString() <<
					", payload: " << newob.getPayload() << endl;

				client_update_store.insertObject(&newob);
				client.tryUpdateDistOb(newob);

				while(1)
				{
					cs_stream_sys.think();

					//if(Random::unit() < 0.05)					
					//	chatclient.sendChatMessage("hello");

					client.think(timer.getSecondsElapsed());

					Sleep(10);
				}
			
			}
			catch(DistObSysExcep& e)
			{
				cout << "DistObSysExcep: " << e.what() << endl;
			}
			catch(StreamSysExcep& e)
			{
				cout << "StreamSysExcep: " << e.what() << endl;
			}	

			break;
		}

	}

	Networking::destroyInstance();

	/*cout << "\nenter something to exit." << endl;
	int a;
	cin >> a;*/
	return 1;



	return 0;
}