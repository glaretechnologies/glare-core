/*=====================================================================
stringuid.h
-----------
File created by ClassTemplate on Wed Oct 27 06:05:01 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __STRINGUID_H_666_
#define __STRINGUID_H_666_


#include "distobuid.h"
#include <string>


/*=====================================================================
StringUID
---------

=====================================================================*/
class StringUID : public DistObUID
{
public:
	/*=====================================================================
	StringUID
	---------
	
	=====================================================================*/
	StringUID();
	StringUID(const std::string& uidstring);

	virtual ~StringUID();

	virtual void serialise(MyStream& stream) const;
	virtual void deserialise(MyStream& stream);


	virtual bool operator == (const DistObUID& other) const;
	virtual bool operator < (const DistObUID& other) const;

	virtual const std::string toString() const { return uidstring; }

private:
	std::string uidstring;
};



#endif //__STRINGUID_H_666_




