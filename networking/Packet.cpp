/*=====================================================================
Packet.cpp
----------
File created by ClassTemplate on Sun Apr 14 10:14:17 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "Packet.h"


#include "assert.h"
#include "../utils/StringUtils.h"
#include "Networking.h"
#include <cstring>


#if defined(_WIN32) || defined(_WIN64)
#include <winsock.h>//#include "winsock2.h"
#else
#include <netinet/in.h>
//#include <unistd.h>//for close()
//#include <sys/time.h>//fdset
//#include <sys/types.h>//fdset
#endif


//const int MAX_PACKETSIZE = 1000;

Packet::Packet()
{
	readindex = 0;

	static_assert(sizeof(char) == 1, "sizeof(char) == 1");
}




Packet::~Packet()
{
}





/*void Packet::write(float x)
{
	assert(sizeof(unsigned int) == sizeof(float));

	const size_t writeindex = data.size();
	data.resize(writeindex + sizeof(float));

	*(unsigned int*)(getData() + writeindex) = htonl(*((unsigned int*)&x));
}*/

void Packet::writeInt32(int32 x)
{
	const size_t writeindex = data.size();
	data.resize(writeindex + sizeof(int32));

	// Copy to uint32 in host byte order
	uint32 val_h;
	std::memcpy(&val_h, &x, sizeof(int32));

	// Get in network order
	uint32 val_n = htonl(val_h);

	// Copy to buffer
	std::memcpy(&data[writeindex], &val_n, sizeof(int32));

	//*(unsigned int*)(getData() + writeindex) = htonl(*((unsigned int*)&x));
}
/*
void Packet::write(unsigned short x)
{
	const size_t writeindex = data.size();
	data.resize(writeindex + sizeof(unsigned short));

	*(unsigned short*)(getData() + writeindex) = htons(x);
}

void Packet::write(unsigned char x)
{
	const size_t writeindex = data.size();
	data.resize(writeindex + sizeof(unsigned char));

	*(unsigned char*)(getData() + writeindex) = x;

}
void Packet::write(char x)
{
	const size_t writeindex = data.size();
	data.resize(writeindex + sizeof(unsigned char));

	*(unsigned char*)(getData() + writeindex) = *((unsigned char*)&x);
}
*/

void Packet::write(const void* src, size_t numbytes)
{
	const size_t writeindex = data.size();
	data.resize(writeindex + numbytes);

	std::memcpy(&data[writeindex], src, numbytes);

	/*for(size_t i=0; i<numbytes; ++i)
	{
		data[writeindex + i] = *((char*)src + i);
	}*/
}

/*
void Packet::write(const std::string& s) // writes null-terminated string
{
	// Write length of string
	write((int32)s.length());

	// Write string data
	write(&(*s.begin()), s.length());

}*/
	
/*void Packet::readTo(float& x)
{
	assert(sizeof(unsigned int) == sizeof(float));


	if(readindex + sizeof(float) > data.size())
		throw MyStreamExcep("read past end of packet.");

	unsigned int xi = ntohl(*(unsigned int*)(getData() + readindex));
	x = *((float*)&xi);
	readindex += sizeof(float);
}
*/
void Packet::readInt32To(int32& x)
{
	if(readindex + sizeof(int32) > data.size())
		throw NetworkingExcep("read past end of packet.");

	// Get in network byte order
	uint32 val_n;
	std::memcpy(&val_n, getData() + readindex, sizeof(uint32));

	// Convert to host order
	uint32 val_h = ntohl(val_n);

	// Copy to x
	std::memcpy(&x, &val_h, sizeof(uint32));

	//unsigned int xi = ntohl(*((unsigned int*)(getData() + readindex)));
	//x = *((int*)&xi);
	
	readindex += sizeof(int32);
}
/*
void Packet::readTo(char& x)
{
	if(readindex + sizeof(char) > data.size())
		throw MyStreamExcep("read past end of packet.");

	x = *(char*)(getData() + readindex);
	readindex += sizeof(char);
}

void Packet::readTo(unsigned short& x)
{
	if(readindex + sizeof(unsigned short) > data.size())
		throw MyStreamExcep("read past end of packet.");

	x = ntohs(*(unsigned short*)(getData() + readindex));
	readindex += sizeof(unsigned short);
}*/

/*void Packet::readTo(Vec3& vec)
{
	readTo(vec.x);
	readTo(vec.y);
	readTo(vec.z);
}*/

/*void Packet::readTo(std::string& s, int maxlength)
{

	int length;
	readTo(length);
	if(length < 0 || length > maxlength)
		throw MyStreamExcep("String was too long.");
	//std::vector<char> buffer(length);
	//readTo(buffer

	s.resize(length);
	readTo(&(*s.begin()), length);
}

void Packet::readTo(void* buffer, int numbytes)
{
	if(readindex + numbytes > (int)data.size())
		throw MyStreamExcep("read past end of packet.");

	for(int i=0; i<numbytes; ++i)
	{
		*((char*)buffer + i)  = data[readindex + i];
	}

	readindex += numbytes;
}



void Packet::writeToStreamSized(MyStream& stream)//write to stream with an int indicating the size first.
{
	stream << (int)data.size();
	stream.write(getData(), data.size());
}

void Packet::readFromStreamSize(MyStream& stream)
{
	int datasize;
	stream >> datasize;
	
	//NOTE: optimise for currently empty
	assert(data.size() == 0);

	if(datasize < 0 || datasize > MAX_PACKET_SIZE)
		throw MyStreamExcep("invalid packet size: " + toString(datasize));

	data.resize(datasize);

	stream.readTo(getData(), datasize);
}

void Packet::writeToStream(MyStream& stream)
{
	stream.write(getData(), data.size());
}
*/
