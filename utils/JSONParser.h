/*=====================================================================
JSONParser.h
------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "RuntimeCheck.h"
#include "string_view.h"
#include <vector>
#include <string>
#include <cassert>
class Parser;
class JSONParser;
struct JSONNode;
struct JSONNodeInternal;


struct JSONArrayNodeChildIterator
{
	inline JSONArrayNodeChildIterator(const JSONNode* node_, uint32 child_index_);

	inline const JSONNode getNode() const;

	bool operator == (const JSONArrayNodeChildIterator& other) const { return cur_child_index == other.cur_child_index; }
	bool operator != (const JSONArrayNodeChildIterator& other) const { return cur_child_index != other.cur_child_index; }

	void operator ++ () { cur_child_index++; }
private:
	uint32 array_node_index;
	uint32 cur_child_index;
	const JSONParser* parser;
};


struct JSONObjectNodeChildIterator
{
	inline JSONObjectNodeChildIterator(const JSONNode* node_, uint32 child_index_);

	inline string_view name() const;
	inline const JSONNode valueNode() const;

	bool operator == (const JSONObjectNodeChildIterator& other) const { return cur_child_index == other.cur_child_index; }
	bool operator != (const JSONObjectNodeChildIterator& other) const { return cur_child_index != other.cur_child_index; }

	void operator ++ () { cur_child_index++; }
private:
	uint32 object_node_index;
	uint32 cur_child_index;
	const JSONParser* parser;
};


enum JSONNodeType
{
	Type_Number,
	Type_String,
	Type_Boolean,
	Type_Array,
	Type_Object,
	Type_Null
};


struct JSONNodeInternal
{
	JSONNodeType type;


	union Value
	{
		double double_v; // For Type_Number
		bool bool_v; // For Type_Boolean
	};
	Value value;

	static const std::string typeString(const JSONNodeType type);

	size_t getUIntValue() const;
	int getIntValue() const;
	double getDoubleValue() const;
	bool getBoolValue() const;

	// Returns ref to string_v if this node has type Type_String, throws glare::Exception otherwise.
	const string_view getStringValue(const JSONParser* parser) const;


	
	/*
	Type_String
	--------------------------

	----layout----
	char string_data[data_size]           <---- data_offset
	[padding]                            (to next multiple of sizeof(uint32))

	*/


	/*
	Type_Array
	--------------------------
	n = num_child_indices = data_size

	----layout----
	uint32 child_0_index              <---- data_offset
	uint32 child_1_index
	...
	uint32 child_n_minus_1_index
	*/
	uint32 numChildIndices() const { assert(type == JSONNodeType::Type_Array); return data_size; }
	inline uint32 arrayChildNodeIndex(const JSONParser& parser, uint32 i) const;


	/*
	Type_Object:
	--------------------------
	n = num_pairs = data_size

	----layout----
	uint32 pair_0_name_data_offset      <---- data_offset       (offset in data of the start of the name string)
	uint32 pair_0_name_size                                     (size in characters of the pair 0 name string)      
	uint32 pair_0_value_node_index                              (index in parser.nodes of the value node corresponding to the name)

	uint32 pair_1_name_data_offset
	uint32 pair_1_name_size
	uint32 pair_1_value_node_index 
	...
	uint32 pair_n_minus_1_name_data_offset
	uint32 pair_n_minus_1_name_size
	uint32 pair_n_minus_1_value_node_index

	pair_0_name_data[pair_0_name_size]                    <---- pair_0_name_data_offset
	pair_1_name_data[pair_1_name_size]                    <---- pair_1_name_data_offset
	...
	pair_n_minus_1_name_data[pair_n_minus_1_name_size]    <---- pair_n_minus_1_name_data_offset
	[padding]

	*/
	uint32 numNameValPairs() const { assert(type == JSONNodeType::Type_Object); return data_size; }
	
	inline const char* nameValPairNameData(const JSONParser& parser, uint32 name_val_pair_index) const;
	inline size_t      nameValPairNameSize(const JSONParser& parser, uint32 name_val_pair_index) const;

	string_view nameValPairName(const JSONParser& parser, uint32 name_val_pair_index) const { return string_view(
		/*chars=*/nameValPairNameData(parser, name_val_pair_index),
		/*count=*/nameValPairNameSize(parser, name_val_pair_index)); 
	}

	inline uint32 nameValPairValueNodeIndex(const JSONParser& parser, uint32 name_val_pair_index) const;

	uint32 data_offset;
	uint32 data_size; // for object or array type, == num_children. // For string type, == string data size.
};


