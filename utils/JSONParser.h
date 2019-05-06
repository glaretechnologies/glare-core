/*=====================================================================
JSONParser.h
-------------------
Copyright Glare Technologies Limited 2018 -
Generated at 2018-03-05 01:34:58 +1300
=====================================================================*/
#pragma once


#include "Platform.h"
#include "string_view.h"
#include <vector>
#include <string>
class Parser;
class JSONParser;


struct JSONNameValuePair
{
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

	// Returns ref to string_v if this node has type Type_String, throws Indigo::Exception otherwise.
	const std::string& getStringValue() const;
	
	size_t getUIntValue() const;
	double getDoubleValue() const;

	// For objects:
	bool hasChild(const JSONParser& parser, const string_view& name) const;

	size_t getChildUIntValue(const JSONParser& parser, const string_view& name) const;
	size_t getChildUIntValueWithDefaultVal(const JSONParser& parser, const string_view& name, size_t default_val) const;

	double getChildDoubleValueWithDefaultVal(const JSONParser& parser, const string_view& name, double default_val) const;

	const std::string& getChildStringValue(const JSONParser& parser, const string_view& name) const;
	const std::string getChildStringValueWithDefaultVal(const JSONParser& parser, const string_view& name, const string_view& default_val) const;
	const JSONNode& getChildObject(const JSONParser& parser, const string_view& name) const;
	const JSONNode& getChildArray(const JSONParser& parser, const string_view& name) const;

	static const std::string typeString(const Type type);

	union Value
	{
		double double_v; // For Type_Number
		bool bool_v; // For Type_Boolean
	};
	Value value;

	std::string string_v; // For Type_String

	std::vector<uint32> child_indices; // For Type_Array

	std::vector<JSONNameValuePair> name_val_pairs; // For Type_Object
};


/*=====================================================================
JSONParser
----------
Not super-optimised, mostly due to all the memory allocations made for the nodes
and the strings they contain etc..
Parses at about 70 MB/s on my i7-8700K CPU.
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
