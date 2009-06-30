/*=====================================================================
IndigoXMLDoc.h
--------------
File created by ClassTemplate on Tue Jun 30 18:16:15 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __INDIGOXMLDOC_H_666_
#define __INDIGOXMLDOC_H_666_


#include "../tinyxml_2_4_3/tinyxml/tinyxml.h"


class IndigoXMLDocExcep
{
public:
	IndigoXMLDocExcep(const std::string& s_) : s(s_) {}
	~IndigoXMLDocExcep(){}
	const std::string& what() const { return s; }
private:
	std::string s;
};


/*=====================================================================
IndigoXMLDoc
------------

=====================================================================*/
class IndigoXMLDoc
{
public:
	IndigoXMLDoc(const std::string& path); // throws IndigoXMLDocExcep

	~IndigoXMLDoc();


	const TiXmlNode& getRootElement(const std::string& name) const;  // throws IndigoXMLDocExcep if no such root element

private:
	FILE* file;
	TiXmlDocument doc;
};


#endif //__INDIGOXMLDOC_H_666_
