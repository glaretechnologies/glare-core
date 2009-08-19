/*=====================================================================
IndigoXMLDoc.cpp
----------------
File created by ClassTemplate on Tue Jun 30 18:16:15 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "IndigoXMLDoc.h"


#include "../utils/fileutils.h"
#include "../utils/stringutils.h"


IndigoXMLDoc::IndigoXMLDoc(const std::string& path_)
:	file(NULL),
	path(path_)
{
	// Open file.  Using FILE* method here to allow Unicode paths.
	file = FileUtils::openFile(path, "rb");

	if(!file)
		throw IndigoXMLDocExcep("Failed to open file '" + path + "'.");

	if(!doc.LoadFile(file))
	{
		if(doc.ErrorId() == TiXmlBase::TIXML_ERROR_OPENING_FILE)
		{
			throw IndigoXMLDocExcep("Failed to open XML doc from path '" + path + "': " + std::string(doc.ErrorDesc()));
		}
		else
		{
			throw IndigoXMLDocExcep("failed to load XML doc from path '" + path + "': " +
				std::string(doc.ErrorDesc()) + "  (Line " + ::toString(doc.ErrorRow()) + ", column " +
				::toString(doc.ErrorCol()) + ".)");
		}
	}
}


IndigoXMLDoc::~IndigoXMLDoc()
{
	if(file)
		fclose(file);
}


TiXmlElement& IndigoXMLDoc::getRootElement(const std::string& name)  // throws IndigoXMLDocExcep if no such root element
{
	TiXmlElement* root_element = doc.FirstChildElement(name);
	if(!root_element)
		throw IndigoXMLDocExcep("Could not find root '" + name + "' element in file '" + path + "'.");

	return *root_element;
}


void IndigoXMLDoc::saveDoc(const std::string& savepath)
{
	FILE* savefile = FileUtils::openFile(savepath, "wb");

	if(!savefile)
		throw IndigoXMLDocExcep("Failed to open file '" + savepath + "' for writing.");

	if(!doc.SaveFile(savefile))
	{
		fclose(savefile);
		throw IndigoXMLDocExcep("Failed to save XML document to '" + savepath + "': " + std::string(doc.ErrorDesc()));
	}

	fclose(savefile);
}


void IndigoXMLDoc::test()
{
	for(int i=0; i<1; ++i)
	{
 		IndigoXMLDoc doc("../testscenes/airy_disc_test.igs");
	}
}
