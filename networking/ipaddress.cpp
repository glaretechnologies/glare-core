/*=====================================================================
ipaddress.cpp
-------------
File created by ClassTemplate on Mon Mar 04 05:05:01 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "ipaddress.h"

//#include "mysocket.h"
//#include "packet.h"
#include "../utils/stringutils.h"
#if defined(WIN32) || defined(WIN64)
#include <winsock.h>
#else
#include <netinet/in.h>
#endif
/*
IPAddress::IPAddress()
{
	
}


IPAddress::~IPAddress()
{
	
}*/

/*IPAddress::IPAddress(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
{
	address = ((unsigned int)a << 24) + 
				((unsigned int)b << 16) + 
				((unsigned int)c << 8) + 
				(unsigned int)d;
}*/

IPAddress::IPAddress(const std::string& addstring) throw (MalformedIPStringExcep)//in form "255.255.255.255"
{
	//nasty piece of code this, there is some library call to do it but..

	if(addstring.size() == 0)
		throw MalformedIPStringExcep();

	int a, b, c, d;

	int nextstartpos;
	{
	const int dotpos = addstring.find_first_of('.');
	if(dotpos == std::string::npos)
		throw MalformedIPStringExcep();

	a = stringToInt(addstring.substr(0, dotpos));
	nextstartpos = dotpos + 1;
	}

	{
	const int dotpos = addstring.find_first_of('.', nextstartpos);
	if(dotpos == std::string::npos)
		throw MalformedIPStringExcep();

	const int byte_len = dotpos - nextstartpos;
	if(byte_len < 1 || byte_len > 3)
		throw MalformedIPStringExcep();

	b = stringToInt(addstring.substr(nextstartpos, byte_len));
	nextstartpos = dotpos + 1;
	}

	{
	const int dotpos = addstring.find_first_of('.', nextstartpos);
	if(dotpos == std::string::npos)
		throw MalformedIPStringExcep();

	const int byte_len = dotpos - nextstartpos;
	if(byte_len < 1 || byte_len > 3)
		throw MalformedIPStringExcep();

	c = stringToInt(addstring.substr(nextstartpos, byte_len));
	nextstartpos = dotpos + 1;
	}

	{
	const int byte_len = addstring.size() - nextstartpos;
	if(byte_len < 1 || byte_len > 3)
		throw MalformedIPStringExcep();

	d = stringToInt(addstring.substr(nextstartpos, byte_len));
	}

	if(a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255)
		throw MalformedIPStringExcep();


	//address = ((unsigned int)a << 24) + //make a most significant byte etc...
	//			((unsigned int)b << 16) + 
	//			((unsigned int)c << 8) + 
	//			(unsigned int)d;
	((unsigned char*)&address)[0] = (unsigned char)a;
	((unsigned char*)&address)[1] = (unsigned char)b;
	((unsigned char*)&address)[2] = (unsigned char)c;
	((unsigned char*)&address)[3] = (unsigned char)d;

	//-----------------------------------------------------------------
	//convert to network byte order for 'address'
	//-----------------------------------------------------------------
	//address = htonl(address_host_byte_order);

}


void IPAddress::getBytes(unsigned char& a_out, unsigned char& b_out, 
						unsigned char& c_out, unsigned char& d_out) const
{

	//a_out = ((address & 0xFF000000) >> 24);
	//b_out = ((address & 0x00FF0000) >> 16);
	//c_out = ((address & 0x0000FF00) >> 8);
	//d_out = ((address & 0x000000FF));

	/*unsigned int hostbyteorderaddr = ntohl(address);//get in host byte order
	
	a_out = ((unsigned char*)(&hostbyteorderaddr))[0];
	b_out = ((unsigned char*)(&hostbyteorderaddr))[1];
	c_out = ((unsigned char*)(&hostbyteorderaddr))[2];
	d_out = ((unsigned char*)(&hostbyteorderaddr))[3];*/
	
	//In network byte order, the most significant byte is first, ie 'a' should be first
	//byte, or the 0th byte in the word
	a_out = ((unsigned char*)(&address))[0];
	b_out = ((unsigned char*)(&address))[1];
	c_out = ((unsigned char*)(&address))[2];
	d_out = ((unsigned char*)(&address))[3];

}


void IPAddress::writeToStream(MyStream& stream) const
{
	//-----------------------------------------------------------------
	//convert to h.b.o. as will be converted to n.b.o. by the stream implementation
	//-----------------------------------------------------------------
	unsigned int hbo_address = ntohl(address);
	stream.write(hbo_address);
}


void IPAddress::setFromStream(MyStream& stream)
{
	//int a;
	//stream.readTo(a);
	//address = (unsigned int)a;

	stream.readTo(address);

	//-----------------------------------------------------------------
	//convert to n.b.o. as stream will have switched it to h.b.o.
	//-----------------------------------------------------------------
	address = htonl(address);
}

const std::string IPAddress::toString() const//in form "255.255.255.255
{
	unsigned char a, b, c, d;
	getBytes(a, b, c, d);

	return intToString(a) + "." + intToString(b) + "." + 
							intToString(c) + "." + intToString(d);
}
