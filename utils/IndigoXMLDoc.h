/*=====================================================================
IndigoXMLDoc.h
--------------
File created by ClassTemplate on Tue Jun 30 18:16:15 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __INDIGOXMLDOC_H_666_
#define __INDIGOXMLDOC_H_666_


#include "FileHandle.h"
#include <tinyxml/tinyxml.h>
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

	~IndigoXMLDoc();


	TiXmlElement& getRootElement();  // throws IndigoXMLDocExcep if no such root element
	TiXmlElement& getRootElement(const std::string& name);  // throws IndigoXMLDocExcep if no such root element

	void saveDoc(const std::string& path);
	
	static void test();

private:
	//FILE* file;
	//FileHandle file;
	TiXmlDocument doc;
	std::string path;
};


#endif //__INDIGOXMLDOC_H_666_
