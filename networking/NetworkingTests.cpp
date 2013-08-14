/*=====================================================================
NetworkingTests.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Fri Oct 22 13:53:30 +1300 2010
=====================================================================*/
#include "NetworkingTests.h"


#include "mysocket.h"
#include "networking.h"


NetworkingTests::NetworkingTests()
{

}


NetworkingTests::~NetworkingTests()
{

}


#if BUILD_TESTS


#include "../indigo/globals.h"


class NetworkingTests_ShouldAbortCallback : public StreamShouldAbortCallback
{
public:
	NetworkingTests_ShouldAbortCallback()
	{}

	virtual ~NetworkingTests_ShouldAbortCallback(){}

	virtual bool shouldAbort()
	{
		return false;
	}

private:
};


void NetworkingTests::test()
{
	conPrint("NetworkingTests::test()");

	const int N = 1;

	NetworkingTests_ShouldAbortCallback callback;

	for(int i=0; i<N; ++i)
	{
		try
		{
			conPrint("Connecting to localhost:6666");
			MySocket sock("localhost", 6666, &callback);
		}
		catch(MySocketExcep& )
		{

		}
	}

	for(int i=0; i<N; ++i)
	{
		try
		{
			conPrint("Connecting to not_a_resolvable_dns_name:6666");
			MySocket sock("not_a_resolvable_dns_name", 6666, &callback);
		}
		catch(MySocketExcep& )
		{

		}
	}
	
}
#endif