// A user-accessible handle to a JSONNodeInternal.
struct JSONNode
{
	JSONNode(uint32 node_index_, const JSONParser* parser_) : node_index(node_index_), parser(parser_) {}

	inline JSONNodeType getType() const;

	// Returns ref to string_v if this node has type Type_String, throws glare::Exception otherwise.
	inline string_view getStringValue() const;

	// Returns the number value cast to an int if this node has Type_Number, throws glare::Exception otherwise.
	inline int getIntValue() const;

	// Returns the number value cast to an uint if this node has Type_Number, throws glare::Exception otherwise.
	inline size_t getUIntValue() const;

	// Returns the number value if this node has Type_Number, throws glare::Exception otherwise.
	inline double getDoubleValue() const;

	// Returns the bool value if this node has Type_Boolean, throws glare::Exception otherwise.
	inline bool getBoolValue() const;

	//----------------------- For Type_Object or Type_Array -----------------------
	inline size_t numChildren() const;


	//----------------------- For Type_Object -----------------------
	bool hasChild(const string_view name) const;
	const JSONNode getChildNode(const string_view name) const;
	const JSONNode getChildNodeForIndex(uint32 index) const;
	const string_view getNameI(uint32 index) const;

	inline JSONObjectNodeChildIterator objectChildrenBegin() const;
	inline JSONObjectNodeChildIterator objectChildrenEnd()   const;

	size_t getChildUIntValue(const string_view name) const;
	size_t getChildUIntValueWithDefaultVal(const string_view name, size_t default_val) const;

	int getChildIntValue(const string_view name) const;
	int getChildIntValueWithDefaultVal(const string_view name, int default_val) const;

	double getChildDoubleValue(const string_view name) const;
	double getChildDoubleValueWithDefaultVal(const string_view name, double default_val) const;

	bool getChildBoolValueWithDefaultVal(const string_view name, bool default_val) const;

	const string_view getChildStringValue(const string_view name) const;

	// Note that the returned string_view may be the default_val passed in as an argument.  Make sure the used lifetime of the result is not longer than the argument scope!
	const string_view getChildStringValueWithDefaultVal(const string_view name, const string_view default_val) const;

	bool hasChildWithStringValueEqualTo(const string_view name, const string_view target_value) const;

	// Returns true if this object has a child 'name' with a string value equal to target_value, ignoring case.  target_value must be lowercase.
	// Returns false if the child is missing or is not a string.
	bool hasChildWithStringValueCaseInsensitiveEqualTo(const string_view name, const string_view target_value) const;

	const JSONNode getChildObject(const string_view name) const;
	const JSONNode getChildArray(const string_view name) const;


	//----------------------- For Type_Array -----------------------
	inline JSONNode getArrayChildNode(size_t index) const;

	inline uint32 numNameValPairs() const;

	inline JSONArrayNodeChildIterator arrayChildrenBegin() const;
	inline JSONArrayNodeChildIterator arrayChildrenEnd() const;

	// Parse the values from an array node like [1.0, 2.0, 3.0]
	void parseDoubleArrayValues(size_t expected_num_elems, double* values_out) const;


//private:
	uint32 node_index;
	const JSONParser* parser;
};

void checkNodeType(const JSONNode& node, JSONNodeType type); // Throws exception if node does not have type 'type'.



/*=====================================================================
JSONParser
----------
=====================================================================*/
class JSONParser
{
public:
	JSONParser();
	~JSONParser();

	GLARE_DISABLE_COPY(JSONParser)

	friend struct JSONNodeInternal;
	friend struct JSONNode;
	friend struct JSONArrayNodeChildIterator;
	friend struct JSONObjectNodeChildIterator;

	void parseFile(const std::string& path);         // Resulting number of nodes will be > 0 if did not throw.
	void parseBuffer(const char* data, size_t size); // Resulting number of nodes will be > 0 if did not throw.

	static void test();

	JSONNode rootNode() const { return JSONNode(0, this); }

private:
	void parseString(Parser& p);
	uint32 parseNode(Parser& p, int depth);
	uint32 parseObject(Parser& p, int depth);
	uint32 parseArray(Parser& p, int depth);
	uint32 parseStringNode(Parser& p);
	uint32 parseTrue(Parser& p);
	uint32 parseFalse(Parser& p);
	uint32 parseNull(Parser& p);
	uint32 parseNumber(Parser& p);
	std::string errorContext(Parser& p);	

	std::vector<JSONNodeInternal> nodes;
	std::vector<uint8> data;

	std::vector<uint32> working_uint32_stack;
	std::vector<char> working_char_stack;
};




