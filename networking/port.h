/*=====================================================================
port.h
------
File created by ClassTemplate on Mon Jul 07 18:58:34 2003
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PORT_H_666_
#define __PORT_H_666_


// #pragma warning(disable : 4786)//disable long debug name warning



//#include "mystream.h"
#include <string>


/*=====================================================================
Port
----

=====================================================================*/
class Port
{
public:
	/*=====================================================================
	Port
	----
	
	=====================================================================*/
	Port();
	Port(unsigned short port);

	~Port();

	void operator = (const Port& other){ port = other.port; }

	bool operator == (const Port& other) const { return port == other.port; }
	bool operator != (const Port& other) const { return port != other.port; }

	bool operator < (const Port& other) const { return port < other.port; }

	const std::string toString() const;

	unsigned short getPort() const { return port; }
	void setPort(unsigned short newport){ port = newport; }

	int toInt() const { return (int)port; }

private:
	unsigned short port;//in host byte order

};



/*inline MyStream& operator << (MyStream& stream, const Port& port)
{
	stream << port.getPort();
	return stream;
}

inline MyStream& operator >> (MyStream& stream, Port& port)
{
	unsigned short portnum;
	stream >> portnum;
	port.setPort(portnum);
	return stream;
}*/



#endif //__PORT_H_666_




