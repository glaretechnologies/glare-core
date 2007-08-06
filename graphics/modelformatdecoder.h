/*=====================================================================
modelformatdecoder.h
--------------------
File created by ClassTemplate on Tue May 07 12:55:02 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MODELFORMATDECODER_H_666_
#define __MODELFORMATDECODER_H_666_

#include "../utils/platform.h"
#include "../utils/namemap.h"
#include <string>
#include <vector>
namespace CS { class Model; }
class ModelDecoder;
class ModelLoadingStreamHandler;

class ModelFormatDecoderExcep
{
public:
	ModelFormatDecoderExcep(const std::string& s_) : s(s_) {}
	~ModelFormatDecoderExcep(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};


/*=====================================================================
ModelFormatDecoder
------------------

=====================================================================*/
class ModelFormatDecoder
{
public:
	/*=====================================================================
	ModelFormatDecoder
	------------------
	
	=====================================================================*/
	ModelFormatDecoder();

	~ModelFormatDecoder();

	//void buildModel(const void* data, int datalen, const std::string& filename, CS::Model& model_out) throw (ModelFormatDecoderExcep);

	//void streamModel(const void* data, int datalen, const std::string& filename, ModelLoadingStreamHandler& handler) throw (ModelFormatDecoderExcep);
	void streamModel(const std::string& filename, ModelLoadingStreamHandler& handler, float scale) throw (ModelFormatDecoderExcep);


	//takes ownership of pointer
	void registerModelDecoder(ModelDecoder* decoder);


private:
	//typedef DECODER_MAP_TYPE;
	//std::map<std::string, ModelDecoder*> decoders;
	NameMap<ModelDecoder*> decoders;
};



#endif //__MODELFORMATDECODER_H_666_




