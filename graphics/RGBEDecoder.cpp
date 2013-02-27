/*=====================================================================
RGBEDecoder.cpp
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at Tue Feb 26 22:12:22 +0100 2013
=====================================================================*/
#include "RGBEDecoder.h"


#include "imformatdecoder.h"
#include "ImageMap.h"
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include "../utils/Parser.h"
#include "../utils/Exception.h"
#include "../indigo/globals.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#endif
#include "../hdr/rgbe.h"


RGBEHeaderData::RGBEHeaderData()
{
	exposure = 1.0;
	gamma = 1.0;
	width = -1;
	height = -1;
}


RGBEDecoder::RGBEDecoder()
{

}


RGBEDecoder::~RGBEDecoder()
{

}


// The header data should be followed by an empty line, then a line containing the resolution, then the data.
/*
[HEADER]
[EMPTY LINE]
[RESOLUTION]
[DATA]
*/
// Read the file to the resolution, making sure the header data was ended by an emtpy line, then break.
void RGBEDecoder::readHeaderString(FileHandle& file, std::string& s_out) // throws ImFormatExcep
{
	if(!file.getFile())
		throw ImFormatExcep("Invalid file");

	s_out = "";

	char buf[128];
	char* res = NULL;
	bool last_line_was_eoh = false;

	while(res = fgets(buf, sizeof(buf) / sizeof(buf[0]), file.getFile()))
	{
		if(res == NULL)
			throw ImFormatExcep("Reached end of file without finding end of header");

		s_out += buf;

		if(last_line_was_eoh)
		{
			if(buf[0] != '-' && buf[1] != 'Y')
				throw ImFormatExcep("Missing image size specifier");

			break;
		}

		if(buf[0] == '\n') // Newline that sperarates header from image size + data
			last_line_was_eoh = true;
	}
}


const RGBEHeaderData RGBEDecoder::parseHeaderString(const std::string& header) // throws ImFormatExcep
{
	Parser parser((const char*)&(header[0]), (unsigned int)header.size());

	RGBEHeaderData header_data;

	int linenum = 0;
	std::string token;
	while(!parser.eof())
	{
		linenum++;

		if(parser.current() == '-' && parser.nextIsNotEOF() && parser.next() == 'Y')
		{
			// Parse resolution, then break, as the resolution comes just before the data.
			parser.advance();
			parser.advance();

			parser.parseSpacesAndTabs();

			parser.parseInt(header_data.height);

			parser.parseSpacesAndTabs();

			if(parser.current() == '+' && parser.nextIsNotEOF() && parser.next() == 'X')
			{
				parser.advance();
				parser.advance();

				parser.parseSpacesAndTabs();

				parser.parseInt(header_data.width);
			}
			else
				throw ImFormatExcep("Failed to parse resolution");

			break;
		}

		parser.parseAlphaToken(token);

		if(token == "FORMAT")  // format
		{
			parser.parseSpacesAndTabs();

			if(parser.parseChar('='))
			{
				parser.parseSpacesAndTabs();
			
				parser.parseNonWSToken(header_data.format);

				if(header_data.format != "32-bit_rle_rgbe")
				{
					throw ImFormatExcep("Invalid format '" + header_data.format + "'");
				}
			}
			else
				throw ImFormatExcep("Failed to parse FORMAT");
		}
		else if(token == "EXPOSURE")  // exposure
		{
			parser.parseSpacesAndTabs();

			if(parser.parseChar('='))
			{
				parser.parseSpacesAndTabs();

				parser.parseFloat(header_data.exposure);
			}
			else
				throw ImFormatExcep("Failed to parse EXPONENT");
		}
		else if(token == "GAMMA")  // gamma
		{
			parser.parseSpacesAndTabs();

			if(parser.parseChar('='))
			{
				parser.parseSpacesAndTabs();

				parser.parseFloat(header_data.gamma);
			}
			else
				throw ImFormatExcep("Failed to parse GAMMA");
		}

		parser.advancePastLine();
	}

	if(header_data.width <= 0 || header_data.height <= 0)
		throw ImFormatExcep("Invalid width or height");

	if(header_data.format == "")
		throw ImFormatExcep("Format not found");

	return header_data;
}


// throws ImFormatExcep on failure
Reference<Map2D> RGBEDecoder::decode(const std::string& path)
{
	FileHandle rgbe_file;

	try
	{
		rgbe_file.open(path, "rb");
	}
	catch(Indigo::Exception& e)
	{
		throw ImFormatExcep("failed to open hdr file: " + e.what());
	}

	/*int width, height;
	rgbe_header_info info;
	int result = RGBE_ReadHeader(rgbe_file.getFile(), &width, &height, &info);

	if(result != RGBE_RETURN_SUCCESS)
		throw ImFormatExcep("failed to open hdr file: ");*/

	// We have our own RGBE header parser because the one from http://www.graphics.cornell.edu/online/formats/rgbe/ is rather bad.
	std::string header_string;
	readHeaderString(rgbe_file, header_string);

	RGBEHeaderData info = parseHeaderString(header_string);


	Reference<ImageMap<float, FloatComponentValueTraits> > image_map = new ImageMap<float, FloatComponentValueTraits>(info.width, info.height, 3);
	image_map->setGamma(1.0f);

	float* image_data = image_map->getData();

	for(size_t i = 0; i < info.height; ++i)
	{
		int result = RGBE_ReadPixels_RLE(rgbe_file.getFile(), &image_data[info.width * 3 * i], info.width, 1);

		if(result != RGBE_RETURN_SUCCESS)
			throw ImFormatExcep("failed to read .hdr file");
	}

	// if exposure != 1.0 go over all pixels an devide by exposure.
	if(!epsEqual<float>(info.exposure , 1.0f))
	{
		float recip_exp = 1 / info.exposure;

		for(size_t i = 0; i < image_map->getWidth() * image_map->getHeight() * 3; ++i)
			image_data[i] *= recip_exp;
	}

	return image_map;
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"


void RGBEDecoder::test()
{
	try
	{
		Reference<Map2D> im = RGBEDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/hdrs/brickweave.hdr");
		testAssert(im->getMapWidth() == 256);
		testAssert(im->getMapHeight() == 256);
		testAssert(im->getBytesPerPixel() == sizeof(float) * 3);

		// Test Unicode path
		const std::string euro = "\xE2\x82\xAC";
		Reference<Map2D> im2 = RGBEDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/hdrs/" + euro + ".hdr");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test that failure to load an image is handled gracefully.

	// Try with an invalid path
	try
	{
		RGBEDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/hdrs/NO_SUCH_FILE.gif");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with a JPG file
	try
	{
		RGBEDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/checker.jpg");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try invalid .hdr file missing FORMAT
	try
	{
		RGBEDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/hdrs/invalid_format.hdr");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try invalid .hdr file missing resolution
	try
	{
		RGBEDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/hdrs/invalid_res.hdr");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}
}


#endif // BUILD_TESTS
