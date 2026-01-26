/*=====================================================================
TextRenderer.cpp
---------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "TextRenderer.h"


#include "../utils/Lock.h"
#include "../utils/UTF8Utils.h"
#include "../utils/ConPrint.h"
#include "../utils/RuntimeCheck.h"
#include "../utils/Timer.h"
#include <freetype/freetype.h>
#include <tracy/Tracy.hpp>


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


static std::string getFreeTypeErrorString(FT_Error err)
{
	const char* str = FT_Error_String(err);
	if(str)
		return std::string(str);
	else
		return "[Unknown]";
}


static inline bool codePointIsEmoji(uint32 code_point)
{
	return code_point >= 0x1F32; // TODO: improve this.  Emojis are not one continuous block, see https://en.wikipedia.org/wiki/Emoji#In_Unicode
}


static void drawCharToBitmap(ImageMapUInt8& map,
	FT_Bitmap* bitmap,
	int start_dest_x,
	int start_dest_y,
	const Colour3f& col
)
{
	ZoneScoped; // Tracy profiler
	const int end_dest_x = start_dest_x + bitmap->width;
	const int end_dest_y = start_dest_y + bitmap->rows;

	const int w = (int)map.getWidth();
	const int h = (int)map.getHeight();
	const int N = (int)map.getN();
	runtimeCheck(map.getN() == 1 || map.getN() >= 3);

	runtimeCheck(bitmap->pitch >= 0); // Pitch can apparently be negative sometimes

	if(bitmap->pixel_mode == FT_PIXEL_MODE_GRAY)
	{
		for(int destx = start_dest_x, srcx = 0; destx < end_dest_x; destx++, srcx++ )
		{
			for(int desty = start_dest_y, srcy = 0; desty < end_dest_y; desty++, srcy++ )
			{
				if( destx < 0  || desty < 0 ||
					destx >= w || desty >= h )
					continue;

				const uint8 v = bitmap->buffer[srcy * bitmap->pitch + srcx];

				uint8* pixel = map.getPixel(destx, desty);

				if(N == 1)
				{
					// Get blended result of existing colour and 'col', where blend factor = font greyscale value.
					const float src_col = pixel[0] * (1.f / 255.f);

					const float new_col = Maths::lerp(src_col, col.r, (float)v * (1.0f / 255.f));
			
					pixel[0] = (uint8)(new_col * 255.01f);
				}
				else if(N == 3)
				{
					// Get blended result of existing colour and 'col', where blend factor = font greyscale value.
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
				else
				{
					// For an imagemap with alpha, just assume we making a font cache, in which case the colour can just be white and we can ignore the existing map colour
					pixel[0] = 255;
					pixel[1] = 255;
					pixel[2] = 255;
					pixel[3] = v;
				}
			}
		}
	}
	else if(bitmap->pixel_mode == FT_PIXEL_MODE_BGRA)
	{
		for(int destx = start_dest_x, srcx = 0; destx < end_dest_x; destx++, srcx++ )
		{
			for(int desty = start_dest_y, srcy = 0; desty < end_dest_y; desty++, srcy++ )
			{
				if( destx < 0  || desty < 0 ||
					destx >= w || desty >= h )
					continue;

				const uint8* src = &bitmap->buffer[srcy * bitmap->pitch + srcx * 4];

				uint8* pixel = map.getPixel(destx, desty);

				if(N == 1)
				{
					// Get blended result of existing colour and colour from font, where blend factor = font alpha.
					const float src_col      = pixel[0] * (1.f / 255.f);
					const float font_rgb_col = src[2]   * (1.f / 255.f); // Convert from BGRA.  NOTE: just using red channel
					const float font_alpha_f = (float)src[3] * (1.0f / 255.f);
					const float new_col = Maths::lerp(src_col, font_rgb_col, font_alpha_f);
			
					pixel[0] = (uint8)(new_col * 255.01f);
				}
				else if(N == 3)
				{
					// Get blended result of existing colour and colour from font, where blend factor = font alpha.
					const Colour3f src_col(
						pixel[0] * (1.f / 255.f),
						pixel[1] * (1.f / 255.f),
						pixel[2] * (1.f / 255.f)
					);

					const Colour3f font_rgb_col(
						src[2] * (1.f / 255.f), // Convert from BGRA
						src[1] * (1.f / 255.f),
						src[0] * (1.f / 255.f)
					);

					const float font_alpha_f = (float)src[3] * (1.0f / 255.f);
					const Colour3f new_col = Maths::lerp(src_col, font_rgb_col, font_alpha_f);
			
					pixel[0] = (uint8)(new_col.r * 255.01f);
					pixel[1] = (uint8)(new_col.g * 255.01f);
					pixel[2] = (uint8)(new_col.b * 255.01f);
				}
				else
				{
					// For an imagemap with alpha, just assume we making a font cache, in which case we can ignore the existing map colour
					pixel[0] = src[2];
					pixel[1] = src[1];
					pixel[2] = src[0];
					pixel[3] = src[3];
				}
			}
		}
	}
	else
	{
		// Unhandled bitmap pixel mode
		assert(0);
	}
}


static const float LINE_HEIGHT_FACTOR = 1.5f;


TextRenderer::SizeInfo TextRenderer::getTextSize(const string_view text, TextRendererFontFace* font, TextRendererFontFace* emoji_font)
{
	ZoneScoped; // Tracy profiler
	runtimeCheck(font != nullptr);

	Lock lock(font->renderer->mutex);

	// The pen position in 26.6 coordinates
	FT_Vector pen;
	pen.x = 0;
	pen.y = 0;

	Vec2i min_bounds(0, 0);
	Vec2i max_bounds(0, 0);
	int min_bitmap_left = 0;
	int max_bitmap_top = 0;

	FT_Set_Transform(font->face,       /*matrix=*/NULL, /*translation=*/NULL); // Set transformation.  Use null matrix to get the identity matrix
	if(emoji_font)
		FT_Set_Transform(emoji_font->face, /*matrix=*/NULL, /*translation=*/NULL); // Set transformation.  Use null matrix to get the identity matrix

	for(size_t i=0; i<text.size();)
	{
		const size_t num_bytes = UTF8Utils::numBytesForChar(((const uint8*)text.data())[i]);
		if(i + num_bytes > text.size())
			throw glare::Exception("Invalid UTF-8 string.");

		if(text[i] == '\n')
		{
			pen.x = 0;
			pen.y -= (int)(font->font_size_pixels * LINE_HEIGHT_FACTOR * 64); // Move pen position down a line.
		}
		else
		{
			const string_view substring(&text[i], text.size() - i);
			const uint32 code_point = UTF8Utils::codePointForUTF8CharString(substring);

			TextRendererFontFace* use_font = (emoji_font && codePointIsEmoji(code_point)) ? emoji_font : font;
			struct FT_FaceRec_* face = use_font->face;

			FT_GlyphSlot slot = face->glyph;
			FT_UInt glyph_index = FT_Get_Char_Index(face, code_point);

			FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER); // load glyph image into the slot (erase previous one)
			use_font->cur_loaded_glyph_index = glyph_index;
			if(error == 0) // If no errors:
			{
				// See https://freetype.org/freetype2/docs/tutorial/step2.html
				const Vec2i p(pen.x / 64, pen.y / 64);
				const Vec2i char_min_bounds(
					p.x + face->glyph->metrics.horiBearingX                                 / 64,
					p.y + (face->glyph->metrics.horiBearingY - face->glyph->metrics.height) / 64
				);
				const Vec2i char_max_bounds(
					p.x + (face->glyph->metrics.horiBearingX + face->glyph->metrics.width) / 64,
					p.y + face->glyph->metrics.horiBearingY                                / 64
				);
				min_bounds.x = myMin(min_bounds.x, char_min_bounds.x);
				min_bounds.y = myMin(min_bounds.y, char_min_bounds.y);
				max_bounds.x = myMax(max_bounds.x, char_max_bounds.x);
				max_bounds.y = myMax(max_bounds.y, char_max_bounds.y);

				min_bitmap_left = myMin(min_bitmap_left, face->glyph->bitmap_left);
				max_bitmap_top = myMax(max_bitmap_top, face->glyph->bitmap_top);

				// increment pen position
				pen.x += slot->advance.x;
				pen.y += slot->advance.y;
			}
		}

		i += num_bytes;
	}

	SizeInfo size_info;
	size_info.min_bounds = min_bounds;
	size_info.max_bounds = max_bounds;
	size_info.hori_advance = pen.x / 64.f; // Pen coords are in 26.6 fixed point coords, so divide by 64 to convert to float.;
	size_info.bitmap_left = min_bitmap_left;
	size_info.bitmap_top = max_bitmap_top;
	return size_info;
}


