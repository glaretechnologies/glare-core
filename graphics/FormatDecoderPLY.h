/*=====================================================================
FormatDecoderPLY.h
------------------
File created by ClassTemplate on Sat Dec 02 04:33:29 2006
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "modeldecoder.h"
#include "modelformatdecoder.h"//just for the exception


/*=====================================================================
FormatDecoderPLY
----------------

=====================================================================*/
class FormatDecoderPLY : public ModelDecoder
{
public:
	FormatDecoderPLY();
	virtual ~FormatDecoderPLY();

	virtual const std::string getExtensionType() const { return "ply"; }

	virtual void streamModel(const std::string& filename, Indigo::Mesh& handler, float scale); 
};
