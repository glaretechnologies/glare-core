/*=====================================================================
JSONParser.cpp
--------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "JSONParser.h"


#include "MemMappedFile.h"
#include "Parser.h"
#include "Exception.h"
#include "UTF8Utils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "../maths/mathstypes.h"
#include <limits>


const std::string JSONNodeInternal::typeString(const JSONNodeType type)
{
	switch(type)
	{
	case Type_Number:
		return "Number";
	case Type_String:
		return "String";
	case Type_Boolean:
		return "Boolean";
	case Type_Array:
		return "Array";
	case Type_Object:
		return "Object";
	case Type_Null:
		return "Null";
	}
	return "";
}


size_t JSONNodeInternal::getUIntValue() const
{
	if(type != JSONNodeType::Type_Number)
		throw glare::Exception("Expected type Number.");

	const double v = value.double_v;
	if(!isFinite(v) || !(v >= 0.0 && v < (double)std::numeric_limits<size_t>::max() + 1.0)) // Reject negative values
		throw glare::Exception("Number value out of range for unsigned integer.");

	return (size_t)v;
}


// Converting a non-finite or out-of-range double to an integer type is undefined behaviour, so check the value is convertible first.
// Values are truncated towards zero.
int JSONNodeInternal::getIntValue() const
{
	if(type != JSONNodeType::Type_Number)
		throw glare::Exception("Expected type Number.");

	const double v = value.double_v;
	if(!isFinite(v) || !(v > (double)std::numeric_limits<int>::min() - 1.0 && v < (double)std::numeric_limits<int>::max() + 1.0))
		throw glare::Exception("Number value out of range for int.");

	return (int)v;
}


double JSONNodeInternal::getDoubleValue() const
{
	if(type == JSONNodeType::Type_Number)
		return value.double_v;
	else
		throw glare::Exception("Expected type Number.");
}


bool JSONNodeInternal::getBoolValue() const
{
	if(type == JSONNodeType::Type_Boolean)
		return value.bool_v;
	else
		throw glare::Exception("Expected type Boolean.");
}


const string_view JSONNodeInternal::getStringValue(const JSONParser& parser) const
{
	if(type == JSONNodeType::Type_String)
		return string_view((const char*)(parser.data.data() + data_offset), data_size);
	else
		throw glare::Exception("Expected type String - type was " + JSONNodeInternal::typeString(type));
}


bool JSONNode::hasChild(const string_view name) const
{
	JSONNode child_node;
	return tryGetChildNodeWithName(name, /*child node out=*/child_node);
}


const JSONNode JSONNode::getChildNode(const string_view name) const
{
	JSONNode child_node;
	if(tryGetChildNodeWithName(name, /*child node out=*/child_node))
		return child_node;
	else
		throw glare::Exception("Failed to find child name/value pair with name '" + toString(name) + "'.");
}


bool JSONNode::tryGetChildNodeWithName(const string_view name, JSONNode& child_node_out) const
{
	const JSONNodeInternal& node = parser->nodes[this->node_index];

	if(node.type != JSONNodeType::Type_Object)
		throw glare::Exception("Expected type object.");

	for(uint32 i=0; i<node.numNameValPairs(); ++i)
		if(node.nameValPairName(*parser, i) == name)
		{
			child_node_out = JSONNode(node.nameValPairValueNodeIndex(*parser, i), parser);
			return true;
		}

	return false;
}


const JSONNode JSONNode::getChildNodeForIndex(uint32 index) const
{
	const JSONNodeInternal& node = parser->nodes[this->node_index];

	if(node.type != JSONNodeType::Type_Object)
		throw glare::Exception("Expected type object.");

	return JSONNode(node.nameValPairValueNodeIndex(*parser, index), parser);
}


const string_view JSONNode::getNameI(uint32 index) const
{
	const JSONNodeInternal& node = parser->nodes[this->node_index];

	if(node.type != JSONNodeType::Type_Object)
		throw glare::Exception("Expected type object.");

	return node.nameValPairName(*parser, index);
}


size_t JSONNode::getChildUIntValue(const string_view name) const
{
	return getChildNode(name).getUIntValue();
}


size_t JSONNode::getChildUIntValueWithDefaultVal(const string_view name, size_t default_val) const
{
	JSONNode child_node;
	if(tryGetChildNodeWithName(name, /*child node out=*/child_node))
		return child_node.getUIntValue();
	else
		return default_val;
}


int JSONNode::getChildIntValue(const string_view name) const
{
	return getChildNode(name).getIntValue();
}


int JSONNode::getChildIntValueWithDefaultVal(const string_view name, int default_val) const
{
	JSONNode child_node;
	if(tryGetChildNodeWithName(name, /*child node out=*/child_node))
		return child_node.getIntValue();
	else
		return default_val;
}


double JSONNode::getChildDoubleValue(const string_view name) const
{
	return getChildNode(name).getDoubleValue();
}


double JSONNode::getChildDoubleValueWithDefaultVal(const string_view name, double default_val) const
{
	JSONNode child_node;
	if(tryGetChildNodeWithName(name, /*child node out=*/child_node))
		return child_node.getDoubleValue();
	else
		return default_val;
}


bool JSONNode::getChildBoolValueWithDefaultVal(const string_view name, bool default_val) const
{
	JSONNode child_node;
	if(tryGetChildNodeWithName(name, /*child node out=*/child_node))
		return child_node.getBoolValue();
	else
		return default_val;
}


const string_view JSONNode::getChildStringValue(const string_view name) const
{
	return getChildNode(name).getStringValue();
}


const string_view JSONNode::getChildStringValueWithDefaultVal(const string_view name, const string_view default_val) const
{
	JSONNode child_node;
	if(tryGetChildNodeWithName(name, /*child node out=*/child_node))
		return child_node.getStringValue();
	else
		return default_val;
}


