/*=====================================================================
IndigoXMLDoc.h
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "FileHandle.h"
#include "Exception.h"
#include <pugixml.hpp>
#include <string>


class IndigoXMLDocExcep : public glare::Exception
{
public:
	IndigoXMLDocExcep(const std::string& s_) : glare::Exception(s_) {}
	~IndigoXMLDocExcep(){}
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

	pugi::xml_document& getDocument() { return doc; }
	pugi::xml_node getRootElement();  // throws IndigoXMLDocExcep if no such root element
	pugi::xml_node getRootElement(const std::string& name);  // throws IndigoXMLDocExcep if no such root element

	void saveDoc(const std::string& path);

	static const std::string getErrorPositionDesc(const std::string& buffer_str, ptrdiff_t offset);
	
	static void test();

private:
	pugi::xml_document doc;
	std::string path;
};
