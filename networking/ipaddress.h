/*=====================================================================
ipaddress.h
-----------
File created by ClassTemplate on Mon Mar 04 05:05:01 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __IPADDRESS_H_666_
#define __IPADDRESS_H_666_

#pragma warning(disable : 4786)//disable long debug name warning
#pragma warning(disable : 4290)//disable exception specification warning in VS2003

#include "mystream.h"
#include <string>



class MalformedIPStringExcep
{
public:
	MalformedIPStringExcep(){}
	~MalformedIPStringExcep(){}
};


/*=====================================================================
IPAddress
---------
Represents a 32 bit IPv4 address.
=====================================================================*/
class IPAddress
{
public:
	/*=====================================================================
	IPAddress
	---------
	
	=====================================================================*/
	inline IPAddress();
	inline IPAddress(unsigned int address);//'address' must be in network byte order
	//inline IPAddress(char* abcd);
	//IPAddress(unsigned char a, unsigned char b, unsigned char c, unsigned char d);
	IPAddress(const std::string& address) throw (MalformedIPStringExcep);//in form "255.255.255.255"

	inline IPAddress(const IPAddress& other);

	inline ~IPAddress();


	inline bool operator < (const IPAddress& other) const;
	inline bool operator == (const IPAddress& other) const;
	inline bool operator != (const IPAddress& other) const;


	//returns in network byte order as unsigned int
	inline unsigned int getAddr() const { return address; }

	void getBytes(unsigned char& a_out, unsigned char& b_out, 
						unsigned char& c_out, unsigned char& d_out) const;

	//void setCharArray(char* chars_out) const; //in char[4] form
	const std::string toString() const;//in form "255.255.255.255"

		
	void writeToStream(MyStream& stream) const;
	void setFromStream(MyStream& stream);

private:
	unsigned int address;//in network byte order

};

inline MyStream& operator << (MyStream& stream, const IPAddress& ipaddress)
{
	ipaddress.writeToStream(stream);
	return stream;
}

inline MyStream& operator >> (MyStream& stream, IPAddress& ipaddress)
{
	ipaddress.setFromStream(stream);
	return stream;
}




IPAddress::IPAddress()
:	address(0)
{}

IPAddress::IPAddress(unsigned int address_)
:	address(address_)
{}


	
IPAddress::IPAddress(const IPAddress& other)
:	address(other.address)
{}

IPAddress::~IPAddress()
{}

bool IPAddress::operator < (const IPAddress& other) const
{
	return address < other.address;
}


bool IPAddress::operator == (const IPAddress& other) const
{
	return address == other.address;
}

bool IPAddress::operator != (const IPAddress& other) const
{
	return address != other.address;
}

#endif //__IPADDRESS_H_666_




