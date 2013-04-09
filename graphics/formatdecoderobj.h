/*=====================================================================
formatdecoderobj.h
------------------
File created by ClassTemplate on Sat Apr 30 18:15:18 2005
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "modeldecoder.h"
#include "../maths/vec3.h"
#include "../maths/coordframe.h"
#include <vector>
namespace CS { class Model; }
namespace CS { class ModelPart; }


/*=====================================================================
FormatDecoderObj
----------------

=====================================================================*/
class FormatDecoderObj : public ModelDecoder
{
public:
	FormatDecoderObj();
	virtual ~FormatDecoderObj();

	virtual const std::string getExtensionType() const;

	virtual void streamModel(const std::string& filename, Indigo::Mesh& handler, float scale); // Throws ModelFormatDecoderExcep on failure.
};
