/*=====================================================================
FormatDecoderSTL.h
------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include <string>
namespace Indigo { class Mesh; }


/*=====================================================================
FormatDecoderSTL
----------------
Loads STL files, can handle both ASCII and binary STL files.
See https://en.wikipedia.org/wiki/STL_(file_format),
http://www.fabbers.com/tech/STL_Format
=====================================================================*/
class FormatDecoderSTL
{
public:
	static void streamModel(const std::string& filename, Indigo::Mesh& handler, float scale); // throws glare::Exception on failure

	static void test();
};
