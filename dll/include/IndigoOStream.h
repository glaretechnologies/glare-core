/*=====================================================================
IndigoOStream.h
---------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "IndigoAllocation.h"


namespace Indigo
{


class String;


///
/// Output stream
///
class OStream
{
public:
	USE_INDIGO_SDK_ALLOCATOR

	OStream();
	virtual ~OStream();

	virtual void writeString(const String& s) = 0;
	virtual void writeDouble(double x) = 0;

private:

};


inline OStream& operator << (OStream& stream, const String& str)
{
	stream.writeString(str);
	return stream;
}


}