bool JSONNode::hasChildWithStringValueEqualTo(const string_view name, const string_view target_value) const
{
	const JSONNodeInternal& node = parser->nodes[this->node_index];

	if(node.type != JSONNodeType::Type_Object)
		throw glare::Exception("Expected type object.");

	for(uint32 i=0; i<node.numNameValPairs(); ++i)
		if(node.nameValPairName(*parser, i) == name && parser->nodes[node.nameValPairValueNodeIndex(*parser, i)].type == JSONNodeType::Type_String)
			return parser->nodes[node.nameValPairValueNodeIndex(*parser, i)].getStringValue(*parser) == target_value;

	return false;
}


bool JSONNode::hasChildWithStringValueCaseInsensitiveEqualTo(const string_view name, const string_view target_value) const
{
	assert(::toLowerCase(target_value) == target_value); // target_value should be lower case.

	const JSONNodeInternal& node = parser->nodes[this->node_index];

	if(node.type != JSONNodeType::Type_Object)
		throw glare::Exception("Expected type object.");

	for(uint32 i=0; i<node.numNameValPairs(); ++i)
		if(node.nameValPairName(*parser, i) == name && parser->nodes[node.nameValPairValueNodeIndex(*parser, i)].type == JSONNodeType::Type_String)
			return StringUtils::equalCaseInsensitive(parser->nodes[node.nameValPairValueNodeIndex(*parser, i)].getStringValue(*parser), target_value);

	return false;
}


const JSONNode JSONNode::getChildObject(const string_view name) const
{
	JSONNode child_node = getChildNode(name);
	if(child_node.getType() != JSONNodeType::Type_Object)
		throw glare::Exception("Expected child with name '" + toString(name) + "' to have type object, had type " + JSONNodeInternal::typeString(child_node.getType()));

	return child_node;
}


const JSONNode JSONNode::getChildArray(const string_view name) const
{
	JSONNode child_node = getChildNode(name);
	if(child_node.getType() != JSONNodeType::Type_Array)
		throw glare::Exception("Expected child with name '" + toString(name) + "' to have type Array, had type " + JSONNodeInternal::typeString(child_node.getType()));

	return child_node;
}


// Parse the values from an array node like [1.0, 2.0, 3.0]
void JSONNode::parseDoubleArrayValues(size_t expected_num_elems, double* values_out) const
{
	const JSONNodeInternal& node = parser->nodes[this->node_index];

	if(node.type != JSONNodeType::Type_Array)
		throw glare::Exception("Expected type array.");

	if(node.numChildIndices() != expected_num_elems)
		throw glare::Exception("Array had wrong size.");

	for(uint32 z=0; z<node.numChildIndices(); ++z)
	{
		const JSONNodeInternal& child_node = parser->nodes[node.arrayChildNodeIndex(*parser, z)];
		if(child_node.type != JSONNodeType::Type_Number)
			throw glare::Exception("JSONNode::parseDoubleArrayValues(): Expected child to have type Number, actual type was " + JSONNodeInternal::typeString(child_node.type));

		values_out[z] = child_node.value.double_v;
	}
}


void checkNodeType(const JSONNode& node, JSONNodeType type)
{
	if(node.getType() != type)
		throw glare::Exception("Expected type " + JSONNodeInternal::typeString(type) + ", got type " + JSONNodeInternal::typeString(node.getType()) + ".");
}


JSONParser::JSONParser()
{}


JSONParser::~JSONParser()
{}


// Writes to the top of working_char_stack.
void JSONParser::parseString(Parser& p)
{
	std::vector<char>& s = this->working_char_stack;

	// Parse opening "
	if(!p.parseChar('"'))
		throw glare::Exception("Expected \"" + errorContext(p));
	
	while(p.notEOF() && p.current() != '"')
	{
		if(p.current() == '\\') // Parse escape sequence:
		{
			p.consume('\\');
			if(p.eof())
				throw glare::Exception("EOF in escape sequence." + errorContext(p));
			switch(p.current())
			{
			case '"':
				s.push_back('"');
				p.advance();
				break;
			case '\\':
				s.push_back('\\');
				p.advance();
				break;
			case '/':
				s.push_back('/');
				p.advance();
				break;
			case 'b':
				s.push_back('\b');
				p.advance();
				break;
			case 'f':
				s.push_back('\f');
				p.advance();
				break;
			case 'n':
				s.push_back('\n');
				p.advance();
				break;
			case 'r':
				s.push_back('\r');
				p.advance();
				break;
			case 't':
				s.push_back('\t');
				p.advance();
				break;
			case 'u':
			{
				p.consume('u');
				if(p.currentPos() + 4 > p.getTextSize())
					throw glare::Exception("EOF while parsing unicode code point.." + errorContext(p));

				// Parse 4-hex-digit unicode code point
				uint32 code_point = 0;
				int bit_offset = 12;
				for(int i=0; i<4; ++i)
				{
					const char c = p.current();
					try
					{
						code_point |= hexCharToUInt(c) << bit_offset;
					}
					catch(StringUtilsExcep& e)
					{
						throw glare::Exception("Error while parsing unicode code point: " + e.what() + errorContext(p));
					}
					p.advance();
					bit_offset -= 4;
				}

				const std::string code_point_str = UTF8Utils::encodeCodePoint(code_point); // Encode code point as UTF-8 and append
				for(size_t z=0; z<code_point_str.size(); ++z)
					s.push_back(code_point_str[z]);
				break;
			}
			default:
				throw glare::Exception("Invalid escape sequence." + errorContext(p));
			}
		}
		else
		{
			s.push_back(p.current());
			p.advance();
		}
	}

	// Parse closing "
	if(!p.parseChar('"'))
		throw glare::Exception("Expected \"" + errorContext(p));
}


uint32 JSONParser::parseNode(Parser& parser, int depth)
{
	if(parser.eof())
		throw glare::Exception("Unexpected end of file while parsing JSON node." + errorContext(parser));

	if(depth > 256)
		throw glare::Exception("Parse depth is too large. (> 256)");

	switch(parser.current())
	{
	case '{':
		return parseObject(parser, /*depth=*/depth);
	case '[':
		return parseArray(parser, /*depth=*/depth);
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
			throw glare::Exception("Unexpected character '" + std::string(1, parser.current()) + "'" + errorContext(parser));
	}
}


