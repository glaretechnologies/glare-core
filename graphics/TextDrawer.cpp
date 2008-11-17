/*=====================================================================
TextDrawer.cpp
--------------
File created by ClassTemplate on Tue Apr 29 21:08:21 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "TextDrawer.h"

#include "bitmap.h"
#include "image.h"
#include "PNGDecoder.h"
#include "imformatdecoder.h"
#include "../utils/fileutils.h"
#include "../utils/Parser.h"



TextDrawer::TextDrawer(const std::string& font_image_path, const std::string& font_widths_path)
{
	try
	{
		Bitmap font_bmp;
		PNGDecoder::decode(font_image_path, font_bmp); // Load png file

		font.setFromBitmap(font_bmp);

		char_widths.resize(256);

		std::string contents;
		FileUtils::readEntireFile(font_widths_path, contents);

		Parser parser(contents.c_str(), (unsigned int)contents.size());
		parser.advancePastLine();
		for(int i=0; i<256; ++i)
		{
			int x;
			parser.parseInt(x);
			if(x != i)
				throw TextDrawerExcep("Error while parsing char widths file.");
			if(!parser.parseChar('='))
				throw TextDrawerExcep("Error while parsing char widths file.");

			if(!parser.parseInt(x))
				throw TextDrawerExcep("Error while parsing char widths file.");

			parser.advancePastLine();
			char_widths[i] = x;
		}
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw TextDrawerExcep(e.what());
	}
	catch(ImFormatExcep& e)
	{
		throw TextDrawerExcep(e.what());
	}
}


TextDrawer::~TextDrawer()
{
	
}


void TextDrawer::drawText(const std::string& msg, Image& target, int target_x, int target_y) const
{
	const int GLYPH_W = 16;
	
	const int VERT_PADDING = 1;

	int dx = target_x;

	for(unsigned int i=0; i<msg.size(); ++i)
	{
		const int dy = target_y;

		const int char_w = char_widths[msg[i]];

		const int src_y = ((int)msg[i] / 16) * GLYPH_W + VERT_PADDING;
		const int src_x = ((int)msg[i] % 16) * GLYPH_W + (GLYPH_W - char_w)/2;

		font.blitToImage(src_x, src_y, src_x + char_w, src_y + GLYPH_W - (VERT_PADDING*2), target, dx, dy);

		dx += char_w;
	}
}




