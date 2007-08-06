/*=====================================================================
distobuid.h
-----------
File created by ClassTemplate on Fri Sep 03 05:22:00 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISTOBUID_H_666_
#define __DISTOBUID_H_666_


#include "../utils/refcounted.h"
#include "../utils/reference.h"
#include <string>
class MyStream;



/*=====================================================================
DistObUID
---------
Interface for a unique identifier
=====================================================================*/
class DistObUID : public RefCounted
{
public:
	/*=====================================================================
	DistObUID
	---------
	
	=====================================================================*/
	//DistObUID();

	virtual ~DistObUID(){}


	virtual void serialise(MyStream& stream) const = 0;
	virtual void deserialise(MyStream& stream) = 0;

	virtual bool operator < (const DistObUID& other) const = 0;
	virtual bool operator == (const DistObUID& other) const = 0;


	virtual const std::string toString() const = 0;
};


typedef Reference<DistObUID> UID_REF;



inline MyStream& operator << (MyStream& stream, const DistObUID& uid)
{
	uid.serialise(stream);
	return stream;
}

inline MyStream& operator >> (MyStream& stream, DistObUID& uid)
{
	uid.deserialise(stream);
	return stream;
}



#endif //__DISTOBUID_H_666_