// Draw text at (x, y).
// The y coordinate gives the position of the text baseline.
// Col is used if the font is greyscale.  If the font is a colour font (e.g. Emoji), the font colour is used.
// Throws glare::Exception on failure, for example on invalid UTF-8 string.
void TextRenderer::drawText(ImageMapUInt8& map, const string_view text, int draw_x, int draw_y, const Colour3f& col, bool render_SDF, TextRendererFontFace* font, TextRendererFontFace* emoji_font)
{
	ZoneScoped; // Tracy profiler
	runtimeCheck(font != nullptr);

	Lock lock(font->renderer->mutex);

	// The pen position in 26.6 fixed-point coordinates
	FT_Vector pen;
	pen.x = 0;
	pen.y = 0;

	for(size_t i=0; i<text.size();)
	{
		const size_t num_bytes = UTF8Utils::numBytesForChar(((const uint8*)text.data())[i]);
		if(i + num_bytes > text.size())
			throw glare::Exception("Invalid UTF-8 string.");

		if(text[i] == '\n')
		{
			pen.x = 0;
			draw_y += (int)(font->font_size_pixels * LINE_HEIGHT_FACTOR);
		}
		else
		{
			const string_view substring(&text[i], text.size() - i);
			const uint32 code_point = UTF8Utils::codePointForUTF8CharString(substring);

			TextRendererFontFace* use_font = (emoji_font && codePointIsEmoji(code_point)) ? emoji_font : font;
			struct FT_FaceRec_* face = use_font->face;

			FT_GlyphSlot slot = face->glyph;
			const FT_UInt glyph_index = FT_Get_Char_Index(face, code_point);

			FT_Set_Transform(face, /*matrix=*/NULL, &pen); // Set transformation.  Use null matrix to get the identity matrix

			// Note that we can't use cur_loaded_glyph_index as we probably have a non-identity transform
			FT_Error load_error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_COLOR); // load glyph image into the slot (erase previous one)
			if(load_error == 0) // If no errors:
			{
				FT_Error render_error = FT_Render_Glyph(slot, render_SDF ? FT_RENDER_MODE_SDF : FT_RENDER_MODE_NORMAL);
				if(render_error == 0) // If no errors:
				{
					// now, draw to our target surface (convert position)
					drawCharToBitmap(map, &slot->bitmap, /*start_dest_x=*/draw_x + slot->bitmap_left, draw_y - slot->bitmap_top, col);
				}
				else
				{
#ifndef NDEBUG
					conPrint("Warning: FT_Render_Glyph failed: " + getFreeTypeErrorString(render_error));
#endif
				}

				// increment pen position
				pen.x += slot->advance.x;
				pen.y += slot->advance.y;
			}
			else
			{
#ifndef NDEBUG
				conPrint("Warning: FT_Load_Glyph failed: " + getFreeTypeErrorString(load_error));
#endif
			}

			use_font->cur_loaded_glyph_index = std::numeric_limits<FT_UInt>::max(); // The loaded glyph probably doesn't have the identity transformation.
		}

		i += num_bytes;
	}
}



