/*=====================================================================
TextRenderer.cpp
---------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "TextRenderer.h"


#include "../utils/Lock.h"
#include "../utils/UTF8Utils.h"
#include "../utils/ConPrint.h"
#include <freetype/freetype.h>


TextRenderer::TextRenderer()
{
	FT_Error error = FT_Init_FreeType(&library); // initialize library
	if(error != FT_Err_Ok)
		throw glare::Exception("FT_Init_FreeType failed");
}


TextRenderer::~TextRenderer()
{
	FT_Done_FreeType(library);
}


static void drawCharToBitmap(ImageMapUInt8& map,
	FT_Bitmap* bitmap,
	int start_dest_x,
	int start_dest_y,
	const Colour3f& col
)
{
	const int end_dest_x = start_dest_x + bitmap->width;
	const int end_dest_y = start_dest_y + bitmap->rows;

	const int w = (int)map.getWidth();
	const int h = (int)map.getHeight();
	assert(map.getN() >= 3);

	// For simplicity, we assume that `bitmap->pixel_mode' is `FT_PIXEL_MODE_GRAY' (i.e., not a bitmap font)
	assert(bitmap->pixel_mode == FT_PIXEL_MODE_GRAY);

	for(int destx = start_dest_x, srcx = 0; destx < end_dest_x; destx++, srcx++ )
	{
		for(int desty = start_dest_y, srcy = 0; desty < end_dest_y; desty++, srcy++ )
		{
			if( destx < 0  || desty < 0 ||
				destx >= w || desty >= h )
				continue;

			const uint8 v = bitmap->buffer[srcy * bitmap->width + srcx];

			uint8* pixel = map.getPixel(destx, desty);

			const Colour3f src_col(
				pixel[0] * (1.f / 255.f),
				pixel[1] * (1.f / 255.f),
				pixel[2] * (1.f / 255.f)
			);

			const Colour3f new_col = Maths::lerp(src_col, col, (float)v * (1.0f / 255.f));
			
			pixel[0] = (uint8)(new_col.r * 255.01f);
			pixel[1] = (uint8)(new_col.g * 255.01f);
			pixel[2] = (uint8)(new_col.b * 255.01f);
		}
	}
}


static std::string getFreeTypeErrorString(FT_Error err)
{
	const char* str = FT_Error_String(err);
	if(str)
		return std::string(str);
	else
		return "[Unknown]";
}


TextRendererFontFace::TextRendererFontFace(TextRendererRef renderer_, const std::string& font_file_path, int font_size_pixels_)
:	font_size_pixels(font_size_pixels_)
{
	renderer = renderer_;

	FT_Error error = FT_New_Face(renderer->library, font_file_path.c_str(), /*face index=*/0, &face); // Create face object
	if(error != FT_Err_Ok)
		throw glare::Exception("TextRendererFontFace: FT_New_Face failed: " + getFreeTypeErrorString(error));

	error = FT_Set_Pixel_Sizes(face, /*char width=*/font_size_pixels, /*char height=*/0);
	if(error != FT_Err_Ok)
		throw glare::Exception("FT_Set_Char_Size failed: " + getFreeTypeErrorString(error));
}


TextRendererFontFace::~TextRendererFontFace()
{
	FT_Done_Face(face);
}