uint32 JSONNodeInternal::arrayChildNodeIndex(const JSONParser& parser, uint32 i) const
{ 
	runtimeCheck(type == Type_Array);
	return *(uint32*)&parser.data[data_offset + sizeof(uint32) * i];
}

size_t JSONNodeInternal::nameValPairNameSize(const JSONParser& parser, uint32 name_val_pair_index) const
{ 
	runtimeCheck(type == Type_Object); 
	return (size_t)*(uint32*)&parser.data[data_offset + sizeof(uint32) * ((name_val_pair_index * 3) + 1)];
}

const char* JSONNodeInternal::nameValPairNameData(const JSONParser& parser, uint32 name_val_pair_index) const
{ 
	runtimeCheck(type == Type_Object);
	const uint32 pair_i_name_data_offset = *(uint32*)&parser.data[data_offset + sizeof(uint32) * (name_val_pair_index * 3)];

	return (const char*)(parser.data.data() + pair_i_name_data_offset);
}

uint32 JSONNodeInternal::nameValPairValueNodeIndex(const JSONParser& parser, uint32 name_val_pair_index) const
{ 
	runtimeCheck(type == Type_Object);
	return *(uint32*)&parser.data[data_offset + sizeof(uint32) * ((name_val_pair_index * 3) + 2)];
}



JSONNodeType JSONNode::getType() const { return parser->nodes[node_index].type; }

size_t JSONNode::numChildren() const { runtimeCheck(getType() == Type_Array || getType() == Type_Object); return parser->nodes[node_index].data_size; }

JSONNode JSONNode::getArrayChildNode(size_t index) const { runtimeCheck(getType() == Type_Array); return JSONNode(parser->nodes[node_index].arrayChildNodeIndex(*parser, (uint32)index), parser); }

uint32 JSONNode::numNameValPairs() const { return parser->nodes[node_index].numNameValPairs(); }

string_view JSONNode::getStringValue() const
{
	return parser->nodes[this->node_index].getStringValue(parser);
}

int JSONNode::getIntValue() const
{
	return parser->nodes[this->node_index].getIntValue();
}

size_t JSONNode::getUIntValue() const
{
	return parser->nodes[this->node_index].getUIntValue();
}

double JSONNode::getDoubleValue() const
{
	return parser->nodes[this->node_index].getDoubleValue();
}

bool JSONNode::getBoolValue() const
{
	return parser->nodes[this->node_index].getBoolValue();
}

JSONArrayNodeChildIterator JSONNode::arrayChildrenBegin() const { runtimeCheck(getType() == JSONNodeType::Type_Array); return JSONArrayNodeChildIterator(this, /*index=*/0); }
JSONArrayNodeChildIterator JSONNode::arrayChildrenEnd() const   { runtimeCheck(getType() == JSONNodeType::Type_Array); return JSONArrayNodeChildIterator(this, /*index=*/parser->nodes[node_index].numChildIndices()); }

JSONObjectNodeChildIterator JSONNode::objectChildrenBegin() const { runtimeCheck(getType() == JSONNodeType::Type_Object); return JSONObjectNodeChildIterator(this, /*index=*/0); }
JSONObjectNodeChildIterator JSONNode::objectChildrenEnd()   const { runtimeCheck(getType() == JSONNodeType::Type_Object); return JSONObjectNodeChildIterator(this, /*index=*/parser->nodes[node_index].numNameValPairs()); }





JSONArrayNodeChildIterator::JSONArrayNodeChildIterator(const JSONNode* node_, uint32 child_index_)
:	array_node_index(node_->node_index), cur_child_index(child_index_), parser(node_->parser) {}

const JSONNode JSONArrayNodeChildIterator::getNode() const { return JSONNode(parser->nodes[array_node_index].arrayChildNodeIndex(*parser, cur_child_index), parser); }





JSONObjectNodeChildIterator::JSONObjectNodeChildIterator(const JSONNode* node_, uint32 child_index_)
:	object_node_index(node_->node_index), cur_child_index(child_index_), parser(node_->parser) {}


string_view JSONObjectNodeChildIterator::name() const
{
	const JSONNodeInternal& object_node = parser->nodes[this->object_node_index];

	return object_node.nameValPairName(*parser, cur_child_index);
}


const JSONNode JSONObjectNodeChildIterator::valueNode() const
{
	const JSONNodeInternal& object_node = parser->nodes[this->object_node_index];

	return JSONNode(object_node.nameValPairValueNodeIndex(*parser, cur_child_index), parser);
}
