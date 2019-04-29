/*=====================================================================
XMLParseUtils.h
----------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include <Platform.h>
#include <pugixml.hpp>


/*=====================================================================
XMLParseUtils
-------------
Utility functions for parsing stuff from a PugiXML object.

All functions throw Indigo::Exception on failure.

Adapted from indigo\trunk\dll\include\IndigoSceneParseUtils.h

TODO: Make IndigoSceneParseUtils use these functions.
The main problem with that is that IndigoSceneParseUtils currently throws SceneParseUtilsExcep,
and catching Indigo::Exception and rethrowing SceneParseUtilsExcep might be a bit slow.
=====================================================================*/
namespace XMLParseUtils
{
	int32 parseIntDirectly(pugi::xml_node elem); // Given an element like <a>10</a>, returns 10.
	int32 parseInt(pugi::xml_node elem, const char* elemname); // Given an element like <a><b>10</b></a>, parseInt(a_elem, "b") returns 10.
	int32 parseIntWithDefault(pugi::xml_node elem, const char* elemname, int32 default_val); // Like parseInt but returns the default value if no such named element found.

	uint32 parseUInt(pugi::xml_node elem, const char* elemname);
	uint32 parseUInt(const char* text);

	const std::string parseString(pugi::xml_node elem, const char* elemname);
	const std::string parseStringWithDefault(pugi::xml_node elem, const char* elemname, const char* default_val);

	double parseDoubleDirectly(pugi::xml_node elem);
	double parseDouble(pugi::xml_node elem, const char* elemname);
	double parseDoubleWithDefault(pugi::xml_node elem, const char* elemname, double default_val);

	bool parseBoolDirectly(pugi::xml_node elem);
	bool parseBool(pugi::xml_node elem, const char* elemname);
	bool parseBoolWithDefault(pugi::xml_node elem, const char* elemname, bool default_val);

	pugi::xml_node getChildElement(pugi::xml_node elem, const char* name); // Throws Indigo::Exception if not found.

	const char* getElementText(pugi::xml_node elem);

	const std::string elemContext(pugi::xml_node elem);

	void test();
};



