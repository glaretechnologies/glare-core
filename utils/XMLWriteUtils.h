/*=====================================================================
XMLWriteUtils.h
---------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "string_view.h"
#include "../maths/vec3.h"
#include "../graphics/colour3.h"
#include <string>


/*=====================================================================
XMLWriteUtils
-------------
Utility functions for writing to XML.
=====================================================================*/
namespace XMLWriteUtils
{
	void writeColour3fToXML(std::string& xml, const string_view elem_name, const Colour3f& col, int tab_depth);
	void writeStringElemToXML(std::string& xml, const string_view elem_name, const std::string& string_val, int tab_depth);
	void writeUInt64ToXML(std::string& xml, const string_view elem_name, uint64 val, int tab_depth);
	void writeInt32ToXML(std::string& xml, const string_view elem_name, int32 val, int tab_depth);
	void writeFloatToXML(std::string& xml, const string_view elem_name, float val, int tab_depth);
	
	template <class T>
	void writeVec3ToXML(std::string& xml, const string_view elem_name, const Vec3<T>& vec, int tab_depth);

	void test();
};