void TextRendererFontFace::drawText(ImageMapUInt8& map, const std::string& text, int draw_x, int draw_y, const Colour3f& col)
{
	Lock lock(renderer->mutex);

	FT_GlyphSlot slot = face->glyph;

	// The pen position in 26.6 cartesian space coordinates
	FT_Vector pen;
	pen.x = 0;
	pen.y = 0;

	FT_Matrix matrix;
	matrix.xx = (FT_Fixed)(1.0 * 0x10000L);
	matrix.xy = (FT_Fixed)(0.0 * 0x10000L);
	matrix.yx = (FT_Fixed)(0.0 * 0x10000L);
	matrix.yy = (FT_Fixed)(1.0 * 0x10000L);

	for(size_t i=0; i<text.size();)
	{
		const size_t num_bytes = UTF8Utils::numBytesForChar(((const uint8*)text.data())[i]);
		if(i + num_bytes > text.size())
			throw glare::Exception("Invalid UTF-8 string.");

		const string_view substring(&text[i], text.size() - i);
		const uint32 code_point = UTF8Utils::codePointForUTF8CharString(substring);

		const FT_UInt glyph_index = FT_Get_Char_Index(face, code_point);

		FT_Set_Transform(face, &matrix, &pen); // set transformation

		FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER); // load glyph image into the slot (erase previous one)
		if(error == 0) // If no errors:
		{
			// now, draw to our target surface (convert position)
			drawCharToBitmap(map, &slot->bitmap, /*start_dest_x=*/draw_x + slot->bitmap_left, draw_y - slot->bitmap_top, col);

			// increment pen position
			pen.x += slot->advance.x;
			pen.y += slot->advance.y;
		}
		else
		{
#ifndef NDEBUG
			conPrint("Warning: FT_Load_Glyph failed: " + getFreeTypeErrorString(error));
#endif
		}

		i += num_bytes;
	}
}


TextRendererFontFace::SizeInfo TextRendererFontFace::getTextSize(const std::string& text)
{
	FT_GlyphSlot slot = face->glyph;

	// the pen position in 26.6 cartesian space coordinates
	FT_Vector pen;
	pen.x = 0;
	pen.y = 0;

	Vec2i min_bounds(0, 0);
	Vec2i max_bounds(0, 0);

	for(size_t i=0; i<text.size();)
	{
		const size_t num_bytes = UTF8Utils::numBytesForChar(((const uint8*)text.data())[i]);
		if(i + num_bytes > text.size())
			throw glare::Exception("Invalid UTF-8 string.");

		const string_view substring(&text[i], text.size() - i);
		const uint32 code_point = UTF8Utils::codePointForUTF8CharString(substring);

		const FT_UInt glyph_index = FT_Get_Char_Index(face, code_point);

		FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER); // load glyph image into the slot (erase previous one)
		if(error == 0) // If no errors:
		{
			// See https://freetype.org/freetype2/docs/tutorial/step2.html
			const Vec2i p(pen.x / 64, pen.y / 64);
			Vec2i char_min_bounds(
				p.x + face->glyph->metrics.horiBearingX                                 / 64,
				p.y + (face->glyph->metrics.horiBearingY - face->glyph->metrics.height) / 64
			);
			Vec2i char_max_bounds(
				p.x + (face->glyph->metrics.horiBearingX + face->glyph->metrics.width) / 64,
				p.y + face->glyph->metrics.horiBearingY                                / 64
			);
			min_bounds.x = myMin(min_bounds.x, char_min_bounds.x);
			min_bounds.y = myMin(min_bounds.y, char_min_bounds.y);
			max_bounds.x = myMax(max_bounds.x, char_max_bounds.x);
			max_bounds.y = myMax(max_bounds.y, char_max_bounds.y);

			// increment pen position
			pen.x += slot->advance.x;
			pen.y += slot->advance.y;
		}

		i += num_bytes;
	}

	SizeInfo size_info;
	size_info.size = max_bounds - min_bounds;
	size_info.min_bounds = min_bounds;
	size_info.max_bounds = max_bounds;
	return size_info;
}


int TextRendererFontFace::getFaceAscender()
{
	return face->ascender / 64;
}







#if BUILD_TESTS


#include <graphics/PNGDecoder.h>
#include <utils/TaskManager.h>
#include <utils/StringUtils.h>
#include <utils/TestUtils.h>


class DrawTextTestTask : public glare::Task
{
public:
	DrawTextTestTask(TextRendererFontFaceRef font_) : font(font_) {}

