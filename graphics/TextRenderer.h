/*=====================================================================
TextRenderer
------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "ImageMap.h"
#include "colour3.h"
#include "../maths/vec2.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Mutex.h"
#include "../utils/string_view.h"
#include <string>
#include <map>


struct FT_LibraryRec_;
struct FT_FaceRec_;
class TextRendererFontFace;


/*=====================================================================
TextRenderer
------------
Uses FreeType
=====================================================================*/
class TextRenderer : public ThreadSafeRefCounted
{
public:
	TextRenderer();
	~TextRenderer();
	
	static void test();

	struct FT_LibraryRec_* library;

	Mutex mutex; // A FT_Library object can't be used by multiple threads concurrently, so protect with mutex.
	// If we really wanted concurrent font text rendering we could use a per-thread FT_Library.
};


typedef Reference<TextRenderer> TextRendererRef;


class TextRendererFontFace : public ThreadSafeRefCounted
{
public:
	TextRendererFontFace(TextRendererRef renderer, const std::string& font_file_path, int font_size_pixels);
	~TextRendererFontFace();

	// Draw text at (x, y).
	// The y coordinate give the position of the text baseline.
	// Col is used if the font is greyscale.  If the font is a colour font (e.g. Emoji), the font colour is used.
	// Throws glare::Exception on failure, for example on invalid UTF-8 string.
	void drawText(ImageMapUInt8& map, const string_view text, int x, int y, const Colour3f& col, bool render_SDF);

	struct SizeInfo
	{
		Vec2i glyphSize() const { return max_bounds - min_bounds; }
		Vec2i bitmapSize() const { return Vec2i(bitmap_width, bitmap_height); }

		Vec2i min_bounds; // Minimum pixel coordinates of glyph rectangle, with x increasing to right, y increasing up.  min_bounds.y will be < 0 for characters with descenders such as 'g'
		Vec2i max_bounds; // Maximum pixel coordinates of glyph rectangle.
		float hori_advance; // Number of pixels to advance drawing location after drawing string.  See https://freetype.org/freetype2/docs/tutorial/step2.html

		int bitmap_left; // "horizontal distance from the glyph origin (0,0) to the leftmost pixel of the glyph bitmap."  Will be negative if bitmap starts left of origin.
		int bitmap_top; // "vertical distance from the glyph origin (0,0) to the topmost pixel of the glyph bitmap (more precise, to the pixel just above the bitmap)"
		int bitmap_width;
		int bitmap_height;
	};

	// Get size info for a single character glyph
	// Throws glare::Exception on failure, for example on invalid UTF-8 string.
	SizeInfo getGlyphSize(const string_view text, bool render_SDF);

	// Get size info for a string that may have multiple characters.
	// NOTE: just about as slow as drawText().
	// Assumes not doing SDF rendering
	// Throws glare::Exception on failure, for example on invalid UTF-8 string.
	SizeInfo getTextSize(const string_view text);


	float getFaceAscender(); // The ascender is the vertical distance from the horizontal baseline to the highest ‘character’ coordinate in a font face. (https://freetype.org/freetype2/docs/tutorial/step2.html)

	int getFontSizePixels() const { return font_size_pixels; }
	

	TextRendererRef renderer;
	struct FT_FaceRec_* face;
	int font_size_pixels;
};


typedef Reference<TextRendererFontFace> TextRendererFontFaceRef;



class TextRendererFontFaceSizeSet : public ThreadSafeRefCounted
{
public:
	TextRendererFontFaceSizeSet(TextRendererRef renderer, const std::string& font_file_path);

	TextRendererFontFaceRef getFontFaceForSize(int font_size_pixels);

	std::string font_file_path;
	std::map<int, TextRendererFontFaceRef> fonts_for_size;
	TextRendererRef renderer;
};

typedef Reference<TextRendererFontFaceSizeSet> TextRendererFontFaceSizeSetRef;

