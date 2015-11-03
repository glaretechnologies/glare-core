/*=====================================================================
formatdecoderobj.h
------------------
File created by ClassTemplate on Sat Apr 30 18:15:18 2005
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include <string>
namespace Indigo { class Mesh; }


/*=====================================================================
FormatDecoderObj
----------------

=====================================================================*/
class FormatDecoderObj
{
public:
	static void streamModel(const std::string& filename, Indigo::Mesh& handler, float scale); // Throws Indigo::Exception on failure.

	static void test();
};
