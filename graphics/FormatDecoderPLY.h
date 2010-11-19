/*=====================================================================
FormatDecoderPLY.h
------------------
File created by ClassTemplate on Sat Dec 02 04:33:29 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __FORMATDECODERPLY_H_666_
#define __FORMATDECODERPLY_H_666_




#include "modeldecoder.h"
#include "modelformatdecoder.h"//just for the exception


/*=====================================================================
FormatDecoderPLY
----------------

=====================================================================*/
class FormatDecoderPLY : public ModelDecoder
{
public:
	/*=====================================================================
	FormatDecoderPLY
	----------------
	
	=====================================================================*/
	FormatDecoderPLY();

	~FormatDecoderPLY();

	virtual const std::string getExtensionType() const { return "ply"; }

	virtual void streamModel(const std::string& filename, Indigo::IndigoMesh& handler, float scale);// throw (ModelFormatDecoderExcep);
};



#endif //__FORMATDECODERPLY_H_666_




