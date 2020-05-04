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


int32 readInt32(std::istream& stream)
{
	int32 x;
	stream.read((char*)&x, sizeof(int32));
	return x;
}


void writeInt32(std::ostream& stream, int32 x)
{
	stream.write((const char*)&x, sizeof(int32));
}


float readFloat(std::istream& stream)
{
	float x;
	stream.read((char*)&x, sizeof(float));
	return x;
}


void writeFloat(std::ostream& stream, float x)
{
	stream.write((const char*)&x, sizeof(float));
}


double readDouble(std::istream& stream)
{
	double x;
	stream.read((char*)&x, sizeof(double));
	return x;
}


void writeDouble(std::ostream& stream, double x)
{
	stream.write((const char*)&x, sizeof(double));
}


}
