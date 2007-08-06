/*=====================================================================
testdistob.cpp
--------------
File created by ClassTemplate on Tue Sep 07 03:03:48 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "testdistob.h"

#include "../utils/random.h"
#include "../networking/mystream.h"


TestDistOb::TestDistOb()
:	DistObject(true),//TEMP HACK always proper object
	uid(NULL)
{
	uid = new IntObUID();
	//uid.setId(Random::randInt());
}


TestDistOb::~TestDistOb()
{
	
}


/*void TestDistOb::serialiseUID(MyStream& mystream)
{
	mystream << uid;
}

void TestDistOb::deserialiseUID(MyStream& mystream)
{
	mystream >> uid;
}*/

void TestDistOb::serialise(MyStream& mystream)
{
	mystream << *uid;
	mystream << payload;
}

void TestDistOb::deserialise(MyStream& mystream)
{
	mystream >> *uid;
	mystream >> payload;
}

TestDistOb* TestDistOb::constructObFromStream(MyStream& stream, bool proper_object)
{
	TestDistOb* ob = new TestDistOb();
	ob->deserialise(stream);
	return ob;
}

const UID_REF TestDistOb::getUID()
{
	return uid;
}

void TestDistOb::setUID(const UID_REF new_uid)
{
	uid = new_uid;
}