	virtual void run(size_t thread_index)
	{
		conPrint("DrawTextTestTask running...");

		ImageMapUInt8Ref map = new ImageMapUInt8(500, 500, 3);
		map->zero();

		const std::string text = "hello";
		const int NUM_ITERS = 100;
		for(int i=0; i<NUM_ITERS; ++i)
		{
			font->drawText(*map, text, 10, 250, Colour3f(1,1,1));

			// if((i % (NUM_ITERS / 4)) == 0)
			// 	conPrint("thread " + toString(thread_index) + ": " + toString(i));
		}

		conPrint("DrawTextTestTask done.");
	}

	TextRendererFontFaceRef font;
};


void TextRenderer::test()
{
	conPrint("TextRenderer::test()");

	const bool WRITE_IMAGES = false;

	//-------------------------------------- Test Unicode rendering with a TTF file --------------------------------------
	try
	{
		ImageMapUInt8Ref map = new ImageMapUInt8(500, 500, 3);
		for(int y=0; y<500; ++y)
		for(int x=0; x<500; ++x)
		{
			map->getPixel(x, y)[0] = x % 255;
			map->getPixel(x, y)[1] = y % 255;
			map->getPixel(x, y)[2] = 100;
		}

		TextRendererRef text_renderer = new TextRenderer();
		TextRendererFontFaceRef font = new TextRendererFontFace(text_renderer, TestUtils::getTestReposDir() + "/testfiles/fonts/TruenoLight-E2pg.otf", 50); // "C:\\Windows\\Fonts\\segoeui.ttf"

		const std::string euro = "\xE2\x82\xAC";
		const std::string gamma = "\xCE\x93"; // Greek capital letter gamma, U+393, http://unicodelookup.com/#gamma/1, 

		const std::string text = euro + "A" + gamma;

		TextRendererFontFace::SizeInfo size_info = font->getTextSize(text);

		font->drawText(*map, text, 10, 250, Colour3f(1,1,1));

		if(WRITE_IMAGES)
			PNGDecoder::write(*map, "text.png");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}


	//-------------------------------------- Test loading and rendering with a TTF file --------------------------------------
	try
	{
		ImageMapUInt8Ref map = new ImageMapUInt8(1000, 500, 3);
		map->zero();

		TextRendererRef text_renderer = new TextRenderer();
		TextRendererFontFaceRef font = new TextRendererFontFace(text_renderer, TestUtils::getTestReposDir() + "/testfiles/fonts/Freedom-10eM.ttf", 30);

		const std::string text = "The quick brown fox jumps over the lazy dog. 1234567890";
		TextRendererFontFace::SizeInfo size_info = font->getTextSize(text);
		font->drawText(*map, text, 10, 250, Colour3f(1,1,1));

		if(WRITE_IMAGES)
			PNGDecoder::write(*map, "text_ttf.png");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	//-------------------------------------- Test loading and rendering with an OTF file --------------------------------------
	try
	{
		ImageMapUInt8Ref map = new ImageMapUInt8(1000, 500, 3);
		map->zero();

		TextRendererRef text_renderer = new TextRenderer();
		TextRendererFontFaceRef font = new TextRendererFontFace(text_renderer, TestUtils::getTestReposDir() + "/testfiles/fonts/TruenoLight-E2pg.otf", 30);

		const std::string text = "The quick brown fox jumps over the lazy dog. 1234567890";
		TextRendererFontFace::SizeInfo size_info = font->getTextSize(text);
		font->drawText(*map, text, 10, 250, Colour3f(1,1,1));

		if(WRITE_IMAGES)
			PNGDecoder::write(*map, "text_otf.png");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}


	//-------------------------------------- Test concurrent rendering --------------------------------------
	// Just make sure we don't have any crashes
	{
		TextRendererRef text_renderer = new TextRenderer();
		TextRendererFontFaceRef font = new TextRendererFontFace(text_renderer, TestUtils::getTestReposDir() + "/testfiles/fonts/Freedom-10eM.ttf", 50);

		glare::TaskManager task_manager;

		glare::TaskGroupRef group = new glare::TaskGroup();

		for(int i=0; i<16; ++i)
			group->tasks.push_back(new DrawTextTestTask(font));

		task_manager.runTaskGroup(group);
	}

	conPrint("TextRenderer::test() done");
}


#endif // BUILD_TESTS
