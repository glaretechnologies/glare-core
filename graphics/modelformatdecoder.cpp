/*=====================================================================
modelformatdecoder.cpp
----------------------
File created by ClassTemplate on Tue May 07 12:55:02 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "modelformatdecoder.h"


//#include "../resourcehandle.h"
//#include "model.h"
#include "../utils/stringutils.h"
#include "modeldecoder.h"

/*#include "formatdecoder3ds.h"
#include "formatdecodermd2.h"
#include "formatdecodercml.h"
#include "formatdecoderjpg.h"
#include "formatdecodertxt.h"*/

#include "../public/IndigoMesh.h"

ModelFormatDecoder::ModelFormatDecoder()
{
	
}


ModelFormatDecoder::~ModelFormatDecoder()
{
	decoders.removeAndDeleteAll();
}



void ModelFormatDecoder::streamModel(const std::string& filename, Indigo::IndigoMesh& handler, float scale) //throw (ModelFormatDecoderExcep)
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


/*
void ModelFormatDecoder::streamModel(const void* data, int datalen, const std::string& filename, ModelLoadingStreamHandler& handler) throw (ModelFormatDecoderExcep)
{
	//model_out.setName(filename);

	if(filename.length() < 4)
		throw ModelFormatDecoderExcep("filename too short: '" + filename + "'");

	ModelDecoder* decoder = decoders.getValue(::toUpperCase(::getExtension(filename)));

	if(!decoder)
	{
		throw ModelFormatDecoderExcep("unsupported model format: " 
				+ getExtension(filename));
	}
	else
	{
		//decoder->buildModel(data, datalen, filename, model_out);
		decoder->streamModel(data, datalen, filename, handler);
	}
}
*/

#if 0
void ModelFormatDecoder::buildModel(const void* data, int datalen, 
									const std::string& filename, CS::Model& model_out) throw (ModelFormatDecoderExcep)
{

	model_out.setName(filename);


	if(filename.length() < 4)
		throw ModelFormatDecoderExcep("filename too short: '" + filename + "'");

	ModelDecoder* decoder = decoders.getValue(::toUpperCase(::getExtension(filename)));

	if(!decoder)
	{
		throw ModelFormatDecoderExcep("unsupported model format: " 
				+ getExtension(filename));
	}
	else
	{

		decoder->buildModel(data, datalen, filename, model_out);
	}


	//-----------------------------------------------------------------
	//sanity check model
	//-----------------------------------------------------------------
	for(unsigned int p=0; p<model_out.model_parts.size(); ++p)
		model_out.model_parts[p].sanityCheckTris();


	//try
	//{
	//-----------------------------------------------------------------
	//switch on model format
	//-----------------------------------------------------------------
	/*if(hasExtension(filename, "cml"))
	{
		FormatDecoderCML decoder;
		decoder.buildModel(data, datalen, filename, model_out);
	}
	else *//*if(hasExtension(filename, "3ds"))
	{
		FormatDecoder3ds decoder;
		decoder.buildModel(data, datalen, filename, model_out);
	}
	else if(hasExtension(filename, "md2"))
	{
		FormatDecoderMd2 decoder;
		decoder.buildModel(data, datalen, filename, model_out);
	}
	else if(hasExtension(filename, "jpg") || hasExtension(filename, "jpeg")
											|| hasExtension(filename, "tga"))
	{
		FormatDecoderJpg decoder;
		decoder.buildModel(data, datalen, filename, model_out);
	}
	else if(hasExtension(filename, "txt"))
	{
		FormatDecoderTxt decoder;
		decoder.buildModel(data, datalen, filename, model_out);
	}
	else
	{
		throw ModelFormatDecoderExcep("unsupported model format: " 
				+ getExtension(filename));
	}

	//-----------------------------------------------------------------
	//sanity check model
	//-----------------------------------------------------------------
	for(int p=0; p<model_out.model_parts.size(); ++p)
		model_out.model_parts[p].sanityCheckTris();*/

	//}
	//catch(ModelFormatDecoderExcep& excep)
	//{
	//}
}
#endif


void ModelFormatDecoder::registerModelDecoder(ModelDecoder* decoder)
{
	assert(!decoders.isInserted(::toUpperCase(decoder->getExtensionType())));

	decoders.insert(::toUpperCase(decoder->getExtensionType()), decoder);
}
