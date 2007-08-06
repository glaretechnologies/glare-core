/*=====================================================================
intobuid.h
----------
File created by ClassTemplate on Sun Sep 05 06:44:13 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __INTOBUID_H_666_
#define __INTOBUID_H_666_


#include "distobuid.h"
#include <string>

/*=====================================================================
IntObUID
--------
32 bit integer UID
=====================================================================*/
class IntObUID : public DistObUID
{
public:
	/*=====================================================================
	IntObUID
	--------
	
	=====================================================================*/
	IntObUID();
	IntObUID(int id);

	~IntObUID();
	/*virtual ~IntObUID();


	virtual void serialise(MyStream& stream) const;
	virtual void deserialise(MyStream& stream);*/

	virtual void serialise(MyStream& stream) const;
	virtual void deserialise(MyStream& stream);

	void setId(int id_){ id = id_; }
	const int getId() const { return id; }

	//const std::string toString() const;

	//virtual inline bool operator == (const IntObUID& other) const;
	//inline bool operator < (const IntObUID& other) const;
	virtual bool operator == (const DistObUID& other) const;
	virtual bool operator < (const DistObUID& other) const;


	virtual const std::string toString() const;

private:
	int id;
};


/*bool IntObUID::operator == (const IntObUID& other) const
{
	return id == other.id;
}*/

/*bool IntObUID::operator < (const IntObUID& other) const
{
	return id < other.id;
}*/


inline MyStream& operator << (MyStream& stream, const IntObUID& uid)
{
	uid.serialise(stream);
	return stream;
}

inline MyStream& operator >> (MyStream& stream, IntObUID& uid)
{
	uid.deserialise(stream);
	return stream;
}


#endif //__INTOBUID_H_666_





