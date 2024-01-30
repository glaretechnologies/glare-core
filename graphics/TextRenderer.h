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
	void drawText(ImageMapUInt8& map, const std::string& text, int x, int y, const Colour3f& col);

	struct SizeInfo
	{
		Vec2i size;
		Vec2i min_bounds; // With x increasing to right, y increasing up.
		Vec2i max_bounds;
	};

	SizeInfo getTextSize(const std::string& text);

	int getFaceAscender(); // The ascender is the vertical distance from the horizontal baseline to the highest ‘character’ coordinate in a font face. (https://freetype.org/freetype2/docs/tutorial/step2.html)
	// NOTE: doesn't seem to take font size into account.

	int getFontSizePixels() const { return font_size_pixels; }
	

	TextRendererRef renderer;
	struct FT_FaceRec_* face;
	int font_size_pixels;
};


typedef Reference<TextRendererFontFace> TextRendererFontFaceRef;
