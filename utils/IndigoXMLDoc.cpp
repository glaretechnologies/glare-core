/*=====================================================================
IndigoXMLDoc.cpp
----------------
File created by ClassTemplate on Tue Jun 30 18:16:15 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "IndigoXMLDoc.h"


#include "../utils/fileutils.h"
#include "../utils/stringutils.h"


IndigoXMLDoc::IndigoXMLDoc(const std::string& path)
:	file(NULL)
{
	// Open file.  Using FILE* method here to allow Unicode paths.
	file = FileUtils::openFile(path, "rb");

	if(!file)
		throw IndigoXMLDocExcep("Failed to open file '" + path + "'.");

	if(!doc.LoadFile(file))
	{
		if(doc.ErrorId() == TiXmlBase::TIXML_ERROR_OPENING_FILE)
		{
			throw IndigoXMLDocExcep("failed to open XML doc from path '" + path + "': " + std::string(doc.ErrorDesc()));
		}
		else
		{
			throw IndigoXMLDocExcep("failed to load XML doc from path '" + path + "': " +
				std::string(doc.ErrorDesc()) + " Line " + ::toString(doc.ErrorRow()) + ", column " +
				::toString(doc.ErrorCol()));
		}
	}
}


IndigoXMLDoc::~IndigoXMLDoc()
{
	if(file)
		fclose(file);
}


const TiXmlNode& IndigoXMLDoc::getRootElement(const std::string& name) const  // throws IndigoXMLDocExcep if no such root element
{
	const TiXmlNode* root_element = doc.FirstChild(name);
	if(!root_element)
		throw IndigoXMLDocExcep("could not find root '" + name + "' element");

	return *root_element;
}
