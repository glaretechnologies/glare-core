/*=====================================================================
FormatDecoderPLY.h
------------------
File created by ClassTemplate on Sat Dec 02 04:33:29 2006
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include <string>
namespace Indigo { class Mesh; }


/*=====================================================================
FormatDecoderPLY
----------------

=====================================================================*/
class FormatDecoderPLY
{
public:
	static void streamModel(const std::string& filename, Indigo::Mesh& handler, float scale); // Throws glare::Exception on failure.
};
