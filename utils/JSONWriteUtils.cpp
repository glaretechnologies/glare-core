/*=====================================================================
JSONWriteUtils.cpp
------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "JSONWriteUtils.h"


#include "StringUtils.h"


namespace JSONWriteUtils
{

// Do explicit template instantiation
template void writeVec3ToJSON(std::string& json_out, const string_view name, const Vec3<float>&  vec);
template void writeVec3ToJSON(std::string& json_out, const string_view name, const Vec3<double>& vec);


const std::string JSONEscape(const string_view s)
{
	std::string result;
	result.reserve(s.size() + 4);

	for(size_t i=0; i<s.size(); ++i)
	{
		const char c = s[i];
		switch(c)
		{
		case '"':  result += "\\\""; break;
		case '\\': result += "\\\\"; break;
		case '\b': result += "\\b"; break;
		case '\f': result += "\\f"; break;
		case '\n': result += "\\n"; break;
		case '\r': result += "\\r"; break;
		case '\t': result += "\\t"; break;
		default:
			if((unsigned char)c < 0x20) // Other control characters must be written as \u00XX.
			{
				const char* const hex = "0123456789abcdef";
				result += "\\u00";
				result.push_back(hex[((unsigned char)c >> 4) & 0xF]);
				result.push_back(hex[ (unsigned char)c       & 0xF]);
			}
			else
				result.push_back(c);
		}
	}
	return result;
}


static inline void appendNameAndColon(std::string& json_out, const string_view name)
{
	json_out += '"';
	json_out += toString(name);
	json_out += "\":";
}


void writeStringToJSON(std::string& json_out, const string_view name, const string_view value)
{
	appendNameAndColon(json_out, name);
	json_out += '"';
	json_out += JSONEscape(value);
	json_out += "\",";
}


void writeUInt32ToJSON(std::string& json_out, const string_view name, uint32 val)
{
	appendNameAndColon(json_out, name);
	json_out += toString(val);
	json_out += ',';
}


void writeUInt64ToJSON(std::string& json_out, const string_view name, uint64 val)
{
	appendNameAndColon(json_out, name);
	json_out += toString(val);
	json_out += ',';
}


void writeInt32ToJSON(std::string& json_out, const string_view name, int32 val)
{
	appendNameAndColon(json_out, name);
	json_out += toString(val);
	json_out += ',';
}


void writeFloatToJSON(std::string& json_out, const string_view name, float val)
{
	appendNameAndColon(json_out, name);
	json_out += toString(val);
	json_out += ',';
}


void writeColour3fToJSON(std::string& json_out, const string_view name, const Colour3f& col)
{
	appendNameAndColon(json_out, name);
	json_out += "{\"r\":" + toString(col.r) + ",\"g\":" + toString(col.g) + ",\"b\":" + toString(col.b) + "},";
}


template <class T>
void writeVec3ToJSON(std::string& json_out, const string_view name, const Vec3<T>& vec)
{
	appendNameAndColon(json_out, name);
	json_out += "{\"x\":" + toString(vec.x) + ",\"y\":" + toString(vec.y) + ",\"z\":" + toString(vec.z) + "},";
}


} // end namespace JSONWriteUtils


#if BUILD_TESTS


#include "TestUtils.h"


void JSONWriteUtils::test()
{
	// Test JSONEscape
	testAssert(JSONEscape("") == "");
	testAssert(JSONEscape("hello") == "hello");
	testAssert(JSONEscape("a\"b") == "a\\\"b"); // Double-quote
	testAssert(JSONEscape("a\\b") == "a\\\\b"); // Backslash

	// The two-character short escapes.
	testAssert(JSONEscape(std::string("a\nb")) == "a\\nb");
	testAssert(JSONEscape(std::string("a\tb")) == "a\\tb");
	testAssert(JSONEscape(std::string("a\rb")) == "a\\rb");
	testAssert(JSONEscape(std::string("a\bb")) == "a\\bb");
	testAssert(JSONEscape(std::string("a\fb")) == "a\\fb");

	// A CR-LF must become \r\n.
	testAssert(JSONEscape(std::string("\r\n")) == "\\r\\n");

	// Control characters without a short escape must use \u00XX, with correct (lowercase) hex.
	testAssert(JSONEscape(std::string(1, '\x00')) == "\\u0000");
	testAssert(JSONEscape(std::string(1, '\x01')) == "\\u0001");
	testAssert(JSONEscape(std::string(1, '\x0B')) == "\\u000b"); // vertical tab (no short escape)
	testAssert(JSONEscape(std::string(1, '\x10')) == "\\u0010"); // 0x10-0x1F is the range the old code got wrong
	testAssert(JSONEscape(std::string(1, '\x1F')) == "\\u001f");

	// Characters >= 0x20 are passed through unescaped (0x7F / DEL is allowed unescaped in JSON).
	testAssert(JSONEscape(std::string(1, '\x20')) == " ");
	testAssert(JSONEscape(std::string(1, '\x7F')) == std::string(1, '\x7F'));

	// A mix of escaped and unescaped characters in one string.
	testAssert(JSONEscape(std::string("line1\nline2\t\"end\"")) == "line1\\nline2\\t\\\"end\\\"");

	// Test a simple object build with the trailing-comma convention.
	{
		std::string s = "{";
		writeUInt64ToJSON(s, "uid", 123);
		writeStringToJSON(s, "name", "bob");
		writeFloatToJSON(s, "x", 1.5f);
		if(s.back() == ',') s.back() = '}'; else s += '}';
		testAssert(s == "{\"uid\":123,\"name\":\"bob\",\"x\":1.5}");
	}
}


#endif // BUILD_TESTS
