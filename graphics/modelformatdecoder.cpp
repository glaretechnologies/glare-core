/*=====================================================================
modelformatdecoder.cpp
----------------------
File created by ClassTemplate on Tue May 07 12:55:02 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "modelformatdecoder.h"


#include "modeldecoder.h"
#include "../dll/include/IndigoMesh.h"
#include "../utils/StringUtils.h"


ModelFormatDecoder::ModelFormatDecoder()
{
}


ModelFormatDecoder::~ModelFormatDecoder()
{
	decoders.removeAndDeleteAll();
}


void ModelFormatDecoder::streamModel(const std::string& filename, Indigo::Mesh& handler, float scale)
{
	if(filename.length() < 4)
		throw ModelFormatDecoderExcep("filename too short: '" + filename + "'");

	try
	{
		ModelDecoder* decoder = decoders.getValue(::toUpperCase(::getExtension(filename)));
		decoder->streamModel(filename, handler, scale);
	}
	catch(NameMapExcep& )
	{
		throw ModelFormatDecoderExcep("unsupported model format: " + getExtension(filename));
	}

}


void ModelFormatDecoder::registerModelDecoder(ModelDecoder* decoder)
{
	assert(!decoders.isInserted(::toUpperCase(decoder->getExtensionType())));

	decoders.insert(::toUpperCase(decoder->getExtensionType()), decoder);
}
