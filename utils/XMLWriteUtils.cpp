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
	xml += elem_name;
	xml += ">";
}


inline void appendElemCloseTag(std::string& xml, const string_view elem_name)
{
	xml += "</";
	xml += elem_name;
	xml += ">\n";
}


void writeColour3fToXML(std::string& xml, const string_view elem_name, const Colour3f& col, int tab_depth)
{
	appendTabsAndElemOpenTag(xml, elem_name, tab_depth);
	xml += toString(col.r) + " " + toString(col.g) + " " + toString(col.b);
	appendElemCloseTag(xml, elem_name);
}


void writeStringElemToXML(std::string& xml, const string_view elem_name, const std::string& string_val, int tab_depth)
{
	appendTabsAndElemOpenTag(xml, elem_name, tab_depth);
	if(!string_val.empty())
		xml += "<![CDATA[" + string_val + "]]>";
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
