/*=====================================================================
modeldecoder.cpp
----------------
File created by ClassTemplate on Sun Nov 07 02:05:28 2004Code By Nicholas Chapman.
=====================================================================*/
#include "modeldecoder.h"


#include <fstream>
#include "../utils/fileutils.h"

ModelDecoder::ModelDecoder()
{
	
}


ModelDecoder::~ModelDecoder()
{
	
}



//throws ModelFormatDecoderExcep
void ModelDecoder::readEntireFile(const std::string& pathname, std::vector<unsigned char>& data)
{
	//------------------------------------------------------------------------
	//read model file from disk
	//------------------------------------------------------------------------
	/*std::ifstream modelfile(pathname.c_str(), std::ios::binary);

	if(!modelfile)
		throw ModelFormatDecoderExcep("could not open file '" + pathname + "' for reading.");

	FileUtils::readEntireFile(modelfile, data);
	
	if(data.empty())
		throw ModelFormatDecoderExcep("failed to read contents of '" + pathname + "'.");*/

	try
	{
		FileUtils::readEntireFile(pathname, data);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw ModelFormatDecoderExcep(e.what());
	}
}




