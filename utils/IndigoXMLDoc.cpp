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
#include "../utils/MemMappedFile.h"
#include "../maths/mathstypes.h"


const std::string IndigoXMLDoc::getErrorPositionDesc(const std::string& buffer_str, int offset_)
{
	// NOTE: PugiXML result offset seems to be one-based.

	assert(offset_ > 0);
	const int offset = (int)offset_ - 1; // Get zero based index.
	std::string line_desc;
	if(offset >= 0 && offset < buffer_str.size())
	{
		unsigned int line_num, column_num;
		StringUtils::getPosition(buffer_str, offset, line_num, column_num);

		const std::string line = StringUtils::getLineFromBuffer(buffer_str, myMax(0, (int)offset - (int)column_num));

		line_desc += "Line " + toString(line_num + 1) + ", column " + toString(column_num + 1) + ".";

		// Print out some of the XML with a caret under it (^) if the line is not too long.
		if(line.size() < 200)
		{
			line_desc += "\nXML around error:\n";
			line_desc += line;
			line_desc += "\n";
			for(int i=0; i<(int)column_num; ++i)
				line_desc += " ";
			line_desc += "^";
		}
	}

	return line_desc;
}


IndigoXMLDoc::IndigoXMLDoc(const std::string& path_)
:	path(path_)
{
	try
	{	
		MemMappedFile file(path);

		if(file.fileSize() == 0)
			throw IndigoXMLDocExcep("XML doc '" + path + "' was empty.");

		pugi::xml_parse_result result = doc.load_buffer(file.fileData(), file.fileSize(), pugi::parse_default, pugi::encoding_utf8);

		if(!result)
		{
			const std::string buffer_str((const char*)file.fileData(), file.fileSize());
			std::string line_desc = getErrorPositionDesc(buffer_str, result.offset);

			throw IndigoXMLDocExcep("Failed to open XML doc from path '" + path + "': " + result.description() + ": " + line_desc);
		}
	}
	catch(Indigo::Exception& e)
	{
		throw IndigoXMLDocExcep(e.what());
	}
}


// From memory buffer
IndigoXMLDoc::IndigoXMLDoc(const char* mem_buf, size_t len) // throws IndigoXMLDocExcep
{
	pugi::xml_parse_result result = doc.load_buffer(mem_buf, len, pugi::parse_default, pugi::encoding_utf8);

	if(!result)
	{
		const std::string buffer_str(mem_buf, len);
		std::string line_desc = getErrorPositionDesc(buffer_str, result.offset);

		throw IndigoXMLDocExcep("Failed to open XML doc from path '" + path + "': " + result.description() + ": " + line_desc);
	}
}


IndigoXMLDoc::~IndigoXMLDoc()
{
}


pugi::xml_node IndigoXMLDoc::getRootElement()  // throws IndigoXMLDocExcep if no such root element
{
	pugi::xml_node root_element = doc.first_child();
	if(!root_element)
		throw IndigoXMLDocExcep("Could not root element in file '" + path + "'.");

	return root_element;
}


pugi::xml_node IndigoXMLDoc::getRootElement(const std::string& name)  // throws IndigoXMLDocExcep if no such root element
{
	pugi::xml_node root_element = doc.child(name.c_str());
	if(!root_element)
		throw IndigoXMLDocExcep("Could not find root '" + name + "' element in file '" + path + "'.");

	return root_element;
}


void IndigoXMLDoc::saveDoc(const std::string& savepath)
{
	try
	{
		std::ofstream file(FileUtils::convertUTF8ToFStreamPath(savepath).c_str()); // NOTE: open in binary mode?

		doc.save(
			file,
			"\t", // indent (default)
			pugi::format_default, // flags (default)
			pugi::encoding_utf8
		);

		if(!file.good())
			throw IndigoXMLDocExcep("Failed to save XML document to '" + savepath + "'");
	}
	catch(Indigo::Exception& )
	{
		throw IndigoXMLDocExcep("Failed to open file '" + savepath + "' for writing.");
	}
}


#if BUILD_TESTS


#include "../utils/timer.h"
#include "../utils/ConPrint.h"
#include "../utils/MemMappedFile.h"
#include <cstring>


void IndigoXMLDoc::test()
{
	conPrint("IndigoXMLDoc::test()");

	const std::string header = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";

	//============ Parse simple XML doc ============
	{
		const std::string xml = header + "<root></root>";

		IndigoXMLDoc doc(xml.c_str(), xml.size());

		pugi::xml_node root = doc.getRootElement();

		testAssert(std::string(root.name()) == "root");
	}


	//============ Testing parsing of text content of an element ============
	{
		const std::string xml = header + "<root><a>content</a></root>";

		IndigoXMLDoc doc(xml.c_str(), xml.size());

		pugi::xml_node root = doc.getRootElement();

		testAssert(std::string(root.name()) == "root");

		pugi::xml_node a = root.child("a");

		testAssert(a != false);

		const std::string content = a.child_value();
		testAssert(content == "content");
	}


	//============ Testing parsing of text content of an element with a comment in the middle ============
	{
		const std::string xml = header + "<root><a>hello<!-- comment -->world</a></root>";

		IndigoXMLDoc doc(xml.c_str(), xml.size());

		pugi::xml_node root = doc.getRootElement();

		testAssert(std::string(root.name()) == "root");

		pugi::xml_node a = root.child("a");

		testAssert(a != false);

		const std::string content = a.child_value();
		testAssert(content == "hello");
	}


	//============ Testing parsing of malformed XML - empty file ============
	/*try
	{
		const std::string xml = "";

		IndigoXMLDoc doc(xml.c_str(), xml.size());

		failTest("Exception expected.");
	}
	catch(IndigoXMLDocExcep& e)
	{
		// Expected
		conPrint(e.what());
	}*/

	//============ Testing parsing of malformed XML with a single line ============
	try
	{
		const std::string xml = "<";

		IndigoXMLDoc doc(xml.c_str(), xml.size());

		failTest("Exception expected.");
	}
	catch(IndigoXMLDocExcep& e)
	{
		// Expected
		conPrint(e.what());
	}

	//============ Testing parsing of malformed XML with a single line ============
	try
	{
		const std::string xml = "<a></MEH>";

		IndigoXMLDoc doc(xml.c_str(), xml.size());

		failTest("Exception expected.");
	}
	catch(IndigoXMLDocExcep& e)
	{
		// Expected
		conPrint(e.what());
	}

	//============ Testing parsing of malformed XML with multiple lines ============
	try
	{
		const std::string xml = header + "<root>\n<a>\n</MEH>\n</root>";

		IndigoXMLDoc doc(xml.c_str(), xml.size());

		failTest("Exception expected.");
	}
	catch(IndigoXMLDocExcep& e)
	{
		// Expected
		conPrint(e.what());
	}







	//============ Perf test ============
	{
		{
			// Warm up cache
			/*{
				TiXmlDocument doc;
				doc.LoadFile(TestUtils::getIndigoTestReposDir() + "/testscenes/instancing_test4.igs");
			}*/

			const std::string path = TestUtils::getIndigoTestReposDir() + "/testscenes/instancing_test4.igs";
			const size_t filesize = FileUtils::getFileSize(path);

			const int NUM_ITERS = 1;

			/*for(int i=0; i<NUM_ITERS; ++i)
			{
				Timer timer;

				TiXmlDocument doc;
				doc.LoadFile(path);

				conPrint("TinyXML 1 elapsed: " + timer.elapsedStringNPlaces(3) + ", " + doubleToStringNDecimalPlaces(filesize * 1.0e-6 / timer.elapsed(), 3) + " MB/s");
				
			}*/

			/*for(int i=0; i<NUM_ITERS; ++i)
			{
				Timer timer;

				tinyxml2::XMLDocument doc;
				doc.LoadFile(path.c_str());

				conPrint("TinyXML 2 elapsed: " + timer.elapsedStringNPlaces(3) + ", " + doubleToStringNDecimalPlaces(filesize * 1.0e-6 / timer.elapsed(), 3) + " MB/s");
			}*/

			/*for(int i=0; i<NUM_ITERS; ++i)
			{
				Timer timer;

				tinyxml2::XMLDocument doc;

				MemMappedFile file(path);

				doc.Parse((const char*)file.fileData(), file.fileSize());

				conPrint("TinyXML 2 parse() from MemMappedFile elapsed: " + timer.elapsedStringNPlaces(3) + ", " + floatToStringNDecimalPlaces(filesize * 1.0e-6 / timer.elapsed(), 3) + " MB/s");
			}*/


			/*for(int i=0; i<NUM_ITERS; ++i)
			{
				Timer timer;

				rapidxml::xml_document<> doc;
				MemMappedFile file(path);
				// Load data
				std::vector<char> data(file.fileSize() + 1);
				std::memcpy(&data[0], file.fileData(), file.fileSize());
				data[file.fileSize()] = 0; // Null terminate

				doc.parse<0>(&data[0]);

				conPrint("rapidxml elapsed: " + timer.elapsedStringNPlaces(3) + ", " + doubleToStringNDecimalPlaces(filesize * 1.0e-6 / timer.elapsed(), 3) + " MB/s");
			}*/

			for(int i=0; i<NUM_ITERS; ++i)
			{
				Timer timer;

				pugi::xml_document doc;
				MemMappedFile file(path);

				pugi::xml_parse_result result = doc.load_buffer(file.fileData(), file.fileSize(), pugi::parse_default, pugi::encoding_utf8);

				conPrint("pugixml load_buffer() elapsed: " + timer.elapsedStringNPlaces(3) + ", " + doubleToStringNDecimalPlaces(filesize * 1.0e-6 / timer.elapsed(), 3) + " MB/s");
			}
			
			for(int i=0; i<NUM_ITERS; ++i)
			{
				Timer timer;

				pugi::xml_document doc;
				MemMappedFile file(path);

				std::vector<char> data(file.fileSize() + 1);
				std::memcpy(&data[0], file.fileData(), file.fileSize());
				data[file.fileSize()] = 0; // Null terminate

				pugi::xml_parse_result result = doc.load_buffer_inplace(&data[0], file.fileSize(), pugi::parse_default, pugi::encoding_utf8);

				conPrint("pugixml load_buffer_inplace() elapsed: " + timer.elapsedStringNPlaces(3) + ", " + doubleToStringNDecimalPlaces(filesize * 1.0e-6 / timer.elapsed(), 3) + " MB/s");
			}
		}



	}


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
