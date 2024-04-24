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
	void drawText(ImageMapUInt8& map, const string_view text, int x, int y, const Colour3f& col);

	struct SizeInfo
	{
		Vec2i getSize() const { return max_bounds - min_bounds; }

		Vec2i min_bounds; // With x increasing to right, y increasing up.  min_bounds.y may be < 0 for characters with descenders such as 'g'
		Vec2i max_bounds;
		float hori_advance; // Number of pixels to advance drawing location after drawing string.  See https://freetype.org/freetype2/docs/tutorial/step2.html
	};

	SizeInfo getTextSize(const string_view text);

	float getFaceAscender(); // The ascender is the vertical distance from the horizontal baseline to the highest ‘character’ coordinate in a font face. (https://freetype.org/freetype2/docs/tutorial/step2.html)
	// NOTE: doesn't seem to take font size into account.

	int getFontSizePixels() const { return font_size_pixels; }
	

	TextRendererRef renderer;
	struct FT_FaceRec_* face;
	int font_size_pixels;
};


typedef Reference<TextRendererFontFace> TextRendererFontFaceRef;
