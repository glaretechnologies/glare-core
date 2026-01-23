/*=====================================================================
XMLParseUtils.h
----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "SharedImmutableString.h"
#include "string_view.h"
#include "../graphics/colour3.h"
#include "../maths/Matrix2.h"
#include <string>
namespace pugi { class xml_node; }
namespace glare { class SharedStringTable; }


/*=====================================================================
XMLParseUtils
-------------
Utility functions for parsing stuff from a pugixml object.

All functions throw glare::Exception on failure.

Adapted from indigo\trunk\dll\include\IndigoSceneParseUtils.h

TODO: Make IndigoSceneParseUtils use these functions.
The main problem with that is that IndigoSceneParseUtils currently throws SceneParseUtilsExcep,
and catching glare::Exception and rethrowing SceneParseUtilsExcep might be a bit slow.
=====================================================================*/
namespace XMLParseUtils
{
	int32 parseIntDirectly(pugi::xml_node elem); // Given an element like <a>10</a>, returns 10.
	int32 parseInt(pugi::xml_node elem, const char* elemname); // Given an element like <a><b>10</b></a>, parseInt(a_elem, "b") returns 10.
	int32 parseIntWithDefault(pugi::xml_node elem, const char* elemname, int32 default_val); // Like parseInt but returns the default value if no such named element found.

	uint32 parseUInt(pugi::xml_node elem, const char* elemname);

	uint64 parseUInt64Directly(pugi::xml_node elem);
	uint64 parseUInt64(pugi::xml_node elem, const char* elemname);
	uint64 parseUInt64WithDefault(pugi::xml_node elem, const char* elemname, uint64 default_val);

	const std::string parseString(pugi::xml_node elem, const char* elemname);
	const std::string parseStringWithDefault(pugi::xml_node elem, const char* elemname, const string_view default_val);
	glare::SharedImmutableString parseSharedImmutableStringWithDefault(pugi::xml_node elem, const char* elemname, const char* default_val, glare::SharedStringTable* table);

	double parseDoubleDirectly(pugi::xml_node elem);
	double parseDouble(pugi::xml_node elem, const char* elemname);
	double parseDoubleWithDefault(pugi::xml_node elem, const char* elemname, double default_val);

	float parseFloatDirectly(pugi::xml_node elem);
	float parseFloat(pugi::xml_node elem, const char* elemname);
	float parseFloatWithDefault(pugi::xml_node elem, const char* elemname, float default_val);

	bool parseBoolDirectly(pugi::xml_node elem);
	bool parseBool(pugi::xml_node elem, const char* elemname);
	bool parseBoolWithDefault(pugi::xml_node elem, const char* elemname, bool default_val);

	Colour3f parseColour3fWithDefault(pugi::xml_node elem, const char* elemname, const Colour3f& default_val);
		
	Vec3d parseVec3dDirectly(pugi::xml_node elem);
	Vec3d parseVec3d(pugi::xml_node elem, const char* elemname);
	Vec3d parseVec3dWithDefault(pugi::xml_node elem, const char* elemname, const Vec3d& default_val);

	Vec3f parseVec3fDirectly(pugi::xml_node elem);
	Vec3f parseVec3f(pugi::xml_node elem, const char* elemname);
	Vec3f parseVec3fWithDefault(pugi::xml_node elem, const char* elemname, const Vec3f& default_val);

	Matrix2f parseMatrix2f(pugi::xml_node elem, const char* elemname);


	pugi::xml_node getChildElement(pugi::xml_node elem, const char* name); // Throws glare::Exception if not found.

	const char* getElementText(pugi::xml_node elem);

	const std::string elemContext(pugi::xml_node elem);

	void test();
};



