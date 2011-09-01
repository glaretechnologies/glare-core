/*=====================================================================
StreamUtils.cpp
---------------
File created by ClassTemplate on Fri May 29 13:14:27 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "StreamUtils.h"


namespace StreamUtils
{


uint32 readUInt32(std::istream& stream)
{
	uint32 x;
	stream.read((char*)&x, sizeof(uint32));
	return x;
}


void writeUInt32(std::ostream& stream, uint32 x)
{
	stream.write((const char*)&x, sizeof(uint32));
}


}
