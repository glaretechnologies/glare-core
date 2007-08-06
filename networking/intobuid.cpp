/*=====================================================================
intobuid.cpp
------------
File created by ClassTemplate on Sun Sep 05 06:44:13 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "intobuid.h"


#include "mystream.h"
#include "../utils/random.h"
#include "../utils/stringutils.h"


IntObUID::IntObUID()
{
	//id = -1;
	id = Random::randInt();
}

IntObUID::IntObUID(int id_)
:	id(id_)
{}


IntObUID::~IntObUID()
{
	
}


void IntObUID::serialise(MyStream& stream) const
{
	stream << id;
}

void IntObUID::deserialise(MyStream& stream)
{
	stream >> id;
}

bool IntObUID::operator == (const DistObUID& other) const
{
	const IntObUID* other_int = dynamic_cast<const IntObUID*>(&other);

	if(other_int)
	{
		return id == other_int->id;
	}
	else
	{
		assert(0);//type error
		return false;
	}
}

bool IntObUID::operator < (const DistObUID& other) const
{
	const IntObUID* other_int = dynamic_cast<const IntObUID*>(&other);

	if(other_int)
	{
		return id < other_int->id;
	}
	else
	{
		assert(0);//type error
		return false;
	}
}

const std::string IntObUID::toString() const
{
	return ::toString(id);
}