TextRendererFontFaceSizeSet::TextRendererFontFaceSizeSet(TextRendererRef renderer_, const std::string& font_file_path_)
{
	renderer = renderer_;
	font_file_path = font_file_path_;
}


TextRendererFontFaceRef TextRendererFontFaceSizeSet::getFontFaceForSize(int font_size_pixels)
{
	Lock lock(renderer->mutex);

	auto res = fonts_for_size.find(font_size_pixels);
	if(res == fonts_for_size.end())
	{
		//Timer timer;
		TextRendererFontFaceRef font = new TextRendererFontFace(renderer, font_file_path, font_size_pixels);
		//conPrint("Creating font took " + timer.elapsedStringMSWIthNSigFigs(4));
		fonts_for_size[font_size_pixels] = font;
		return font;
	}
	else
		return res->second;
}


TextRendererFontFace::TextRendererFontFace(TextRendererRef renderer_, const std::string& font_file_path, int font_size_pixels_)
:	font_size_pixels(font_size_pixels_),
	cur_loaded_glyph_index(std::numeric_limits<FT_UInt>::max())
{
	ZoneScoped; // Tracy profiler

	renderer = renderer_;

	FT_Error error = FT_New_Face(renderer->library, font_file_path.c_str(), /*face index=*/0, &face); // Create face object
	if(error != FT_Err_Ok)
		throw glare::Exception("TextRendererFontFace: FT_New_Face failed for font '" + 
			font_file_path + "': " + getFreeTypeErrorString(error));

	// FT_Set_Pixel_Sizes doesn't give accurate sizes, something to do with FreeType getting confused with resolutions.  See https://stackoverflow.com/questions/60061441/freetype-correct-size-rendering
	//error = FT_Set_Pixel_Sizes(face, /*char width=*/font_size_pixels, /*char height=*/0);

	// Use FT_Set_Char_Size with resolution = 96 ppi which seems to give correct results.
	error = FT_Set_Char_Size(
		face, // handle to face object
		0,    // char_width in 1/64th of points
		font_size_pixels * 64,   // char_height in 1/64th of points
		96, // horizontal device resolution
		96  // vertical device resolution
	);

	if(error != FT_Err_Ok)
		throw glare::Exception("FT_Set_Char_Size failed: " + getFreeTypeErrorString(error));
}


