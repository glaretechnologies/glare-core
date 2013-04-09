/*=====================================================================
modeldecoder.h
--------------
File created by ClassTemplate on Sun Nov 07 02:05:28 2004
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "modelformatdecoder.h"
#include <string>
namespace Indigo { class Mesh; }


/*=====================================================================
ModelDecoder
------------

=====================================================================*/
class ModelDecoder
{
public:
	ModelDecoder();
	virtual ~ModelDecoder();

	virtual const std::string getExtensionType() const = 0;

	// throws ModelFormatDecoderExcep on failure
	virtual void streamModel(const std::string& filename, Indigo::Mesh& handler, float scale) = 0;
};
