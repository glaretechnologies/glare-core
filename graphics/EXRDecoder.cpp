/*=====================================================================
EXRDecoder.cpp
--------------
File created by ClassTemplate on Fri Jul 11 02:36:44 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "EXRDecoder.h"


#include "../graphics/FPImageMap16.h"
#include <ImfRgbaFile.h>
#include <ImathBox.h>
#include "imformatdecoder.h"
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include <fstream>
#include <ImfStdIO.h>


EXRDecoder::EXRDecoder()
{
	
}


EXRDecoder::~EXRDecoder()
{
	
}


Reference<Map2D> EXRDecoder::decode(const std::string& pathname)
{
#ifdef OPENEXR_SUPPORT
	try
	{
		std::ifstream infile(FileUtils::convertUTF8ToFStreamPath(pathname).c_str(), std::ios::binary);

		Imf::StdIFStream exr_ifstream(infile, pathname.c_str());

		Imf::RgbaInputFile file(exr_ifstream);

		const Imath::Box2i dw = file.dataWindow();

		const unsigned int filewidth = (unsigned int)(dw.max.x - dw.min.x + 1);
		const unsigned int fileheight = (unsigned int)(dw.max.y - dw.min.y + 1);

		if(filewidth < 0 || fileheight < 0)
			throw ImFormatExcep("invalid image dimensions");

		FPImageMap16* image = new FPImageMap16(filewidth, fileheight);

		std::vector<Imf::Rgba> data(filewidth * fileheight);

		file.setFrameBuffer(
			&data[0], // base
			1, // x stride
			filewidth // y stride
			);
		file.readPixels(dw.min.y, dw.max.y);

		unsigned int c=0;
		for(unsigned int y=0; y<fileheight; ++y)
			for(unsigned int x=0; x<filewidth; ++x)
			{
				image->getData()[c++] = data[y*filewidth + x].r;
				image->getData()[c++] = data[y*filewidth + x].g;
				image->getData()[c++] = data[y*filewidth + x].b;
			}

		return Reference<Map2D>(image);
	}
	catch(const std::exception& e)
	{
		throw ImFormatExcep("Error reading EXR file: " + std::string(e.what()));
	}

#else //OPENEXR_SUPPORT

	throw ImFormatExcep("OPENEXR_SUPPORT disabled.");

#endif //OPENEXR_SUPPORT
}
