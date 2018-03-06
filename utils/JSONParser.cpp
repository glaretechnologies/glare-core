/*=====================================================================
JSONParser.cpp
-------------------
Copyright Glare Technologies Limited 2018 -
Generated at 2018-03-05 01:34:58 +1300
=====================================================================*/
#include "JSONParser.h"


#include "MemMappedFile.h"
#include "Parser.h"
#include "Exception.h"


JSONParser::JSONParser()
{}


JSONParser::~JSONParser()
{}


std::string JSONParser::parseString(Parser& p)
{
	std::string s;

	// Parse opening "
	if(!p.parseChar('"'))
		throw Indigo::Exception("Expected \"" + errorContext(p));
	
	while(p.notEOF() && p.current() != '"')
	{
		if(p.current() == '\\')
		{
			p.advance();
			if(p.eof())
				throw Indigo::Exception("EOF in escape sequence." + errorContext(p));
			s.push_back(p.current());
		}
		else
			s.push_back(p.current());
		p.advance();
	}

	// Parse closing "
	if(!p.parseChar('"'))
		throw Indigo::Exception("Expected \"" + errorContext(p));

	return s;
}


uint32 JSONParser::parseNode(Parser& parser)
{
	switch(parser.current())
	{
	case '{':
		return parseObject(parser);
	case '[':
		return parseArray(parser);
	case '"':
		return parseStringNode(parser);
	case 't':
		return parseTrue(parser);
	case 'f':
		return parseFalse(parser);
	case 'n':
		return parseNull(parser);
	case '-':
		return parseNumber(parser);
	default:
		if(::isNumeric(parser.current()))
			return parseNumber(parser);
		else
			throw Indigo::Exception("Unexpected character '" + std::string(1, parser.current()) + "'" + errorContext(parser));
	}
}


uint32 JSONParser::parseStringNode(Parser& p)
{
	const uint32 node_index = (uint32)nodes.size();
	nodes.push_back(JSONNode());
	nodes[node_index].type = JSONNode::Type_String;
	nodes[node_index].string_v = parseString(p);
	return node_index;
}


uint32 JSONParser::parseTrue(Parser& p)
{
	if(!p.parseCString("true"))
		throw Indigo::Exception("Expected 'true'" + errorContext(p));

	const uint32 node_index = (uint32)nodes.size();
	nodes.push_back(JSONNode());
	nodes[node_index].type = JSONNode::Type_Boolean;
	nodes[node_index].value.bool_v = true;
	return node_index;
}


uint32 JSONParser::parseFalse(Parser& p)
{
	if(!p.parseCString("false"))
		throw Indigo::Exception("Expected 'false'" + errorContext(p));

	const uint32 node_index = (uint32)nodes.size();
	nodes.push_back(JSONNode());
	nodes[node_index].type = JSONNode::Type_Boolean;
	nodes[node_index].value.bool_v = false;
	return node_index;
}


uint32 JSONParser::parseNull(Parser& p)
{
	if(!p.parseCString("null"))
		throw Indigo::Exception("Expected 'null'" + errorContext(p));

	const uint32 node_index = (uint32)nodes.size();
	nodes.push_back(JSONNode());
	nodes[node_index].type = JSONNode::Type_Null;
	return node_index;
}


uint32 JSONParser::parseNumber(Parser& p)
{
	const uint32 node_index = (uint32)nodes.size();
	nodes.push_back(JSONNode());
	nodes[node_index].type = JSONNode::Type_Number;

	if(!p.parseDouble(nodes[node_index].value.double_v))
		throw Indigo::Exception("Failed parsing number." + errorContext(p));
	
	return node_index;
}


uint32 JSONParser::parseArray(Parser& p)
{
	uint32 node_index = (uint32)nodes.size();
	nodes.push_back(JSONNode());
	nodes[node_index].type = JSONNode::Type_Array;

	p.consume('[');
	p.parseWhiteSpace();

	while(!p.currentIsChar(']'))
	{
		// Parse name string
		nodes[node_index].child_indices.push_back(parseNode(p));

		p.parseWhiteSpace();

		if(p.currentIsChar(','))
		{
			p.advance();
			p.parseWhiteSpace();
		}
		else
			break;
	}

	if(!p.parseChar(']'))
		throw Indigo::Exception("Expected }" + errorContext(p));

	return node_index;
}


