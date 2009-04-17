/*=====================================================================
modeldecoder.h
--------------
File created by ClassTemplate on Sun Nov 07 02:05:28 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MODELDECODER_H_666_
#define __MODELDECODER_H_666_


#include "modelformatdecoder.h"
#include <string>
class ModelLoadingStreamHandler;

/*=====================================================================
ModelDecoder
------------

=====================================================================*/
class ModelDecoder
{
public:
	/*=====================================================================
	ModelDecoder
	------------

	=====================================================================*/
	ModelDecoder();

	virtual ~ModelDecoder();


	virtual const std::string getExtensionType() const = 0;


	//virtual void buildModel(const void* data, int datalen, const std::string& filename,
	//	CS::Model& model_out) throw (ModelFormatDecoderExcep) = 0;


	//throws ModelFormatDecoderExcep
	virtual void streamModel(/*const void* data, int datalen, */const std::string& filename, ModelLoadingStreamHandler& handler, float scale) = 0;// throw (ModelFormatDecoderExcep) = 0;

protected:
	//throws ModelFormatDecoderExcep
	void readEntireFile(const std::string& filename, std::vector<unsigned char>& data);

};



#endif //__MODELDECODER_H_666_




