/*=====================================================================
packet.cpp
----------
File created by ClassTemplate on Sun Apr 14 10:14:17 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "packet.h"


#include "assert.h"
#include "../maths/vec3.h"
#include "../utils/stringutils.h"

#if defined(WIN32) || defined(WIN64)
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

	assert(sizeof(char) == 1);
}




Packet::~Packet()
{
}





void Packet::write(float x)
{
	assert(sizeof(unsigned int) == sizeof(float));

	const unsigned int writeindex = data.size();
	data.resize(writeindex + sizeof(float));

	*(unsigned int*)(getData() + writeindex) = htonl(*((unsigned int*)&x));
}

void Packet::write(int x)
{
	const unsigned int writeindex = data.size();
	data.resize(writeindex + sizeof(int));

	*(unsigned int*)(getData() + writeindex) = htonl(*((unsigned int*)&x));
}

void Packet::write(unsigned short x)
{
	const unsigned int writeindex = data.size();
	data.resize(writeindex + sizeof(unsigned short));

	*(unsigned short*)(getData() + writeindex) = htons(x);
}

void Packet::write(unsigned char x)
{
	const unsigned int writeindex = data.size();
	data.resize(writeindex + sizeof(unsigned char));

	*(unsigned char*)(getData() + writeindex) = x;

}
void Packet::write(char x)
{
	const unsigned int writeindex = data.size();
	data.resize(writeindex + sizeof(unsigned char));

	*(unsigned char*)(getData() + writeindex) = *((unsigned char*)&x);
}


/*void Packet::write(const Vec3& vec)
{
	write(vec.x);
	write(vec.y);
	write(vec.z);
}*/

void Packet::write(const void* src, int numbytes)
{
	const unsigned int writeindex = data.size();
	data.resize(writeindex + numbytes);

	for(int i=0; i<numbytes; ++i)
	{
		data[writeindex + i] = *((char*)src + i);
	}
}


void Packet::write(const std::string& s)//writes null-terminated string
{
	//assert(index + s.size() + 1 <= MAX_PACKETSIZE);
	/*const unsigned int writeindex = data.size();
	data.resize(writeindex + s.size() + 1);


	for(unsigned int i=0; i<s.size()+1; ++i)
	{
		*(getData() + writeindex + i) = s.c_str()[i];
	}*/

	// Write length of string
	write((int)s.length());

	// Write string data
	write(&(*s.begin()), s.length());

}
	
void Packet::readTo(float& x)
{
	assert(sizeof(unsigned int) == sizeof(float));


	if(readindex + sizeof(float) > data.size())
		throw MyStreamExcep("read past end of packet.");

	unsigned int xi = ntohl(*(unsigned int*)(getData() + readindex));
	x = *((float*)&xi);
	readindex += sizeof(float);
}

void Packet::readTo(int& x)
{
	if(readindex + sizeof(int) > data.size())
		throw MyStreamExcep("read past end of packet.");

	unsigned int xi = ntohl(*((unsigned int*)(getData() + readindex)));
	x = *((int*)&xi);
	readindex += sizeof(int);
}

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
}

/*void Packet::readTo(Vec3& vec)
{
	readTo(vec.x);
	readTo(vec.y);
	readTo(vec.z);
}*/

void Packet::readTo(std::string& s, int maxlength)
{
	/*std::vector<char> buffer(1000);

	int i = 0;
	while(1)
	{
		buffer.push_back('\0');
		readTo(buffer[i]);

		if(buffer[i] == '\0')
			break;

		//TODO: break after looping to long
		++i;
	}

	s = &(*buffer.begin());*/
	//assert(0);

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