uint32 JSONParser::parseObject(Parser& p)
{
	uint32 node_index = (uint32)nodes.size();
	nodes.push_back(JSONNode());
	nodes[node_index].type = JSONNode::Type_Object;

	p.consume('{');
	p.parseWhiteSpace();

	while(!p.currentIsChar('}'))
	{
		// Parse name string
		nodes[node_index].name_val_pairs.push_back(JSONNameValuePair());
		nodes[node_index].name_val_pairs.back().name = parseString(p);

		p.parseWhiteSpace();
		if(!p.parseChar(':'))
			throw Indigo::Exception("Expected :" + errorContext(p));
		p.parseWhiteSpace();

		nodes[node_index].name_val_pairs.back().value_node_index = parseNode(p);

		p.parseWhiteSpace();
		if(p.currentIsChar(','))
		{
			p.advance();
			p.parseWhiteSpace();
		}
		else
			break;
	}

	if(!p.parseChar('}'))
		throw Indigo::Exception("Expected }" + errorContext(p));

	return node_index;
}


void JSONParser::parseBuffer(const char* data, size_t size)
{
	Parser parser(data, (unsigned int)size);

	parseNode(parser);
}


void JSONParser::parseFile(const std::string& path)
{
	MemMappedFile file(path);

	parseBuffer((const char*)file.fileData(), file.fileSize());
}