TextRendererFontFace::~TextRendererFontFace()
{
	FT_Done_Face(face);
}


void TextRendererFontFace::drawGlyph(ImageMapUInt8& map, const string_view char_text, int draw_x, int draw_y, const Colour3f& col, bool render_SDF)
{
	ZoneScoped; // Tracy profiler

	Lock lock(renderer->mutex);

	FT_GlyphSlot slot = face->glyph;

	const uint32 code_point = UTF8Utils::codePointForUTF8CharString(char_text);
	const FT_UInt glyph_index = FT_Get_Char_Index(face, code_point);

	FT_Set_Transform(face, /*matrix=*/NULL, /*translation=*/NULL); // Set transformation.  Use null matrix to get the identity matrix,
	// and null translation for the zero vector.

	if(this->cur_loaded_glyph_index != glyph_index)
	{
		FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_COLOR); // load glyph image into the slot (erase previous one)
		if(error != 0)
			throw glare::Exception("FT_Render_Glyph failed: " + getFreeTypeErrorString(error));
		this->cur_loaded_glyph_index = glyph_index;
	}

	FT_Error error = FT_Render_Glyph(slot, render_SDF ? FT_RENDER_MODE_SDF : FT_RENDER_MODE_NORMAL);
	if(error != 0)
		throw glare::Exception("FT_Render_Glyph failed: " + getFreeTypeErrorString(error));
	
	// Now, draw to our target surface (convert position)
	drawCharToBitmap(map, &slot->bitmap, /*start_dest_x=*/draw_x + slot->bitmap_left, draw_y - slot->bitmap_top, col);
}


void TextRendererFontFace::drawText(ImageMapUInt8& map, const string_view text, int draw_x, int draw_y, const Colour3f& col, bool render_SDF)
{
	this->renderer->drawText(map, text, draw_x, draw_y, col, render_SDF, /*font=*/this, /*emoji_font=*/nullptr);
}


