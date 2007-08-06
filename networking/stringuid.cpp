/*=====================================================================
stringuid.cpp
-------------
File created by ClassTemplate on Wed Oct 27 06:05:01 2004Code By Nicholas Chapman.
=====================================================================*/
#include "stringuid.h"


#include "mystream.h"


StringUID::StringUID()
{
	
}

StringUID::StringUID(const std::string& uidstring_)
:	uidstring(uidstring_)
{
	
}

StringUID::~StringUID()
{
	
}

void StringUID::serialise(MyStream& stream) const
{
	stream << uidstring;
}

void StringUID::deserialise(MyStream& stream)
{
	stream >> uidstring;
}


bool StringUID::operator == (const DistObUID& other) const
{
	const StringUID* other_stringuid = dynamic_cast<const StringUID*>(&other);

	if(other_stringuid)
	{
		return uidstring == other_stringuid->uidstring;
	}
	else
	{
		assert(0);//type error
		return false;
	}
}

bool StringUID::operator < (const DistObUID& other) const
{
	const StringUID* other_stringuid = dynamic_cast<const StringUID*>(&other);

	if(other_stringuid)
	{
		return uidstring < other_stringuid->uidstring;
	}
	else
	{
		assert(0);//type error
		return false;
	}
}






