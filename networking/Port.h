/*=====================================================================
Port.h
------
File created by ClassTemplate on Mon Jul 07 18:58:34 2003
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include <string>


/*=====================================================================
Port
----

=====================================================================*/
class Port
{
public:
	Port();
	Port(unsigned short port);
	~Port();

	void operator = (const Port& other){ port = other.port; }

	bool operator == (const Port& other) const { return port == other.port; }
	bool operator != (const Port& other) const { return port != other.port; }

	bool operator < (const Port& other) const { return port < other.port; }

	const std::string toString() const;

	unsigned short getPort() const { return port; }

	int toInt() const { return (int)port; }

private:
	unsigned short port; // In host byte order
};