// Get the size information for a single character glyph.
TextRenderer::SizeInfo TextRendererFontFace::getGlyphSize(const string_view text, bool render_SDF)
{
	ZoneScoped; // Tracy profiler

	Lock lock(renderer->mutex);

	FT_GlyphSlot slot = face->glyph;

	if(text.empty())
		throw glare::Exception("Empty string.");

	const uint32 code_point = UTF8Utils::codePointForUTF8CharString(text);

	const FT_UInt glyph_index = FT_Get_Char_Index(face, code_point);

	FT_Set_Transform(face, /*matrix=*/NULL, /*translation=*/NULL); // Set transformation.  Use null matrix to get the identity matrix,
	// and null translation for the zero vector.

	// NOTE: could try FT_LOAD_BITMAP_METRICS_ONLY or similar to just get glyph metrics without doing actual rendering.  Don't think it will work for SDF rendering though.
	FT_Error error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT | FT_LOAD_COLOR); // load glyph image into the slot (erase previous one)
	if(error != 0)
		throw glare::Exception("Error calling FT_Load_Glyph: " + getFreeTypeErrorString(error));
	this->cur_loaded_glyph_index = glyph_index;

	error = FT_Render_Glyph(slot, render_SDF ? FT_RENDER_MODE_SDF : FT_RENDER_MODE_NORMAL);
	if(error != 0)
		throw glare::Exception("Error calling FT_Render_Glyph: " + getFreeTypeErrorString(error));

	// See https://freetype.org/freetype2/docs/tutorial/step2.html
	const Vec2i char_min_bounds(
		face->glyph->metrics.horiBearingX                                 / 64,
		(face->glyph->metrics.horiBearingY - face->glyph->metrics.height) / 64
	);
	const Vec2i char_max_bounds(
		(face->glyph->metrics.horiBearingX + face->glyph->metrics.width) / 64,
		face->glyph->metrics.horiBearingY                                / 64
	);

	TextRenderer::SizeInfo size_info;
	size_info.min_bounds = char_min_bounds;
	size_info.max_bounds = char_max_bounds;
	size_info.hori_advance = slot->advance.x / 64.f; // Pen coords are in 26.6 fixed point coords, so divide by 64 to convert to float.
		
	size_info.bitmap_left = face->glyph->bitmap_left;
	size_info.bitmap_top  = face->glyph->bitmap_top;
	size_info.bitmap_width  = face->glyph->bitmap.width;
	size_info.bitmap_height = face->glyph->bitmap.rows;

	return size_info;
}


// Get the size information for a string with multiple characters/glyphs
// Assumes not doing SDF rendering
TextRenderer::SizeInfo TextRendererFontFace::getTextSize(const string_view text)
{
	return this->renderer->getTextSize(text, /*font=*/this, /*emoji_font=*/nullptr);
}


float TextRendererFontFace::getFaceAscender()
{
	return font_size_pixels * (float)face->ascender / (float)face->units_per_EM;
}







#if BUILD_TESTS


#include "Drawing.h"
#include "PNGDecoder.h"
#include <utils/TaskManager.h>
#include <utils/StringUtils.h>
#include <utils/TestUtils.h>
#include <utils/PlatformUtils.h>


class DrawTextTestTask : public glare::Task
{
public:
	DrawTextTestTask(TextRendererRef text_renderer_, TextRendererFontFaceRef font_) : text_renderer(text_renderer_), font(font_) {}

	virtual void run(size_t /*thread_index*/)
	{
		conPrint("DrawTextTestTask running...");

		ImageMapUInt8Ref map = new ImageMapUInt8(500, 500, 3);
		map->zero();

		const std::string text = "hello";
		const int NUM_ITERS = 100;
		for(int i=0; i<NUM_ITERS; ++i)
		{
			text_renderer->drawText(*map, text, 10, 250, Colour3f(1,1,1), /*render_SDF=*/false, font.ptr(), /*emoji font=*/nullptr);

			text_renderer->drawText(*map, text, 10, 250, Colour3f(1,1,1), /*render_SDF=*/true, font.ptr(), /*emoji font=*/nullptr);

			// if((i % (NUM_ITERS / 4)) == 0)
			// 	conPrint("thread " + toString(thread_index) + ": " + toString(i));
		}

		conPrint("DrawTextTestTask done.");
	}

