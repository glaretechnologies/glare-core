/*=====================================================================
Lib3dsFormatDecoder.h
---------------------
File created by ClassTemplate on Fri Jun 17 04:11:35 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __LIB3DSFORMATDECODER_H_666_
#define __LIB3DSFORMATDECODER_H_666_


#error


#include "../graphics/modeldecoder.h"
//#include "../simple3D/colour.h"
//#include "../graphics/colour3.h"
//#include "../maths/vec3.h"
//#include "../maths/coordframe.h"
//#include "triangle.h"
//#include "vertex.h"
#include "../graphics/modelformatdecoder.h"//just for the exception
//#include <vector>
namespace CS { class Model; }
namespace CS { class ModelPart; }


/*=====================================================================
Lib3dsFormatDecoder
-------------------

=====================================================================*/
class Lib3dsFormatDecoder : public ModelDecoder
{
public:
	/*=====================================================================
	Lib3dsFormatDecoder
	-------------------

	=====================================================================*/
	Lib3dsFormatDecoder();

	~Lib3dsFormatDecoder();


	virtual const std::string getExtensionType() const { return "3ds"; }


	//virtual void buildModel(const void* data, int datalen, const std::string& filename,
	//	CS::Model& model_out) throw (ModelFormatDecoderExcep);

	void setScale(float newscale){ scale = newscale; }//set b4 calling buildModel()

	virtual void streamModel(/*const void* data, int datalen, */const std::string& filename, Indigo::Mesh& handler, float scale);// throw (ModelFormatDecoderExcep);


private:
	float scale;
};



#endif //__LIB3DSFORMATDECODER_H_666_