// Since node indices are uint32s, we need to make sure we don't allocate too many nodes such that indices overflow.
static void pushBackNewNode(std::vector<JSONNodeInternal>& nodes)
{
	const size_t MAX_NUM_NODES = 1'000'000'000;
	if(nodes.size() >= MAX_NUM_NODES)
		throw glare::Exception("Too many nodes");
	nodes.push_back(JSONNodeInternal());
}


static const size_t MAX_DATA_SIZE = 2'000'000'000;

// Indices into data are likewise 32 bits, so we need to make sure we don't allocate too much data such that indices overflow.
static void resizeData(std::vector<uint8>& data, size_t new_size)
{
	if(new_size > MAX_DATA_SIZE)
		throw glare::Exception("Max data size exceeded");
	data.resize(new_size);
}


uint32 JSONParser::parseStringNode(Parser& p)
{
	const uint32 node_index = (uint32)nodes.size();
	pushBackNewNode(nodes);
	nodes[node_index].type = JSONNodeType::Type_String;
	
	// Parse the string (placing on the top of working_char_stack)
	const size_t initial_working_char_stack_size = working_char_stack.size();
	parseString(p);
	const size_t str_size = working_char_stack.size() - initial_working_char_stack_size;

	const uint32 initial_data_size = (uint32)data.size();
	nodes[node_index].data_offset = initial_data_size;
	nodes[node_index].data_size   = (uint32)str_size;

	// resize data to make room for string, while zero-padding data.size() to a multiple of sizeof(uint32) bytes.
	if(str_size > MAX_DATA_SIZE) // Avoid overflow
		throw glare::Exception("String too long");
	resizeData(data, /*new size=*/Maths::roundUpToMultipleOfPowerOf2(data.size() + str_size, sizeof(uint32)));
	if(str_size > 0)
		std::memcpy(&data[initial_data_size], &working_char_stack[initial_working_char_stack_size], str_size);

	// Restore working_char_stack
	working_char_stack.resize(initial_working_char_stack_size);

	return node_index;
}


uint32 JSONParser::parseTrue(Parser& p)
{
	if(!p.parseString("true"))
		throw glare::Exception("Expected 'true'" + errorContext(p));

	const uint32 node_index = (uint32)nodes.size();
	pushBackNewNode(nodes);
	nodes[node_index].type = JSONNodeType::Type_Boolean;
	nodes[node_index].value.bool_v = true;
	return node_index;
}


uint32 JSONParser::parseFalse(Parser& p)
{
	if(!p.parseString("false"))
		throw glare::Exception("Expected 'false'" + errorContext(p));

	const uint32 node_index = (uint32)nodes.size();
	pushBackNewNode(nodes);
	nodes[node_index].type = JSONNodeType::Type_Boolean;
	nodes[node_index].value.bool_v = false;
	return node_index;
}


uint32 JSONParser::parseNull(Parser& p)
{
	if(!p.parseString("null"))
		throw glare::Exception("Expected 'null'" + errorContext(p));

	const uint32 node_index = (uint32)nodes.size();
	pushBackNewNode(nodes);
	nodes[node_index].type = JSONNodeType::Type_Null;
	return node_index;
}


uint32 JSONParser::parseNumber(Parser& p)
{
	const uint32 node_index = (uint32)nodes.size();
	pushBackNewNode(nodes);
	nodes[node_index].type = JSONNodeType::Type_Number;

	if(!p.parseDouble(nodes[node_index].value.double_v))
		throw glare::Exception("Failed parsing number." + errorContext(p));
	
	return node_index;
}


