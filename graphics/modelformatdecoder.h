/*=====================================================================
modelformatdecoder.h
--------------------
File created by ClassTemplate on Tue May 07 12:55:02 2002
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
#include "../utils/NameMap.h"
#include <string>
#include <vector>
namespace CS { class Model; }
namespace Indigo { class Mesh; }
class ModelDecoder;


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
	ModelFormatDecoder();
	~ModelFormatDecoder();

	void streamModel(const std::string& filename, Indigo::Mesh& handler, float scale); // throws ModelFormatDecoderExcep

	// Takes ownership of pointer.
	void registerModelDecoder(ModelDecoder* decoder);

private:
	NameMap<ModelDecoder*> decoders;
};
