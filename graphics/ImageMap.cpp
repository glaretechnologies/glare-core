/*=====================================================================
ImageMap.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at Fri Mar 11 13:14:38 +0000 2011
=====================================================================*/
#include "ImageMap.h"


#include "../utils/OutStream.h"
#include "../utils/InStream.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"


// Explicit template instantiation
template void writeToStream(const ImageMap<float, FloatComponentValueTraits>& im, OutStream& stream);
template void readFromStream(InStream& stream, ImageMap<float, FloatComponentValueTraits>& image);


static const uint32 IMAGEMAP_SERIALISATION_VERSION = 1;


template <class V, class VTraits>
void writeToStream(const ImageMap<V, VTraits>& im, OutStream& stream)
{
	stream.writeUInt32(IMAGEMAP_SERIALISATION_VERSION);
	stream.writeUInt32((uint32)im.getWidth());
	stream.writeUInt32((uint32)im.getHeight());
	stream.writeUInt32((uint32)im.getN());
	stream.writeData(im.getData(), (size_t)im.getWidth() * (size_t)im.getHeight() * (size_t)im.getN() * sizeof(V));
}


template <class V, class VTraits>
void readFromStream(InStream& stream, ImageMap<V, VTraits>& image)
{
	const uint32 v = stream.readUInt32();
	if(v != IMAGEMAP_SERIALISATION_VERSION)
		throw Indigo::Exception("Unknown version " + toString(v) + ", expected " + toString(IMAGEMAP_SERIALISATION_VERSION) + ".");

	const uint32 w = stream.readUInt32();
	const uint32 h = stream.readUInt32();
	const uint32 N = stream.readUInt32();

	// TODO: handle max image size

	image.resize(w, h, N);
	stream.readData((void*)image.getData(), (size_t)w * (size_t)h * (size_t)N * sizeof(V));
}