uint32 JSONParser::parseArray(Parser& p, int depth)
{
	const uint32 node_index = (uint32)nodes.size();
	pushBackNewNode(nodes);
	nodes[node_index].type = JSONNodeType::Type_Array;

	p.consume('[');
	p.parseWhiteSpace();

	const size_t working_stack_initial_size = working_uint32_stack.size();

	while(!p.currentIsChar(']'))
	{
		// Parse name string
		const uint32 child_index = parseNode(p, /*depth=*/depth + 1);
		working_uint32_stack.push_back(child_index);

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
		throw glare::Exception("Expected ]" + errorContext(p));

	const size_t num_children = working_uint32_stack.size() - working_stack_initial_size;

	if(num_children > 500'000'000) // Avoid overflow when casting down to uint32 and when computing sizeof(uint32) * num_children below.
		throw glare::Exception("Too many children");

	// Now write the child indices data the node to the data array
	nodes[node_index].data_offset = (uint32)data.size();
	nodes[node_index].data_size = (uint32)num_children;

	if(num_children > 0)
	{
		// Allocate space
		// Since num_children <= 500,000,000 from above, (sizeof(uint32) * num_children) <= 2B, and data.size() <= 2B, so we can't overflow size_t on 32 bit here.
		resizeData(data, /*new size=*/data.size() + sizeof(uint32) * num_children);

		// Copy the data
		std::memcpy(&data[nodes[node_index].data_offset], &working_uint32_stack[working_stack_initial_size], sizeof(uint32) * num_children);
	}

	// Restore working_stack
	working_uint32_stack.resize(working_stack_initial_size);

	return node_index;
}


uint32 JSONParser::parseObject(Parser& p, int depth)
{
	const uint32 node_index = (uint32)nodes.size();
	pushBackNewNode(nodes);
	nodes[node_index].type = JSONNodeType::Type_Object;

	p.consume('{');
	p.parseWhiteSpace();

	const size_t initial_working_uint32_stack_size = working_uint32_stack.size();
	const size_t initial_working_char_stack_size   = working_char_stack.size();

	// We will store some temp data on the working_uint32_stack in the following layout:
	//
	// pair_0_name_size                 <---- initial_working_uint32_stack_size
	// pair_0_value_node_index
	// pair_1_name_size
	// pair_1_value_node_index
	// ...
	// pair_n_minus_1_name_size
	// pair_n_minus_1_value_node_index
	//
	//
	// and we will store temp name data on the working_char_stack contiguously.

	while(!p.currentIsChar('}'))
	{
		// Parse name string
		const size_t char_stack_size = working_char_stack.size();
		parseString(p);
		const size_t name_string_size = working_char_stack.size() - char_stack_size;
		if(name_string_size > 1'000'000'000) // Avoid overflow of uint32
			throw glare::Exception("String too long");
		working_uint32_stack.push_back((uint32)name_string_size);

		p.parseWhiteSpace();
		if(!p.parseChar(':'))
			throw glare::Exception("Expected :" + errorContext(p));
		p.parseWhiteSpace();

		const uint32 child_index = parseNode(p, /*depth=*/depth + 1);
		working_uint32_stack.push_back(child_index);

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
		throw glare::Exception("Expected }" + errorContext(p));

	const size_t total_name_data_size = working_char_stack.size() - initial_working_char_stack_size;

	const size_t num_children = (working_uint32_stack.size() - initial_working_uint32_stack_size) / 2;
	if(num_children > 100'000'000) // Avoid overflow when casting down to uint32, and in the sizeof(uint32) * 3 * num_children mul below.
		throw glare::Exception("Too many children");

	// Now write the child data for the node to the data array
	const size_t start_data_offset = data.size();
	nodes[node_index].data_offset = (uint32)data.size();
	nodes[node_index].data_size = (uint32)num_children;

	// First, compute total data size used by this node, including the name string data.
	const size_t fixed_size_part_data_size = sizeof(uint32) * 3 * num_children; 

	const uint64 data_used_before_padding = (uint64)fixed_size_part_data_size + (uint64)total_name_data_size;
	const uint64 data_used = Maths::roundUpToMultipleOfPowerOf2<uint64>(data_used_before_padding, sizeof(uint32));
	if(data_used > MAX_DATA_SIZE)
		throw glare::Exception("too much data");

	// Now allocate the data space
	resizeData(data, /*new size=*/data.size() + data_used);

	// Copy the data
	uint32 cur_name_data_offset = (uint32)(start_data_offset + fixed_size_part_data_size);
	size_t name_data_size_partial_sum = 0;
	for(size_t i=0; i<num_children; ++i)
	{
		// Write pair_i_name_data_offset
		std::memcpy(&data[start_data_offset + sizeof(uint32)*(3*i + 0)], &cur_name_data_offset, sizeof(uint32));

		// Write pair_i_name_size
		const uint32 pair_i_name_size = working_uint32_stack[initial_working_uint32_stack_size + i*2];
		std::memcpy(&data[start_data_offset + sizeof(uint32)*(3*i + 1)], &pair_i_name_size, sizeof(uint32));

		// Write pair_i_value_node_index
		const uint32 pair_i_value_node_index = working_uint32_stack[initial_working_uint32_stack_size + i*2 + 1];
		std::memcpy(&data[start_data_offset + sizeof(uint32)*(3*i + 2)], &pair_i_value_node_index, sizeof(uint32));

		// Copy pair_i_name_data[pair_i_name_size]
		if(pair_i_name_size > 0)
		{
			const char* pair_i_name_data = &working_char_stack[initial_working_char_stack_size + name_data_size_partial_sum];
			std::memcpy(data.data() + cur_name_data_offset, pair_i_name_data, pair_i_name_size);
		}

		name_data_size_partial_sum += pair_i_name_size;
		cur_name_data_offset       += pair_i_name_size;
		assert(cur_name_data_offset <= start_data_offset + data_used);
	}

	// Padding should already be zero-initialised.

	// Restore working stacks
	working_uint32_stack.resize(initial_working_uint32_stack_size);
	working_char_stack.resize(initial_working_char_stack_size);

	return node_index;
}


void JSONParser::parseBuffer(const char* buffer_data, size_t size)
{
	// Reset state
	nodes.resize(0);
	data.resize(0);
	working_uint32_stack.resize(0);
	working_char_stack.resize(0);

	nodes.reserve(size / 8);
	data.reserve(size);
	working_uint32_stack.reserve(1024);
	working_char_stack.reserve(4096);

	Parser parser(buffer_data, size);

	parser.parseWhiteSpace();

	parseNode(parser, /*depth=*/0);
}


void JSONParser::parseFile(const std::string& path)
{
	MemMappedFile file(path);

	parseBuffer((const char*)file.fileData(), file.fileSize());
}


std::string JSONParser::errorContext(Parser& p)
{
	const string_view buf(p.getText(), p.getTextSize());

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


#include "../utils/TestUtils.h"
#include "../utils/TestExceptionUtils.h"
#include "../utils/FileUtils.h"
#include "Timer.h"


#if 1
// Command line:
// C:\fuzz_corpus\json -max_len=1000000 -seed=1

// Walks the whole tree through the public API.
// Value getters are called on every node, and should throw glare::Exception on a type mismatch.
// Container accessors use runtimeCheck on the node type, so are only called on nodes of the matching type.
static void fuzzTraverseNode(const JSONParser& parser, const JSONNode& node)
{
	try { node.getStringValue(); } catch(glare::Exception&) {}
	try { node.getDoubleValue(); } catch(glare::Exception&) {}
	try { node.getIntValue(); }    catch(glare::Exception&) {}
	try { node.getUIntValue(); }   catch(glare::Exception&) {}
	try { node.getBoolValue(); }   catch(glare::Exception&) {}

	if(node.getType() == JSONNodeType::Type_String)
	{
		const string_view s = node.getStringValue();
		volatile size_t sum = 0;
		for(size_t i=0; i<s.size(); ++i) // Touch every char so ASan checks the string data range.
			sum = sum + (uint8)s[i];
	}
	else if(node.getType() == JSONNodeType::Type_Array)
	{
		const size_t num_children = node.numChildren();

		size_t i = 0;
		for(auto it = node.arrayChildrenBegin(); it != node.arrayChildrenEnd(); ++it, ++i)
		{
			testAssert(it.getNode().node_index == node.getArrayChildNode(i).node_index);
			fuzzTraverseNode(parser, it.getNode());
		}
		testAssert(i == num_children);

		std::vector<double> values(num_children);
		try { node.parseDoubleArrayValues(num_children, values.data()); } catch(glare::Exception&) {}
		try { node.parseDoubleArrayValues(num_children + 1, values.data()); failTest("Expected excep."); } catch(glare::Exception&) {}
	}
	else if(node.getType() == JSONNodeType::Type_Object)
	{
		const size_t num_pairs = node.numNameValPairs();
		testAssert(node.numChildren() == num_pairs);

		size_t i = 0;
		for(auto it = node.objectChildrenBegin(); it != node.objectChildrenEnd(); ++it, ++i)
		{
			const string_view name = it.name();
			testAssert(name == node.getNameI((uint32)i));
			testAssert(it.valueNode().node_index == node.getChildNodeForIndex((uint32)i).node_index);

			// Lookups by name.  Names may be duplicated, so getChildNode may return an earlier pair's value.
			testAssert(node.hasChild(name));
			const JSONNode by_name = node.getChildNode(name);
			testAssert(by_name.node_index <= it.valueNode().node_index);

			try { node.getChildUIntValue(name); }                            catch(glare::Exception&) {}
			try { node.getChildUIntValueWithDefaultVal(name, 0); }           catch(glare::Exception&) {}
			try { node.getChildIntValue(name); }                             catch(glare::Exception&) {}
			try { node.getChildIntValueWithDefaultVal(name, 0); }            catch(glare::Exception&) {}
			try { node.getChildDoubleValue(name); }                          catch(glare::Exception&) {}
			try { node.getChildDoubleValueWithDefaultVal(name, 0.0); }       catch(glare::Exception&) {}
			try { node.getChildBoolValueWithDefaultVal(name, false); }       catch(glare::Exception&) {}
			try { node.getChildStringValue(name); }                          catch(glare::Exception&) {}
			try { node.getChildStringValueWithDefaultVal(name, "default"); } catch(glare::Exception&) {}
			try { node.getChildObject(name); }                               catch(glare::Exception&) {}
			try { node.getChildArray(name); }                                catch(glare::Exception&) {}
			node.hasChildWithStringValueEqualTo(name, "a");
			node.hasChildWithStringValueCaseInsensitiveEqualTo(name, "a");

			fuzzTraverseNode(parser, it.valueNode());
		}
		testAssert(i == num_pairs);

		// Lookups of a name that is probably missing.
		node.hasChild("__missing__");
		try { node.getChildNode("__missing__"); } catch(glare::Exception&) {}
		try { node.getChildIntValueWithDefaultVal("__missing__", 0); } catch(glare::Exception&) {}
	}
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		JSONParser parser;
		parser.parseBuffer((const char*)data, size);

		fuzzTraverseNode(parser, parser.rootNode());
	}
	catch(glare::Exception&)
	{
	}

	return 0;  // Non-zero return values are reserved for future use.
}
#endif


static void testStringEscapeSequence(const std::string& encoded_string, const std::string& target_decoding)
{
	try
	{
		JSONParser p;
		std::string s = "{ \"the string\": \"" + encoded_string + "\" }";
		p.parseBuffer(s.data(), s.size());

		JSONNode root_node = p.rootNode();
		testAssert(root_node.getType() == JSONNodeType::Type_Object);
		testAssert(root_node.numNameValPairs() == 1);

		testAssert(root_node.objectChildrenBegin().name() == "the string");
		testAssert(root_node.objectChildrenBegin().valueNode().getType() == JSONNodeType::Type_String);
		testAssert(root_node.objectChildrenBegin().valueNode().getStringValue() == target_decoding);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


static void testInvalidStringEscapeSequence(const std::string& encoded_string)
{
	try
	{
		JSONParser p;
		std::string s = "{ \"the string\": \"" + encoded_string + "\" }";
		p.parseBuffer(s.data(), s.size());
		failTest("Expected exception to be thrown.");
	}
	catch(glare::Exception&)
	{
	}
}


void JSONParser::test()
{
	conPrint("JSONParser::test()");

	try
	{
		JSONParser p;
		std::string s = "{ \"key_1\": \"value_1\", \"key_2\": \"value_2\" }";
		p.parseBuffer(s.data(), s.size());

		JSONNode root_node = p.rootNode();

		int i=0;
		for(auto it = root_node.objectChildrenBegin(); it != root_node.objectChildrenEnd(); ++it)
		{
			const string_view name = it.name();
			const JSONNode val_node = it.valueNode();

			if(i == 0)
			{
				testAssert(name == "key_1");
				testAssert(val_node.getStringValue() == "value_1");
			}
			else if(i == 1)
			{
				testAssert(name == "key_2");
				testAssert(val_node.getStringValue() == "value_2");
			}
			else
				failTest("expected i==0 or i==1");

			i++;
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}


	try
	{
		JSONParser p;
		p.parseFile(TestUtils::getTestReposDir() + "/testfiles/json/example.json");

		JSONNode root_ob = p.rootNode();
		testAssert(root_ob.getType() == JSONNodeType::Type_Object);
		testEqual(root_ob.numNameValPairs(), (uint32)8);

		testAssert(root_ob.getNameI(0) == "firstName");
		testAssert(root_ob.getChildNodeForIndex(0).getType() == JSONNodeType::Type_String);
		testAssert(root_ob.getChildNodeForIndex(0).getStringValue() == "John");

		testAssert(root_ob.getNameI(1) == "lastName");
		testAssert(root_ob.getChildNodeForIndex(1).getType() == JSONNodeType::Type_String);
		testAssert(root_ob.getChildNodeForIndex(1).getStringValue() == "Smith");

		testAssert(root_ob.getNameI(2) == "isAlive");
		testAssert(root_ob.getChildNodeForIndex(2).getType() == JSONNodeType::Type_Boolean);
		testAssert(root_ob.getChildNodeForIndex(2).getBoolValue() == true);

		testAssert(root_ob.getNameI(3) == "age");
		testAssert(root_ob.getChildNodeForIndex(3).getType() == JSONNodeType::Type_Number);
		testAssert(root_ob.getChildNodeForIndex(3).getDoubleValue() == 27.0);



		testAssert(root_ob.getNameI(7) == "spouse");
		testAssert(root_ob.getChildNodeForIndex(7).getType() == JSONNodeType::Type_Null);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}


	//--------- Test tring to parse empty string ----------
	try
	{
		JSONParser p;
		std::string s = "";
		p.parseBuffer(s.data(), s.size());

		failTest("Expected excep to be thrown.");
	}
	catch(glare::Exception&)
	{}

	//--------- Test empty array ----------
	try
	{
		JSONParser p;
		std::string s = "{ \"a\": [] }";
		p.parseBuffer(s.data(), s.size());

		JSONNode root_ob = p.rootNode();
		testAssert(root_ob.getType() == JSONNodeType::Type_Object);
		testAssert(root_ob.numNameValPairs() == 1);

		testAssert(root_ob.getNameI(0) == "a");
		testAssert(root_ob.getChildNodeForIndex(0).getType() == JSONNodeType::Type_Array);
		testAssert(root_ob.getChildNodeForIndex(0).numChildren() == 0);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	//--------- Test empty object ----------
	try
	{
		JSONParser p;
		std::string s = "{ \"a\": {} }";
		p.parseBuffer(s.data(), s.size());

		JSONNode root_ob = p.rootNode();
		testAssert(root_ob.getType() == JSONNodeType::Type_Object);
		testAssert(root_ob.numNameValPairs() == 1);

		testAssert(root_ob.getNameI(0) == "a");
		testAssert(root_ob.getChildNodeForIndex(0).getType() == JSONNodeType::Type_Object);
		testAssert(root_ob.getChildNodeForIndex(0).numNameValPairs() == 0);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	//--------- Test leading and/or trailing whitespace ----------
	try
	{
		JSONParser p;
		std::string s = " {}";
		p.parseBuffer(s.data(), s.size());

		JSONNode root_ob = p.rootNode();
		testAssert(root_ob.getType() == JSONNodeType::Type_Object);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
	try
	{
		JSONParser p;
		std::string s = "{} ";
		p.parseBuffer(s.data(), s.size());

		JSONNode root_ob = p.rootNode();
		testAssert(root_ob.getType() == JSONNodeType::Type_Object);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	//--------- Test parse depth limit ----------
	try
	{
		// Moderately nested input should parse.
		{
			const std::string s = std::string(100, '[') + std::string(100, ']');
			JSONParser p;
			p.parseBuffer(s.data(), s.size());
		}
		{
			std::string s;
			for(int i=0; i<100; ++i)
				s += "{\"a\":";
			s += "{}" + std::string(100, '}');
			JSONParser p;
			p.parseBuffer(s.data(), s.size());
		}

		// Very deep nesting should throw, not overflow the stack.
		{
			const std::string s = std::string(1000000, '[');
			JSONParser p;
			testThrowsExcepContainingString([&]() { p.parseBuffer(s.data(), s.size()); }, "depth");
		}
		{
			std::string s;
			for(int i=0; i<100000; ++i)
				s += "{\"a\":";
			JSONParser p;
			testThrowsExcepContainingString([&]() { p.parseBuffer(s.data(), s.size()); }, "depth");
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	//--------- Test hasChildWithStringValueEqualTo and hasChildWithStringValueCaseInsensitiveEqualTo ----------
	try
	{
		JSONParser p;
		std::string s = "{ \"type\": \"sale\", \"addr\": \"0xAbCd\", \"empty\": \"\", \"num\": 1, \"null\": null, \"ob\": {}, \"arr\": [\"sale\"] }";
		p.parseBuffer(s.data(), s.size());

		const JSONNode root_ob = p.rootNode();

		// hasChildWithStringValueEqualTo
		testAssert(root_ob.hasChildWithStringValueEqualTo("type", "sale"));
		testAssert(!root_ob.hasChildWithStringValueEqualTo("type", "Sale")); // Case sensitive
		testAssert(!root_ob.hasChildWithStringValueEqualTo("type", "sal")); // Target is prefix of value
		testAssert(!root_ob.hasChildWithStringValueEqualTo("type", "sales")); // Value is prefix of target
		testAssert(!root_ob.hasChildWithStringValueEqualTo("type", ""));
		testAssert(root_ob.hasChildWithStringValueEqualTo("addr", "0xAbCd"));
		testAssert(!root_ob.hasChildWithStringValueEqualTo("addr", "0xabcd"));
		testAssert(root_ob.hasChildWithStringValueEqualTo("empty", ""));
		testAssert(!root_ob.hasChildWithStringValueEqualTo("empty", "a"));
		testAssert(!root_ob.hasChildWithStringValueEqualTo("missing", "sale")); // Missing child
		testAssert(!root_ob.hasChildWithStringValueEqualTo("missing", ""));
		testAssert(!root_ob.hasChildWithStringValueEqualTo("num", "1")); // Non-string children
		testAssert(!root_ob.hasChildWithStringValueEqualTo("null", "null"));
		testAssert(!root_ob.hasChildWithStringValueEqualTo("ob", ""));
		testAssert(!root_ob.hasChildWithStringValueEqualTo("arr", "sale"));

		// hasChildWithStringValueCaseInsensitiveEqualTo.  target_value must be lowercase.
		testAssert(root_ob.hasChildWithStringValueCaseInsensitiveEqualTo("type", "sale"));
		testAssert(root_ob.hasChildWithStringValueCaseInsensitiveEqualTo("addr", "0xabcd")); // Mixed-case value
		testAssert(!root_ob.hasChildWithStringValueCaseInsensitiveEqualTo("addr", "0xabce"));
		testAssert(!root_ob.hasChildWithStringValueCaseInsensitiveEqualTo("addr", "0xabc")); // Target is prefix of value
		testAssert(!root_ob.hasChildWithStringValueCaseInsensitiveEqualTo("addr", "0xabcde")); // Value is prefix of target
		testAssert(root_ob.hasChildWithStringValueCaseInsensitiveEqualTo("empty", ""));
		testAssert(!root_ob.hasChildWithStringValueCaseInsensitiveEqualTo("empty", "a"));
		testAssert(!root_ob.hasChildWithStringValueCaseInsensitiveEqualTo("missing", "sale")); // Missing child
		testAssert(!root_ob.hasChildWithStringValueCaseInsensitiveEqualTo("num", "1")); // Non-string children
		testAssert(!root_ob.hasChildWithStringValueCaseInsensitiveEqualTo("null", "null"));
		testAssert(!root_ob.hasChildWithStringValueCaseInsensitiveEqualTo("arr", "sale"));

		// Calling on a non-object node should throw.
		const JSONNode& arr_node = root_ob.getChildArray("arr");
		try
		{
			arr_node.hasChildWithStringValueEqualTo("type", "sale");
			failTest("Expected excep to be thrown.");
		}
		catch(glare::Exception&)
		{}
		try
		{
			arr_node.hasChildWithStringValueCaseInsensitiveEqualTo("type", "sale");
			failTest("Expected excep to be thrown.");
		}
		catch(glare::Exception&)
		{}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	//--------- Test getIntValue and getUIntValue range checking ----------
	try
	{
		JSONParser p;
		std::string s = "{ \"zero\": 0, \"neg_zero\": -0.0, \"a\": 27, \"neg_a\": -27, \"frac\": 27.9, \"neg_frac\": -27.9, \"small_neg_frac\": -0.5, \"neg_one\": -1, "
			"\"int_max\": 2147483647, \"int_max_frac\": 2147483647.5, \"int_max_plus_one\": 2147483648, "
			"\"int_min\": -2147483648, \"int_min_frac\": -2147483648.5, \"int_min_minus_one\": -2147483649, "
			"\"big\": 1e19, \"two_pow_64\": 18446744073709551616, \"huge\": 1e300, \"neg_huge\": -1e300, \"str\": \"1\" }";
		p.parseBuffer(s.data(), s.size());

		const JSONNode& root_ob = p.rootNode();

		// getIntValue: values are truncated towards zero.
		testEqual(root_ob.getChildIntValue("zero"), 0);
		testEqual(root_ob.getChildIntValue("neg_zero"), 0);
		testEqual(root_ob.getChildIntValue("a"), 27);
		testEqual(root_ob.getChildIntValue("neg_a"), -27);
		testEqual(root_ob.getChildIntValue("frac"), 27);
		testEqual(root_ob.getChildIntValue("neg_frac"), -27);
		testEqual(root_ob.getChildIntValue("small_neg_frac"), 0);
		testEqual(root_ob.getChildIntValue("neg_one"), -1);
		testEqual(root_ob.getChildIntValue("int_max"), std::numeric_limits<int>::max());
		testEqual(root_ob.getChildIntValue("int_max_frac"), std::numeric_limits<int>::max());
		testEqual(root_ob.getChildIntValue("int_min"), std::numeric_limits<int>::min());
		testEqual(root_ob.getChildIntValue("int_min_frac"), std::numeric_limits<int>::min());
		testThrowsExcepContainingString([&]() { root_ob.getChildIntValue("int_max_plus_one"); }, "out of range");
		testThrowsExcepContainingString([&]() { root_ob.getChildIntValue("int_min_minus_one"); }, "out of range");
		testThrowsExcepContainingString([&]() { root_ob.getChildIntValue("big"); }, "out of range");
		testThrowsExcepContainingString([&]() { root_ob.getChildIntValue("huge"); }, "out of range");
		testThrowsExcepContainingString([&]() { root_ob.getChildIntValue("neg_huge"); }, "out of range");
		testThrowsExcepContainingString([&]() { root_ob.getChildIntValue("str"); }, "Expected type Number");

		// getChildIntValueWithDefaultVal should throw for an out-of-range value, not return the default.
		testThrowsExcepContainingString([&]() { root_ob.getChildIntValueWithDefaultVal("int_max_plus_one", 0); }, "out of range");
		testEqual(root_ob.getChildIntValueWithDefaultVal("missing", 123), 123);

		// getUIntValue: values are truncated towards zero, negative values are rejected.
		testEqual(root_ob.getChildUIntValue("zero"), (size_t)0);
		testEqual(root_ob.getChildUIntValue("neg_zero"), (size_t)0);
		testEqual(root_ob.getChildUIntValue("a"), (size_t)27);
		testEqual(root_ob.getChildUIntValue("frac"), (size_t)27);
		testEqual(root_ob.getChildUIntValue("int_max_plus_one"), (size_t)2147483648u);
		if(sizeof(size_t) == 8)
			testEqual(root_ob.getChildUIntValue("big"), (size_t)10000000000000000000ull); // 1e19 is exactly representable as a double.
		testThrowsExcepContainingString([&]() { root_ob.getChildUIntValue("neg_a"); }, "out of range");
		testThrowsExcepContainingString([&]() { root_ob.getChildUIntValue("neg_frac"); }, "out of range");
		testThrowsExcepContainingString([&]() { root_ob.getChildUIntValue("small_neg_frac"); }, "out of range");
		testThrowsExcepContainingString([&]() { root_ob.getChildUIntValue("neg_one"); }, "out of range");
		testThrowsExcepContainingString([&]() { root_ob.getChildUIntValue("two_pow_64"); }, "out of range");
		testThrowsExcepContainingString([&]() { root_ob.getChildUIntValue("huge"); }, "out of range");
		testThrowsExcepContainingString([&]() { root_ob.getChildUIntValue("neg_huge"); }, "out of range");
		testThrowsExcepContainingString([&]() { root_ob.getChildUIntValue("str"); }, "Expected type Number");

		// getChildUIntValueWithDefaultVal should throw for an out-of-range value, not return the default.
		testThrowsExcepContainingString([&]() { root_ob.getChildUIntValueWithDefaultVal("neg_one", 0); }, "out of range");
		testEqual(root_ob.getChildUIntValueWithDefaultVal("missing", 123), (size_t)123);

		// Non-finite values.  Set these directly on nodes since they can't be written in JSON.
		const double non_finite_vals[] = { std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() };
		for(size_t i=0; i<staticArrayNumElems(non_finite_vals); ++i)
		{
			JSONNodeInternal node;
			node.type = JSONNodeType::Type_Number;
			node.value.double_v = non_finite_vals[i];

			testThrowsExcepContainingString([&]() { node.getIntValue(); }, "out of range");
			testThrowsExcepContainingString([&]() { node.getUIntValue(); }, "out of range");
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	//--------- Test empty string ----------
	try
	{
		JSONParser p;
		std::string s = "{ \"a\": \"\" }";
		p.parseBuffer(s.data(), s.size());

		JSONNode root_ob = p.rootNode();
		testAssert(root_ob.getType() == JSONNodeType::Type_Object);
		testAssert(root_ob.numNameValPairs() == 1);

		testAssert(root_ob.getNameI(0) == "a");
		testAssert(root_ob.getChildNodeForIndex(0).getType() == JSONNodeType::Type_String);
		testAssert(root_ob.getChildNodeForIndex(0).getStringValue() == "");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	//--------- Test some numbers ----------
	try
	{
		JSONParser p;
		std::string s = "{ \"a\": [1.0, -1.0, 1, 2, 1e20, -1.0E+20, 15.4e-5] }";
		p.parseBuffer(s.data(), s.size());

		JSONNode root_ob = p.rootNode();
		testAssert(root_ob.getType() == JSONNodeType::Type_Object);
		testAssert(root_ob.numNameValPairs() == 1);

		testAssert(root_ob.objectChildrenBegin().name() == "a");

		JSONNode array_node = root_ob.getChildNodeForIndex(0);
		testAssert(array_node.getType() == JSONNodeType::Type_Array);
		testEqual(array_node.numChildren(), (size_t)7);

		testAssert(array_node.getArrayChildNode(0).getType() == JSONNodeType::Type_Number);
		testAssert(array_node.getArrayChildNode(0).getDoubleValue() == 1.0);

		testAssert(array_node.getArrayChildNode(1).getType() == JSONNodeType::Type_Number);
		testAssert(array_node.getArrayChildNode(1).getDoubleValue() == -1.0);

		testAssert(array_node.getArrayChildNode(2).getType() == JSONNodeType::Type_Number);
		testAssert(array_node.getArrayChildNode(2).getDoubleValue() == 1.0);

		testAssert(array_node.getArrayChildNode(3).getType() == JSONNodeType::Type_Number);
		testAssert(array_node.getArrayChildNode(3).getDoubleValue() == 2.0);

		testAssert(array_node.getArrayChildNode(4).getType() == JSONNodeType::Type_Number);
		testAssert(array_node.getArrayChildNode(4).getDoubleValue() == 1.0e20);

		testAssert(array_node.getArrayChildNode(5).getType() == JSONNodeType::Type_Number);
		testAssert(array_node.getArrayChildNode(5).getDoubleValue() == -1.0e20);
		
		testAssert(array_node.getArrayChildNode(6).getType() == JSONNodeType::Type_Number);
		testAssert(array_node.getArrayChildNode(6).getDoubleValue() == 15.4e-5);

	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}


	testStringEscapeSequence("\\\"", "\""); //		\" should decode to "
	testStringEscapeSequence("\\\\", "\\"); //		backslash backslash should decode to backslash. 
	testStringEscapeSequence("\\/", "/");
	testStringEscapeSequence("\\b", "\b");
	testStringEscapeSequence("\\f", "\f");
	testStringEscapeSequence("\\n", "\n");
	testStringEscapeSequence("\\r", "\r");
	testStringEscapeSequence("\\t", "\t");
	
	testStringEscapeSequence("\\u0000", UTF8Utils::encodeCodePoint(0));
	testStringEscapeSequence("\\u1234", UTF8Utils::encodeCodePoint(0x1234));
	testStringEscapeSequence("\\uabCD", UTF8Utils::encodeCodePoint(0xABCD));


	testInvalidStringEscapeSequence("\\");
	testInvalidStringEscapeSequence("\\a");
	testInvalidStringEscapeSequence("\\\0");

	testInvalidStringEscapeSequence("\\u0");
	testInvalidStringEscapeSequence("\\u00");
	testInvalidStringEscapeSequence("\\u000");
	testInvalidStringEscapeSequence("\\uz");
	testInvalidStringEscapeSequence("\\u0z");
	testInvalidStringEscapeSequence("\\u00z");
	testInvalidStringEscapeSequence("\\u000z");

	// Perf test
	if(false)
	{
		const std::string path = TestUtils::getTestReposDir() + "/testfiles/json/nobel-prizes.json";
		Timer timer;
		const int N = 10;
		size_t num_nodes = 0;
		for(int i=0; i<N; ++i)
		{
			JSONParser p;
			p.parseFile(path);

			num_nodes = p.nodes.size();
		}

		const double elapsed = timer.elapsed() / N;
		const double decode_speed = FileUtils::getFileSize(path) / elapsed;
		conPrint("elapsed: " + toString(elapsed) + " s (" + toString(decode_speed * 1.0e-6) + " MB/s)");
		conPrint("num nodes: " + toString(num_nodes));
	}

	conPrint("JSONParser::test() done.");
}


#endif // BUILD_TESTS