	TextRendererRef text_renderer;
	TextRendererFontFaceRef font;
};


void TextRenderer::test()
{
	conPrint("TextRenderer::test()");

	const bool WRITE_IMAGES = false;

	//-------------------------------------- Test getTextSize and drawText with newlines --------------------------------------
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

		const std::string text = "aA\nbB\ncC";

		TextRenderer::SizeInfo size_info = text_renderer->getTextSize(text, font.ptr(), /*emoji_font=*/nullptr);

		// Draw glyph bounds so can check is correct visually.
		const int draw_x = 10;
		const int draw_y = 250;
		const uint8 border_col[] = {255, 0, 0};
		//const int rect_top_y = draw_y - size_info.glyphSize().y;
		//Drawing::drawRect(*map, draw_x, rect_top_y, size_info.glyphSize().x, size_info.glyphSize().y, border_col);

		// Note that the size_info.max_bounds is the top right of the bounding rectangle, so we need to subtract it as Drawing::drawRect uses y-down coords.

		Drawing::drawRect(*map, draw_x + size_info.min_bounds.x, draw_y - size_info.max_bounds.y, size_info.glyphSize().x, size_info.glyphSize().y, border_col);

		text_renderer->drawText(*map, text, 10, 250, Colour3f(1,1,1), /*render_SDF=*/false, font.ptr(), /*emoji_font=*/nullptr);

		if(WRITE_IMAGES)
			PNGDecoder::write(*map, "text_with_newlines.png");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}



	// Test mixed non-emoji and emoji
