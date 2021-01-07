/*=====================================================================
NetworkingTests.cpp
-------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "NetworkingTests.h"


#include "MySocket.h"
#include "Networking.h"


NetworkingTests::NetworkingTests()
{

}


NetworkingTests::~NetworkingTests()
{

}


#if BUILD_TESTS


#include "../indigo/globals.h"


void NetworkingTests::test()
{
	conPrint("NetworkingTests::test()");

	const int N = 1;

	for(int i=0; i<N; ++i)
	{
		try
		{
			conPrint("Connecting to localhost:6666");
			MySocket sock("localhost", 6666);
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
			MySocket sock("not_a_resolvable_dns_name", 6666);
		}
		catch(MySocketExcep& )
		{

		}
	}
	
}
#endif