std::string JSONParser::errorContext(Parser& p)
{
	const std::string buf(p.getText(), p.getText() + p.getTextSize());

	size_t line_num, col;
	StringUtils::getPosition(buf, p.currentPos(), line_num, col);

	const std::string line = StringUtils::getLineFromBuffer(buf, p.currentPos());

	std::string res = "\nline " + toString(line_num + 1) + ", col " + toString(col) + "\n";
	res += line;
	res += "\n";
	for(size_t z=0; z<col; ++z)
		res.push_back('-');
	res.push_back('^');
	return res;
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"


void JSONParser::test()
{
	try
	{
		JSONParser p;
		p.parseFile(TestUtils::getIndigoTestReposDir() + "/testfiles/json/example.json");

		JSONNode root_ob = p.nodes[0];
		testAssert(root_ob.type == JSONNode::Type_Object);
		testAssert(root_ob.name_val_pairs.size() == 8);

		testAssert(root_ob.name_val_pairs[0].name == "firstName");
		testAssert(p.nodes[root_ob.name_val_pairs[0].value_node_index].type == JSONNode::Type_String);
		testAssert(p.nodes[root_ob.name_val_pairs[0].value_node_index].string_v == "John");

		testAssert(root_ob.name_val_pairs[1].name == "lastName");
		testAssert(p.nodes[root_ob.name_val_pairs[1].value_node_index].type == JSONNode::Type_String);
		testAssert(p.nodes[root_ob.name_val_pairs[1].value_node_index].string_v == "Smith");

		testAssert(root_ob.name_val_pairs[2].name == "isAlive");
		testAssert(p.nodes[root_ob.name_val_pairs[2].value_node_index].type == JSONNode::Type_Boolean);
		testAssert(p.nodes[root_ob.name_val_pairs[2].value_node_index].value.bool_v == true);

		testAssert(root_ob.name_val_pairs[3].name == "age");
		testAssert(p.nodes[root_ob.name_val_pairs[3].value_node_index].type == JSONNode::Type_Number);
		testAssert(p.nodes[root_ob.name_val_pairs[3].value_node_index].value.double_v == 27.0);



		testAssert(root_ob.name_val_pairs[7].name == "spouse");
		testAssert(p.nodes[root_ob.name_val_pairs[7].value_node_index].type == JSONNode::Type_Null);
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}


	//--------- Test empty array ----------
	try
	{
		JSONParser p;
		std::string s = "{ \"a\": [] }";
		p.parseBuffer(s.data(), s.size());

		JSONNode root_ob = p.nodes[0];
		testAssert(root_ob.type == JSONNode::Type_Object);
		testAssert(root_ob.name_val_pairs.size() == 1);

		testAssert(root_ob.name_val_pairs[0].name == "a");
		testAssert(p.nodes[root_ob.name_val_pairs[0].value_node_index].type == JSONNode::Type_Array);
		testAssert(p.nodes[root_ob.name_val_pairs[0].value_node_index].child_indices.size() == 0);
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}

	//--------- Test empty object ----------
	try
	{
		JSONParser p;
		std::string s = "{ \"a\": {} }";
		p.parseBuffer(s.data(), s.size());

		JSONNode root_ob = p.nodes[0];
		testAssert(root_ob.type == JSONNode::Type_Object);
		testAssert(root_ob.name_val_pairs.size() == 1);

		testAssert(root_ob.name_val_pairs[0].name == "a");
		testAssert(p.nodes[root_ob.name_val_pairs[0].value_node_index].type == JSONNode::Type_Object);
		testAssert(p.nodes[root_ob.name_val_pairs[0].value_node_index].name_val_pairs.empty());
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}

	//--------- Test empty string ----------
	try
	{
		JSONParser p;
		std::string s = "{ \"a\": \"\" }";
		p.parseBuffer(s.data(), s.size());

		JSONNode root_ob = p.nodes[0];
		testAssert(root_ob.type == JSONNode::Type_Object);
		testAssert(root_ob.name_val_pairs.size() == 1);

		testAssert(root_ob.name_val_pairs[0].name == "a");
		testAssert(p.nodes[root_ob.name_val_pairs[0].value_node_index].type == JSONNode::Type_String);
		testAssert(p.nodes[root_ob.name_val_pairs[0].value_node_index].string_v == "");
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}

	//--------- Test some numbers ----------
	try
	{
		JSONParser p;
		std::string s = "{ \"a\": [1.0, -1.0, 1, 2, 1e20, -1.0E+20, 15.4e-5] }";
		p.parseBuffer(s.data(), s.size());

		JSONNode root_ob = p.nodes[0];
		testAssert(root_ob.type == JSONNode::Type_Object);
		testAssert(root_ob.name_val_pairs.size() == 1);

		testAssert(root_ob.name_val_pairs[0].name == "a");

		JSONNode& array_node = p.nodes[root_ob.name_val_pairs[0].value_node_index];
		testAssert(array_node.type == JSONNode::Type_Array);
		testAssert(array_node.child_indices.size() == 7);

		testAssert(p.nodes[array_node.child_indices[0]].type == JSONNode::Type_Number);
		testAssert(p.nodes[array_node.child_indices[0]].value.double_v == 1.0);

		testAssert(p.nodes[array_node.child_indices[1]].type == JSONNode::Type_Number);
		testAssert(p.nodes[array_node.child_indices[1]].value.double_v == -1.0);

		testAssert(p.nodes[array_node.child_indices[2]].type == JSONNode::Type_Number);
		testAssert(p.nodes[array_node.child_indices[2]].value.double_v == 1.0);

		testAssert(p.nodes[array_node.child_indices[3]].type == JSONNode::Type_Number);
		testAssert(p.nodes[array_node.child_indices[3]].value.double_v == 2.0);

		testAssert(p.nodes[array_node.child_indices[4]].type == JSONNode::Type_Number);
		testAssert(p.nodes[array_node.child_indices[4]].value.double_v == 1.0e20);

		testAssert(p.nodes[array_node.child_indices[5]].type == JSONNode::Type_Number);
		testAssert(p.nodes[array_node.child_indices[5]].value.double_v == -1.0e20);
		
		testAssert(p.nodes[array_node.child_indices[6]].type == JSONNode::Type_Number);
		testAssert(p.nodes[array_node.child_indices[6]].value.double_v == 15.4e-5);

	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS
