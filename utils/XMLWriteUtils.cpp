/*=====================================================================
XMLWriteUtils.cpp
-----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "XMLWriteUtils.h"


#include "StringUtils.h"


namespace XMLWriteUtils
{

// Do explicit template instantiation
template void writeVec3ToXML(std::string& xml, const string_view elem_name, const Vec3<float>&  vec, int tab_depth);
template void writeVec3ToXML(std::string& xml, const string_view elem_name, const Vec3<double>& vec, int tab_depth);


inline void appendTabsAndElemOpenTag(std::string& xml, const string_view elem_name, int tab_depth)
{
	xml.append(tab_depth, '\t');
	xml += "<";
#if __cplusplus >= 201703L // If c++ version is >= c++17:
	xml += elem_name;
#else
	xml += toString(elem_name);
#endif
	xml += ">";
}


inline void appendElemCloseTag(std::string& xml, const string_view elem_name)
{
	xml += "</";
#if __cplusplus >= 201703L // If c++ version is >= c++17:
	xml += elem_name;
#else
	xml += toString(elem_name);
#endif
	xml += ">\n";
}


void writeColour3fToXML(std::string& xml, const string_view elem_name, const Colour3f& col, int tab_depth)
{
	appendTabsAndElemOpenTag(xml, elem_name, tab_depth);
	xml += toString(col.r) + " " + toString(col.g) + " " + toString(col.b);
	appendElemCloseTag(xml, elem_name);
}


// Returns true if there is leading or trailing whitespace, or contiguous blocks of whitespace.
static bool needCData(const string_view s)
{
	// An empty string does not need a CDATA section.
	if(s.empty())
		return false;

	assert(s.size() >= 1);

	// Check for leading whitespace
	if(::isWhitespace(s[0]))
		return true;

	// Check for trailing whitespace
	if(::isWhitespace(s[s.size() - 1]))
		return true;

	// Check for contiguous blocks of more than one whitespace character.
	bool prev_was_ws = false;
	for(size_t i=0; i<s.size(); ++i)
	{
		const bool current_is_ws = ::isWhitespace(s[i]);
		if(current_is_ws && prev_was_ws)
			return true;

		prev_was_ws = current_is_ws;
	}

	return false;
}



/*
Replace "<" with "&lt;" etc..
From Escaping::HTMLEscape in webserver/Escaping.cpp
*/
static const std::string XMLEscape(const string_view s)
{
	std::string result;
	result.reserve(s.size());

	for(size_t i=0; i<s.size(); ++i)
	{
		if(s[i] == '<')
			result += "&lt;";
		else if(s[i] == '>')
			result += "&gt;";
		else if(s[i] == '"')
			result += "&quot;";
		else if(s[i] == '&')
			result += "&amp;";
		else
			result.push_back(s[i]);
	}
	return result;
}


void writeStringElemToXML(std::string& xml, const string_view elem_name, const std::string& string_val, int tab_depth)
{
	appendTabsAndElemOpenTag(xml, elem_name, tab_depth);
	if(needCData(string_val))
	{
		xml += "<![CDATA[" + string_val + "]]>";
	}
	else
	{
		xml += XMLEscape(string_val);
	}
	appendElemCloseTag(xml, elem_name);
}


void writeStringElemToXML(std::string& xml, const string_view elem_name, const glare::SharedImmutableString& string_val, int tab_depth)
{
	appendTabsAndElemOpenTag(xml, elem_name, tab_depth);
	if(needCData(string_val.toStringView()))
	{
		xml += "<![CDATA[" + string_val.toString() + "]]>";
	}
	else
	{
		xml += XMLEscape(string_val.toStringView());
	}
	appendElemCloseTag(xml, elem_name);
}


void writeUInt32ToXML(std::string& xml, const string_view elem_name, uint32 val, int tab_depth)
{
	appendTabsAndElemOpenTag(xml, elem_name, tab_depth);
	xml += toString(val);
	appendElemCloseTag(xml, elem_name);
}


void writeUInt64ToXML(std::string& xml, const string_view elem_name, uint64 val, int tab_depth)
{
	appendTabsAndElemOpenTag(xml, elem_name, tab_depth);
	xml += toString(val);
	appendElemCloseTag(xml, elem_name);
}


void writeInt32ToXML(std::string& xml, const string_view elem_name, int32 val, int tab_depth)
{
	appendTabsAndElemOpenTag(xml, elem_name, tab_depth);
	xml += toString(val);
	appendElemCloseTag(xml, elem_name);
}


void writeFloatToXML(std::string& xml, const string_view elem_name, float val, int tab_depth)
{
	appendTabsAndElemOpenTag(xml, elem_name, tab_depth);
	xml += toString(val);
	appendElemCloseTag(xml, elem_name);
}


template <class T>
void writeVec3ToXML(std::string& xml, const string_view elem_name, const Vec3<T>& vec, int tab_depth)
{
	appendTabsAndElemOpenTag(xml, elem_name, tab_depth);
	xml += toString(vec.x) + " " + toString(vec.y) + " " + toString(vec.z);
	appendElemCloseTag(xml, elem_name);
}


} // end namespace XMLWriteUtils


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "IndigoXMLDoc.h"
#include "Timer.h"
#include "ConPrint.h"


void XMLWriteUtils::test()
{
	
}


#endif // BUILD_TESTS
