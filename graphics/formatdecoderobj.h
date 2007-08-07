/*=====================================================================
formatdecoderobj.h
------------------
File created by ClassTemplate on Sat Apr 30 18:15:18 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __FORMATDECODEROBJ_H_666_
#define __FORMATDECODEROBJ_H_666_


#include "modeldecoder.h"
//#include "../simple3D/colour.h"
#include "../maths/vec3.h"
#include "../maths/coordframe.h"
namespace CS { class Model; }
namespace CS { class ModelPart; }
//#include "triangle.h"
//#include "vertex.h"
#include "modelformatdecoder.h"//just for the exception
#include <vector>

/*=====================================================================
FormatDecoderObj
----------------

=====================================================================*/
class FormatDecoderObj : public ModelDecoder
{
public:
	/*=====================================================================
	FormatDecoderObj
	----------------
	
	=====================================================================*/
	FormatDecoderObj();

	virtual ~FormatDecoderObj();


	virtual const std::string getExtensionType() const;


	//virtual void buildModel(const void* data, int datalen, const std::string& filename, 
	//	CS::Model& model_out) throw (ModelFormatDecoderExcep);

	virtual void streamModel(const std::string& filename, ModelLoadingStreamHandler& handler, 
		float scale);// throw (ModelFormatDecoderExcep);


private:
	//void readMatFile(const std::string& matfilename, CS::Model& model, NameMap<int>& namemap);

};



#endif //__FORMATDECODEROBJ_H_666_




