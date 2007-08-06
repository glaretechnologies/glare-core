/*=====================================================================
distobject.h
------------
File created by ClassTemplate on Fri Sep 03 04:58:27 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISTOBJECT_H_666_
#define __DISTOBJECT_H_666_

#include "distobuid.h"
//class IntObUID;
//class DistObSerialisedState;
class MyStream;


/*=====================================================================
DistObject
----------
Base class that all Distributed objects must inherit from.

NOTE: TODO: move UID into this class if no more design changes.
kinda bad cause loses type information :P
=====================================================================*/
class DistObject
{
public:
	/*=====================================================================
	DistObject
	----------
	
	=====================================================================*/
	DistObject(bool proper_object);

	virtual ~DistObject();


	//virtual const DistObUID& getUID() = 0;

	//virtual const IntObUID& getUID() = 0;

	//virtual void serialiseUID(MyStream& mystream) = 0;
	//virtual void deserialiseUID(MyStream& mystream) = 0;

	//virtual const IntObUID& getUID() const = 0;

	//NOTE: don't call this when it has been inserted in something
	//virtual void setUID(const IntObUID& uid) = 0;


	//NOTE: make this return a ref to UID_REF?
	virtual const UID_REF getUID() const = 0;
	virtual void setUID(const UID_REF& new_uid) = 0;




	//virtual void serialiseState(DistObSerialisedState& state_out) = 0;

	//virtual void deserialiseState(const DistObSerialisedState& state) = 0;

	virtual void serialise(MyStream& mystream) = 0;
	virtual void deserialise(MyStream& mystream) = 0;

	//also need to implement
	//static SubType* constructObFromStream(MyStream& stream);

protected:
	bool isProperObject() const { return proper_object; }

private:
	bool proper_object;
};



#endif //__DISTOBJECT_H_666_




