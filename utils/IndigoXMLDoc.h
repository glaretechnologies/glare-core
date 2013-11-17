/*=====================================================================
IndigoXMLDoc.h
--------------
File created by ClassTemplate on Tue Jun 30 18:16:15 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __INDIGOXMLDOC_H_666_
#define __INDIGOXMLDOC_H_666_


#include "FileHandle.h"
#include <pugixml.hpp>
#include <string>


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

	// From memory buffer
	IndigoXMLDoc(const char* mem_buf, size_t len); // throws IndigoXMLDocExcep

	~IndigoXMLDoc();


	pugi::xml_node getRootElement();  // throws IndigoXMLDocExcep if no such root element
	pugi::xml_node getRootElement(const std::string& name);  // throws IndigoXMLDocExcep if no such root element

	void saveDoc(const std::string& path);

	static const std::string getErrorPositionDesc(const std::string& buffer_str, int offset);
	
	static void test();

private:
	pugi::xml_document doc;
	std::string path;
};


#endif //__INDIGOXMLDOC_H_666_
