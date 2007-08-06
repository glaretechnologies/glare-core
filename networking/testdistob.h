/*=====================================================================
testdistob.h
------------
File created by ClassTemplate on Tue Sep 07 03:03:48 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TESTDISTOB_H_666_
#define __TESTDISTOB_H_666_



#include "distobject.h"
#include "intobuid.h"
#include <string>
class MyStream;

/*=====================================================================
TestDistOb
----------

=====================================================================*/
class TestDistOb : public DistObject
{
public:
	/*=====================================================================
	TestDistOb
	----------
	
	=====================================================================*/
	TestDistOb();

	virtual ~TestDistOb();

	//virtual void serialiseUID(MyStream& mystream);
	//virtual void deserialiseUID(MyStream& mystream);

	virtual void serialise(MyStream& mystream);
	virtual void deserialise(MyStream& mystream);


	void setPayload(const std::string& s){ payload = s; }
	const std::string& getPayload() const { return payload; }

	//virtual const IntObUID& getDistObUID() const { return uid; }

	virtual const UID_REF getUID();
	virtual void setUID(const UID_REF new_uid);

	static TestDistOb* constructObFromStream(MyStream& stream, bool proper_object);

private:
	//int uid;
	//IntObUID uid;
	UID_REF uid;

	std::string payload;

};



#endif //__TESTDISTOB_H_666_