#ifdef _WIN32
	try
	{
		ImageMapUInt8Ref map = new ImageMapUInt8(1000, 500, 4);
		map->set(0);

		TextRendererRef text_renderer = new TextRenderer();
		//TextRendererFontFaceSizeSetRef fonts       = new TextRendererFontFaceSizeSet(text_renderer, PlatformUtils::getFontsDirPath() + "/Segoeui.ttf");
		//TextRendererFontFaceSizeSetRef emoji_fonts = new TextRendererFontFaceSizeSet(text_renderer, PlatformUtils::getFontsDirPath() + "/Seguiemj.ttf");
		TextRendererFontFaceSizeSetRef fonts       = new TextRendererFontFaceSizeSet(text_renderer, TestUtils::getTestReposDir() + "/testfiles/fonts/TruenoLight-E2pg.otf"); // "C:\\Windows\\Fonts\\segoeui.ttf"
		TextRendererFontFaceSizeSetRef emoji_fonts = fonts;

		TextRendererFontFaceRef font       = fonts      ->getFontFaceForSize(30);
		TextRendererFontFaceRef emoji_font = emoji_fonts->getFontFaceForSize(30);

		const std::string text = "abc\xF0\x9F\xA4\x96" "def";  // \xF0\x9F\xA4\x96 is Unicode char with codepoint U+1F916 - "Robot Face".
		
		TextRenderer::SizeInfo size_info = text_renderer->getTextSize(text, font.ptr(), emoji_font.ptr());

		text_renderer->drawText(*map, text, -size_info.bitmap_left, size_info.bitmap_top, Colour3f(1,1,1), /*render_SDF=*/false, font.ptr(), emoji_font.ptr());

		if(WRITE_IMAGES)
			PNGDecoder::write(*map, "plain text and emoji drawText mix.png");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
#endif


	//-------------------------------------- Test drawText with a repeated letter. --------------------------------------
	// There was a bug with this where the second letter wouldn't draw due to use of cur_loaded_glyph_index.
#ifdef _WIN32
	try
	{
		ImageMapUInt8Ref map = new ImageMapUInt8(1000, 500, 4);
		map->set(0);

		TextRendererRef text_renderer = new TextRenderer();
		TextRendererFontFaceRef font = new TextRendererFontFace(text_renderer, PlatformUtils::getFontsDirPath() + "/Segoeui.ttf", 30);

		const std::string text = "abbc";
		TextRenderer::SizeInfo size_info = text_renderer->getTextSize(text, font.ptr(), /*emoji_font=*/nullptr);
		text_renderer->drawText(*map, text, -size_info.bitmap_left, size_info.bitmap_top, Colour3f(1,1,1), /*render_SDF=*/false, font.ptr(), /*emoji_font=*/nullptr);

		if(WRITE_IMAGES)
			PNGDecoder::write(*map, "repeating letter test.png");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
#endif
	

	//-------------------------------------- Test rendering with a TTF file and SDF rendering --------------------------------------
#ifdef _WIN32
	try
	{
		ImageMapUInt8Ref map = new ImageMapUInt8(1000, 500, 4);
		map->set(255);

		TextRendererRef text_renderer = new TextRenderer();
		TextRendererFontFaceRef font = new TextRendererFontFace(text_renderer, PlatformUtils::getFontsDirPath() + "/Segoeui.ttf", 30);

		const std::string text = "W";
		TextRenderer::SizeInfo size_info = font->getGlyphSize(text, /*render_SDF=*/true);
		text_renderer->drawText(*map, text, -size_info.bitmap_left, size_info.bitmap_top, Colour3f(1,1,1), /*render_SDF=*/true, font.ptr(), /*emoji_font=*/nullptr);

		if(WRITE_IMAGES)
			PNGDecoder::write(*map, "text_ttf_W_SDF.png");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
#endif

	//-------------------------------------- Test rendering Emoji --------------------------------------
#ifdef _WIN32
	try
	{
		ImageMapUInt8Ref map = new ImageMapUInt8(500, 500, 3);
		map->zero();

		TextRendererRef text_renderer = new TextRenderer();
		TextRendererFontFaceRef font = new TextRendererFontFace(text_renderer, PlatformUtils::getFontsDirPath() + "/Seguiemj.ttf", 50); // Use the Windows Emoji version of Segoe UI.

		// https://unicode.org/emoji/charts/full-emoji-list.html#1f600, https://unicode.org/emoji/charts/full-emoji-list.html#1f60e, https://unicode.org/emoji/charts/full-emoji-list.html#1f4af
		const std::string text = UTF8Utils::encodeCodePoint(0x1F600) + 
				UTF8Utils::encodeCodePoint(0x1F60E) + 
				UTF8Utils::encodeCodePoint(0x1f4af);

		TextRenderer::SizeInfo size_info = text_renderer->getTextSize(text, font.ptr(), /*emoji_font=*/nullptr);

		text_renderer->drawText(*map, text, 10, 250, Colour3f(1,1,1), /*render_SDF=*/false, font.ptr(), /*emoji_font=*/nullptr);

		if(WRITE_IMAGES)
			PNGDecoder::write(*map, "emoji.png");
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
#endif

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

		/*TextRendererFontFace::SizeInfo size_info = */ text_renderer->getTextSize(text, font.ptr(), /*emoji_font=*/nullptr);

		text_renderer->drawText(*map, text, 10, 250, Colour3f(1,1,1), /*render_SDF=*/false, font.ptr(), /*emoji_font=*/nullptr);

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
		/*TextRendererFontFace::SizeInfo size_info = */ text_renderer->getTextSize(text, font.ptr(), /*emoji_font=*/nullptr);
		text_renderer->drawText(*map, text, 10, 250, Colour3f(1,1,1), /*render_SDF=*/false, font.ptr(), /*emoji_font=*/nullptr);

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
		/*TextRendererFontFace::SizeInfo size_info = */ text_renderer->getTextSize(text, font.ptr(), /*emoji_font=*/nullptr);
		text_renderer->drawText(*map, text, 10, 250, Colour3f(1,1,1), /*render_SDF=*/false, font.ptr(), /*emoji_font=*/nullptr);

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
			group->tasks.push_back(new DrawTextTestTask(text_renderer, font));

		task_manager.runTaskGroup(group);
	}

	conPrint("TextRenderer::test() done");
}


#endif // BUILD_TESTS
