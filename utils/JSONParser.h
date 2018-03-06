/*=====================================================================
JSONParser.h
-------------------
Copyright Glare Technologies Limited 2018 -
Generated at 2018-03-05 01:34:58 +1300
=====================================================================*/
#pragma once


#include "Platform.h"
#include <vector>
#include <string>
class Parser;


struct JSONNameValuePair
{
	//string_view name;
	std::string name;
	uint32 value_node_index;
};


struct JSONNode
{
	enum Type
	{
		Type_Number,
		Type_String,
		Type_Boolean,
		Type_Array,
		Type_Object,
		Type_Null
	};
	Type type;

	union Value
	{
		double double_v; // For Type_Number
		bool bool_v; // For Type_Boolean
		//string_view string_v;
	};
	Value value;

	std::string string_v; // For Type_String

	std::vector<uint32> child_indices; // For Type_Array

	std::vector<JSONNameValuePair> name_val_pairs; // For Type_Object
};


/*=====================================================================
JSONParser
-------------------

=====================================================================*/
class JSONParser
{
public:
	JSONParser();
	~JSONParser();

	void parseFile(const std::string& path);
	void parseBuffer(const char* data, size_t size);

	static void test();

	std::vector<JSONNode> nodes;
private:
	std::string parseString(Parser& p);
	uint32 parseNode(Parser& p);
	uint32 parseObject(Parser& p);
	uint32 parseArray(Parser& p);
	uint32 parseStringNode(Parser& p);
	uint32 parseTrue(Parser& p);
	uint32 parseFalse(Parser& p);
	uint32 parseNull(Parser& p);
	uint32 parseNumber(Parser& p);
	std::string errorContext(Parser& p);
};
