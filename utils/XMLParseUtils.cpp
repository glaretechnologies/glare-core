/*=====================================================================
XMLParseUtils.cpp
-----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "XMLParseUtils.h"


#include <Exception.h>
#include <Parser.h>
#include <SharedStringTable.h>
#include <pugixml.hpp>
#include <sstream>
#include <cstring>


int32 XMLParseUtils::parseIntDirectly(pugi::xml_node elem)
{
	const char* const child_text = elem.child_value();

	Parser parser(child_text, std::strlen(child_text));

	// Parse any whitespace preceding the number
	parser.parseWhiteSpace();

	// Parse the number
	int32 value = 0;
	if(!parser.parseInt(value))
		throw glare::Exception("Failed to parse integer from '" + std::string(child_text) + "'." + elemContext(elem));

	// Parse any trailing whitespace
	parser.parseWhiteSpace();

	// We should be at the end of the string now
	if(parser.notEOF())
		throw glare::Exception("Parse error while parsing integer from '" + std::string(child_text) + "'." + elemContext(elem));

	return value;
}


int32 XMLParseUtils::parseInt(pugi::xml_node elem, const char* elemname)
{
	pugi::xml_node childnode = elem.child(elemname);
	if(!childnode)
		throw glare::Exception(std::string("could not find element '") + elemname + "'." + elemContext(elem));

	return parseIntDirectly(childnode);
}


int32 XMLParseUtils::parseIntWithDefault(pugi::xml_node parent_elem, const char* elemname, int32 default_val)
{
	pugi::xml_node childnode = parent_elem.child(elemname);
	if(childnode)
		return parseIntDirectly(childnode);
	else
		return default_val;
}


uint32 XMLParseUtils::parseUInt(pugi::xml_node elem, const char* elemname)
{
	pugi::xml_node childnode = elem.child(elemname);
	if(!childnode)
		throw glare::Exception(std::string("could not find element '") + elemname + "'." + elemContext(elem));

	const char* const child_text = childnode.child_value();

	Parser parser(child_text, std::strlen(child_text));

	// Parse any whitespace preceding the number
	parser.parseWhiteSpace();

	// Parse the number
	uint32 value = 0;
	if(!parser.parseUnsignedInt(value))
		throw glare::Exception("Failed to parse unsigned integer from '" + std::string(child_text) + "'." + elemContext(childnode));

	// Parse any trailing whitespace
	parser.parseWhiteSpace();

	// We should be at the end of the string now
	if(parser.notEOF())
		throw glare::Exception("Parse error while parsing unsigned integer from '" + std::string(child_text) + "'." + elemContext(childnode));

	return value;
}


uint64 XMLParseUtils::parseUInt64Directly(pugi::xml_node elem)
{
	const char* const child_text = elem.child_value();

	Parser parser(child_text, std::strlen(child_text));

	// Parse any whitespace preceding the number
	parser.parseWhiteSpace();

	// Parse the number
	uint64 value = 0;
	if(!parser.parseUInt64(value))
		throw glare::Exception("Failed to parse unsigned integer from '" + std::string(child_text) + "'." + elemContext(elem));

	// Parse any trailing whitespace
	parser.parseWhiteSpace();

	// We should be at the end of the string now
	if(parser.notEOF())
		throw glare::Exception("Parse error while parsing unsigned integer from '" + std::string(child_text) + "'." + elemContext(elem));

	return value;
}


uint64 XMLParseUtils::parseUInt64(pugi::xml_node elem, const char* elemname)
{
	pugi::xml_node childnode = elem.child(elemname);
	if(!childnode)
		throw glare::Exception(std::string("could not find element '") + elemname + "'." + elemContext(elem));

	return parseUInt64Directly(childnode);
}


uint64 XMLParseUtils::parseUInt64WithDefault(pugi::xml_node elem, const char* elemname, uint64 default_val)
{
	pugi::xml_node childnode = elem.child(elemname);
	if(childnode)
		return parseUInt64Directly(childnode);
	else
		return default_val;
}


//uint32 XMLParseUtils::parseUInt(const char* text)
//{
//	Parser parser(text, std::strlen(text));
//
//	// Parse any whitespace preceding the number
//	parser.parseWhiteSpace();
//
//	// Parse the number
//	unsigned int value = 0;
//	if(!parser.parseUnsignedInt(value))
//		throw glare::Exception("Failed to parse unsigned integer from '" + std::string(text) + "'.");
//
//	// Parse any trailing whitespace
//	parser.parseWhiteSpace();
//
//	// We should be at the end of the string now
//	if(parser.notEOF())
//		throw glare::Exception("Parse error while parsing unsigned integer from '" + std::string(text) + "'.");
//
//	return value;
//}


const std::string XMLParseUtils::parseString(pugi::xml_node parent_elem, const char* child_elem_name)
{
	pugi::xml_node childnode = parent_elem.child(child_elem_name);

	if(!childnode)
		throw glare::Exception(std::string("could not find element '") + child_elem_name + "'." + elemContext(parent_elem));

	return childnode.child_value();
}


const std::string XMLParseUtils::parseStringWithDefault(pugi::xml_node parent_elem, const char* elemname, const string_view default_val)
{
	pugi::xml_node childnode = parent_elem.child(elemname);
	if(childnode)
		return childnode.child_value();
	else
		return toString(default_val);
}


glare::SharedImmutableString XMLParseUtils::parseSharedImmutableStringWithDefault(pugi::xml_node parent_elem, const char* elemname, const char* default_val, glare::SharedStringTable* table)
{
	pugi::xml_node childnode = parent_elem.child(elemname);
	if(childnode)
		return glare::getOrMakeSharedImmutableString(childnode.child_value(), table);
	else
		return glare::getOrMakeSharedImmutableString(default_val, table);
}


double XMLParseUtils::parseDoubleDirectly(pugi::xml_node elem)
{
	const char* const child_text = elem.child_value();

	Parser parser(child_text, std::strlen(child_text));

	// Parse any whitespace preceding the number
	parser.parseWhiteSpace();

	// Parse the number
	double value = 0.0;
	if(!parser.parseDouble(value))
		throw glare::Exception("Failed to parse double from '" + std::string(child_text) + "'." + elemContext(elem));

	// Parse any trailing whitespace
	parser.parseWhiteSpace();

	// We should be at the end of the string now
	if(parser.notEOF())
		throw glare::Exception("Parse error while parsing double from '" + std::string(child_text) + "'." + elemContext(elem));

	return value;
}


double XMLParseUtils::parseDouble(pugi::xml_node elem, const char* elemname)
{
	pugi::xml_node childnode = elem.child(elemname);
	if(!childnode)
		throw glare::Exception(std::string("could not find element '") + elemname + "'." + elemContext(elem));

	return parseDoubleDirectly(childnode);
}


double XMLParseUtils::parseDoubleWithDefault(pugi::xml_node parent_elem, const char* elemname, double default_val)
{
	pugi::xml_node child = parent_elem.child(elemname);
	if(child)
		return parseDoubleDirectly(child);
	else
		return default_val;
}


float XMLParseUtils::parseFloatDirectly(pugi::xml_node elem)
{
	return (float)parseDoubleDirectly(elem);
}


float XMLParseUtils::parseFloat(pugi::xml_node elem, const char* elemname)
{
	return (float)parseDouble(elem, elemname);
}


float XMLParseUtils::parseFloatWithDefault(pugi::xml_node elem, const char* elemname, float default_val)
{
	return (float)parseDoubleWithDefault(elem, elemname, default_val);
}


bool XMLParseUtils::parseBoolDirectly(pugi::xml_node elem)
{
	const char* const child_text = elem.child_value();

	Parser parser(child_text, std::strlen(child_text));
	parser.parseWhiteSpace();
	bool res;
	if(parser.parseCString("true"))
		res = true;
	else if(parser.parseCString("false"))
		res = false;
	else
		throw glare::Exception("Invalid boolean value (Should be 'true' or 'false')." + elemContext(elem));

	// Parse any trailing whitespace
	parser.parseWhiteSpace();

	// We should be at the end of the string now
	if(parser.notEOF())
		throw glare::Exception("Parse error while parsing boolean value from '" + std::string(child_text) + "'." + elemContext(elem));

	return res;
}


bool XMLParseUtils::parseBool(pugi::xml_node elem, const char* elemname)
{
	pugi::xml_node childnode = elem.child(elemname);
	if(!childnode)
		throw glare::Exception(std::string("could not find element '") + elemname + "'." + elemContext(elem));

	return parseBoolDirectly(childnode);
}


bool XMLParseUtils::parseBoolWithDefault(pugi::xml_node parent_elem, const char* elemname, bool default_val)
{
	pugi::xml_node child = parent_elem.child(elemname);
	if(child)
		return parseBoolDirectly(child);
	else
		return default_val;
}


Colour3f XMLParseUtils::parseColour3fWithDefault(pugi::xml_node elem, const char* elemname, const Colour3f& default_val)
{
	pugi::xml_node childnode = elem.child(elemname);
	if(!childnode)
		return default_val;

	const char* const child_text = childnode.child_value();

	Parser parser(child_text, std::strlen(child_text));

	parser.parseWhiteSpace();

	Colour3f v;
	if(!parser.parseFloat(v.r))
		throw glare::Exception("Failed to parse Vec3 from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(childnode));
	parser.parseWhiteSpace();
	if(!parser.parseFloat(v.g))
		throw glare::Exception("Failed to parse Vec3 from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(childnode));
	parser.parseWhiteSpace();
	if(!parser.parseFloat(v.b))
		throw glare::Exception("Failed to parse Vec3 from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(childnode));
	parser.parseWhiteSpace();

	// We should be at the end of the string now
	if(parser.notEOF())
		throw glare::Exception("Parse error while parsing Vec3 from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(childnode));

	return v;
}


Vec3d XMLParseUtils::parseVec3dDirectly(pugi::xml_node elem)
{
	const char* const child_text = elem.child_value();

	Parser parser(child_text, std::strlen(child_text));

	parser.parseWhiteSpace();

	Vec3d v;
	if(!parser.parseDouble(v.x))
		throw glare::Exception("Failed to parse Vec3 from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(elem));
	parser.parseWhiteSpace();
	if(!parser.parseDouble(v.y))
		throw glare::Exception("Failed to parse Vec3 from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(elem));
	parser.parseWhiteSpace();
	if(!parser.parseDouble(v.z))
		throw glare::Exception("Failed to parse Vec3 from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(elem));
	parser.parseWhiteSpace();

	// We should be at the end of the string now
	if(parser.notEOF())
		throw glare::Exception("Parse error while parsing Vec3 from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(elem));

	return v;
}


Vec3d XMLParseUtils::parseVec3d(pugi::xml_node elem, const char* elemname)
{
	pugi::xml_node childnode = elem.child(elemname);
	if(!childnode)
		throw glare::Exception(std::string("could not find element '") + elemname + "'." + elemContext(elem));

	return parseVec3dDirectly(childnode);
}


Vec3d XMLParseUtils::parseVec3dWithDefault(pugi::xml_node elem, const char* elemname, const Vec3d& default_val)
{
	pugi::xml_node child = elem.child(elemname);
	if(child)
		return parseVec3dDirectly(child);
	else
		return default_val;
}


Vec3f XMLParseUtils::parseVec3fDirectly(pugi::xml_node elem)
{
	return toVec3f(parseVec3dDirectly(elem));
}


Vec3f XMLParseUtils::parseVec3f(pugi::xml_node elem, const char* elemname)
{
	return toVec3f(parseVec3d(elem, elemname));
}


Vec3f XMLParseUtils::parseVec3fWithDefault(pugi::xml_node elem, const char* elemname, const Vec3f& default_val)
{
	return toVec3f(parseVec3dWithDefault(elem, elemname, toVec3d(default_val)));
}


Matrix2f XMLParseUtils::parseMatrix2f(pugi::xml_node elem, const char* elemname)
{
	pugi::xml_node childnode = elem.child(elemname);
	if(!childnode)
		throw glare::Exception(std::string("could not find element '") + elemname + "'." + XMLParseUtils::elemContext(elem));

	const char* const child_text = childnode.child_value();

	Parser parser(child_text, std::strlen(child_text));

	parser.parseWhiteSpace();

	Matrix2f m;
	if(!parser.parseFloat(m.e[0]))
		throw glare::Exception("Failed to parse Matrix2f from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(childnode));
	parser.parseWhiteSpace();
	if(!parser.parseFloat(m.e[1]))
		throw glare::Exception("Failed to parse Matrix2f from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(childnode));
	parser.parseWhiteSpace();
	if(!parser.parseFloat(m.e[2]))
		throw glare::Exception("Failed to parse Matrix2f from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(childnode));
	parser.parseWhiteSpace();
	if(!parser.parseFloat(m.e[3]))
		throw glare::Exception("Failed to parse Matrix2f from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(childnode));
	parser.parseWhiteSpace();

	// We should be at the end of the string now
	if(parser.notEOF())
		throw glare::Exception("Parse error while parsing Matrix2f from '" + std::string(std::string(child_text)) + "'." + XMLParseUtils::elemContext(childnode));

	return m;
}


pugi::xml_node XMLParseUtils::getChildElement(pugi::xml_node elem, const char* name)
{
	pugi::xml_node e = elem.child(name);
	if(!e)
		throw glare::Exception(std::string("Failed to find expected element '") + name + "'." + elemContext(elem));
	return e;
}


const char* XMLParseUtils::getElementText(pugi::xml_node elem)
{
	return elem.child_value();
}


static const std::string xmlStackTrace(pugi::xml_node elem)
{
	std::vector<std::string> node_names;

	pugi::xml_node current = elem;
	while(current)
	{
		if(current.type() != pugi::node_document)
			node_names.push_back(current.name());
		current = current.parent();
	}

	std::string s;
	for(int i=(int)node_names.size()-1; i >= 0; --i)
		s += node_names[i] + (i > 0 ? " / " : "");

	return s;
}



const std::string XMLParseUtils::elemContext(pugi::xml_node elem)
{
	//NOTE: if we have a pointer to the original buffer, could use pugi::xml_node::offset_debug().

	// Serialise element so we can print it out
	std::ostringstream stream;
	elem.print(stream);

	std::string buffer = stream.str();

	// If string is too long, just take the first 80 chars.
	if(buffer.length() > 80)
		buffer = buffer.substr(0, 80) + "...";

	return std::string("  (In element '") + xmlStackTrace(elem) + "'):\n" + buffer;
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "IndigoXMLDoc.h"
#include "Timer.h"
#include "ConPrint.h"


void XMLParseUtils::test()
{
	//==================================== Test failure of parseInt() and elemContext() in error message ====================================
	try
	{
		const std::string s = "<root><a>bleh</a></root>";

		IndigoXMLDoc doc(s.c_str(), s.size());

		pugi::xml_node root = doc.getRootElement();

		parseInt(root, "a");

		failTest("");
	}
	catch(glare::Exception& e)
	{
		conPrint(e.what());
	}


	//==================================== Test parseIntDirectly() ====================================
	try
	{
		{
			const std::string s = "<root><a>10</a></root>";

			IndigoXMLDoc doc(s.c_str(), s.size());
			pugi::xml_node a_node = getChildElement(doc.getRootElement(), "a");
			testAssert(parseIntDirectly(a_node) == 10);
		}

		// Test with some leading and trailing whitespace
		{
			const std::string s = "<root><a>  -10	</a></root>";

			IndigoXMLDoc doc(s.c_str(), s.size());
			pugi::xml_node a_node = getChildElement(doc.getRootElement(), "a");
			testAssert(parseIntDirectly(a_node) == -10);
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}


	//==================================== Test parseInt() ====================================
	try
	{
		{
			const std::string s = "<root><a>1</a></root>";

			IndigoXMLDoc doc(s.c_str(), s.size());
			pugi::xml_node root = doc.getRootElement();

			const int x = parseInt(root, "a");
			testAssert(x == 1);
		}

		{
			const std::string s = "<root><a>1</a><b>123</b>  <c>  -3456436 </c>  <d> 0 </d></root>";

			IndigoXMLDoc doc(s.c_str(), s.size());
			pugi::xml_node root = doc.getRootElement();

			testAssert(parseInt(root, "a") == 1);
			testAssert(parseInt(root, "b") == 123);
			testAssert(parseInt(root, "c") == -3456436);
			testAssert(parseInt(root, "d") == 0);
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	// Test for invalid value
	try
	{
		const std::string s = "<root><a>bleh</a></root>";
		IndigoXMLDoc doc(s.c_str(), s.size());
		pugi::xml_node root = doc.getRootElement();

		parseInt(root, "a");

		failTest("Exception expected");
	}
	catch(glare::Exception& e)
	{
		conPrint(e.what());
	}


	
	//==================================== Test parseIntWithDefault() ====================================
	try
	{
		{
			const std::string s = "<root><a>1</a></root>";

			IndigoXMLDoc doc(s.c_str(), s.size());
			pugi::xml_node root = doc.getRootElement();

			testAssert(parseIntWithDefault(root, "a", 10) == 1);
			testAssert(parseIntWithDefault(root, "b", 10) == 10);
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	//==================================== Test parseUInt() ====================================
	try
	{
		{
			const std::string s = "<root><a>123</a></root>";

			IndigoXMLDoc doc(s.c_str(), s.size());
			pugi::xml_node root = doc.getRootElement();

			testAssert(parseUInt(root, "a") == 123);
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	try
	{
		const std::string s = "<root><a>-123</a></root>";
		IndigoXMLDoc doc(s.c_str(), s.size());
		pugi::xml_node root = doc.getRootElement();

		parseUInt(root, "a");

		failTest("Exception expected");
	}
	catch(glare::Exception& e)
	{
		conPrint(e.what());
	}


	//==================================== Test parseString() ====================================
	try
	{
		{
			const std::string s = "<root><a>hello</a></root>";
			IndigoXMLDoc doc(s.c_str(), s.size());

			testAssert(parseString(doc.getRootElement(), "a") == "hello");
		}

		// Test with leading and trailing whitespace around text
		{
			const std::string s = "<root><a>  hello  </a></root>";
			IndigoXMLDoc doc(s.c_str(), s.size());

			std::string astr = parseString(doc.getRootElement(), "a");
			testAssert(astr == "  hello  ");
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	try
	{
		const std::string s = "<root><a>hello</a></root>";
		IndigoXMLDoc doc(s.c_str(), s.size());
		pugi::xml_node root = doc.getRootElement();

		parseString(root, "b");

		failTest("Exception expected");
	}
	catch(glare::Exception& e)
	{
		conPrint(e.what());
	}


	//==================================== Test parseDouble() ====================================
	try
	{
		{
			const std::string s = "<root><a>123.456</a></root>";

			IndigoXMLDoc doc(s.c_str(), s.size());
			pugi::xml_node root = doc.getRootElement();

			testAssert(parseDouble(root, "a") == 123.456);
		}

		{
			const std::string s = "<root><a>0</a></root>";

			IndigoXMLDoc doc(s.c_str(), s.size());
			pugi::xml_node root = doc.getRootElement();

			testAssert(parseDouble(root, "a") == 0);
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	try
	{
		const std::string s = "<root><a>hello</a></root>";
		IndigoXMLDoc doc(s.c_str(), s.size());
		pugi::xml_node root = doc.getRootElement();

		parseDouble(root, "a");

		failTest("Exception expected");
	}
	catch(glare::Exception& e)
	{
		conPrint(e.what());
	}


	//==================================== Test parseBool() ====================================
	try
	{
		{
			const std::string s = "<root><a>true</a></root>";
			IndigoXMLDoc doc(s.c_str(), s.size());
			testAssert(parseBool(doc.getRootElement(), "a") == true);
		}

		{
			const std::string s = "<root><a>false</a></root>";
			IndigoXMLDoc doc(s.c_str(), s.size());
			testAssert(parseBool(doc.getRootElement(), "a") == false);
		}

		// Test with whitespace
		{
			const std::string s = "<root><a>   false \t</a></root>";
			IndigoXMLDoc doc(s.c_str(), s.size());
			testAssert(parseBool(doc.getRootElement(), "a") == false);
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	// Test invalid boolean
	try
	{
		const std::string s = "<root><a> falseBLEH </a></root>";
		IndigoXMLDoc doc(s.c_str(), s.size());
		parseBool(doc.getRootElement(), "a");
		failTest("Exception expected");
	}
	catch(glare::Exception& e)
	{
		conPrint(e.what());
	}


	//============ Perf test ============
	if(false)
	{
		const int NUM_ITERS = 1000000;

		{
			const std::string s = "<root><a>1</a></root>";
			IndigoXMLDoc doc(s.c_str(), s.size());
			pugi::xml_node root = doc.getRootElement();

			int sum = 0;
			Timer timer;
			for(int i=0; i<NUM_ITERS; ++i)
			{
				const int x = parseInt(root, "a");
				sum += x;
			}

			const double elapsed = timer.elapsed();
			printVar(sum);
			conPrint("parseInt() time: " + doubleToStringNDecimalPlaces(elapsed * 1.0e9 / NUM_ITERS, 5) + " ns");
		}
	}
}


#endif // BUILD_TESTS
