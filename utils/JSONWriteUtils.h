/*=====================================================================
JSONWriteUtils.h
----------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "string_view.h"
#include "../maths/vec3.h"
#include "../graphics/colour3.h"
#include <string>


/*=====================================================================
JSONWriteUtils
--------------
Utility functions for writing JSON, analogous to XMLWriteUtils.

Each write function appends a `"name": value,` pair - note the trailing comma -
to the JSON string.  The caller is responsible for stripping the final trailing
comma before the closing '}' or ']' (e.g. by replacing it, since JSON does not
allow trailing commas).
=====================================================================*/
namespace JSONWriteUtils
{
	void writeStringToJSON  (std::string& json_out, const string_view name, const string_view value);
	void writeUInt32ToJSON  (std::string& json_out, const string_view name, uint32 val);
	void writeUInt64ToJSON  (std::string& json_out, const string_view name, uint64 val);
	void writeInt32ToJSON   (std::string& json_out, const string_view name, int32 val);
	void writeFloatToJSON   (std::string& json_out, const string_view name, float val);
	void writeColour3fToJSON(std::string& json_out, const string_view name, const Colour3f& col);

	template <class T>
	void writeVec3ToJSON(std::string& json_out, const string_view name, const Vec3<T>& vec);

	// Escape a string for use as a JSON string value (does not add the surrounding quotes).
	const std::string JSONEscape(const string_view s);

	void test();
};
