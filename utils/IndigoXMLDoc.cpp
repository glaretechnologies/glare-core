/*=====================================================================
IndigoXMLDoc.cpp
----------------
File created by ClassTemplate on Tue Jun 30 18:16:15 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "IndigoXMLDoc.h"


#include "../utils/fileutils.h"
#include "../utils/stringutils.h"
#include "../indigo/TestUtils.h"
#include "../utils/Exception.h"


IndigoXMLDoc::IndigoXMLDoc(const std::string& path_)
:	path(path_)
{
	try
	{	
		// Open file.  Using FILE* method here to allow Unicode paths.
		FileHandle file(path, "rb");

		if(!doc.LoadFile(file.getFile()))
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
	catch(Indigo::Exception& e)
	{
		throw IndigoXMLDocExcep(e.what());
	}
}


IndigoXMLDoc::~IndigoXMLDoc()
{
}


TiXmlElement& IndigoXMLDoc::getRootElement()  // throws IndigoXMLDocExcep if no such root element
{
	TiXmlElement* root_element = doc.FirstChildElement();
	if(!root_element)
		throw IndigoXMLDocExcep("Could not root element in file '" + path + "'.");

	return *root_element;
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
	try
	{
		FileHandle savefile(savepath, "wb");

		if(!doc.SaveFile(savefile.getFile()))
			throw IndigoXMLDocExcep("Failed to save XML document to '" + savepath + "': " + std::string(doc.ErrorDesc()));
	}
	catch(Indigo::Exception& )
	{
		throw IndigoXMLDocExcep("Failed to open file '" + savepath + "' for writing.");
	}
}

#if (BUILD_TESTS)
void IndigoXMLDoc::test()
{
	try
	{
		for(int i=0; i<1; ++i)
		{
			IndigoXMLDoc doc(TestUtils::getIndigoTestReposDir() + "/testscenes/airy_disc_test.igs");
		}
	}
	catch(IndigoXMLDocExcep& )
	{
		failTest("IndigoXMLDocExcep");
	}
}
#endif
