/*=====================================================================
FontCharTexCache.cpp
--------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "FontCharTexCache.h"


#include "../../graphics/PNGDecoder.h"
#include "../../utils/UTF8Utils.h"


FontCharTexCache::FontCharTexCache()
{
}


FontCharTexCache::~FontCharTexCache()
{
}


static inline bool codePointIsEmoji(uint32 code_point)
{
	return code_point >= 0x1F32; // TODO: improve this.  Emojis are not one continuous block, see https://en.wikipedia.org/wiki/Emoji#In_Unicode
}


CharTexInfo FontCharTexCache::getCharTexture(Reference<OpenGLEngine> opengl_engine, TextRendererFontFaceSizeSet* fonts, TextRendererFontFaceSizeSet* emoji_fonts, const string_view charstring, int font_size_px, bool render_SDF)
{
	const uint32 code_point = UTF8Utils::codePointForUTF8CharString(charstring);
	const bool is_emoji = codePointIsEmoji(code_point);

	FontCharKey key;
	key.charstring = toString(charstring);
	key.is_emoji_font = is_emoji;
	key.sdf = render_SDF;
	key.font_size_px = font_size_px;

	auto res = char_to_tex_info_map.find(key);
	if(res != char_to_tex_info_map.end())
		return res->second;
	else
	{
		CharTexInfo tex_info = makeCharTexture(opengl_engine, fonts, emoji_fonts, charstring, font_size_px, render_SDF);
		char_to_tex_info_map[key] = tex_info;
		return tex_info;
	}
}


void FontCharTexCache::writeAtlasToDiskDebug(const std::string& path)
{
	if(!atlases.empty())
	{
		PNGDecoder::write(*atlases[0]->imagemap, path);
	}
}


/*
Example of a letter like 'g' with a descender, in which case size_info.min_bounds < 0.

                                              font y
                                                ^ 
                                                |
                            |-------------------|-----------------------------------> map x
                            |                   |    size_info.max_bounds.x
                 topleft.y--|                 x |      |
     topleft_plus_margin.y--|           topleft |   _          --- size_info.max_bounds.y
                            |                   | /   \
                            |                   ||  O  |
                            |                   | \    |
                            |                   |____|_|__________> font x
                            |                      / /
                            |                     --          --- size_info.min_bounds.y
                            |                 | |
                            |                 | topleft_plus_margin.x
                            |               topleft.x
                            |
                            |
                            v
                          map y

*/

static const int margin_px = 1;


static void drawBorderAroundGlyph(ImageMapUInt8& map, const Vec2i& topleft, const TextRendererFontFace::SizeInfo size_info, const uint8* border_col)
{
	for(int x=topleft.x; x<topleft.x + size_info.bitmap_width + margin_px*2; ++x)
	{
		map.setPixelIfInBounds(x, topleft.y, border_col); // Top row
		map.setPixelIfInBounds(x, topleft.y + margin_px + size_info.bitmap_height, border_col); // bottom row
	}

	for(int y=topleft.y; y<topleft.y + size_info.bitmap_height + margin_px*2; ++y)
	{
		map.setPixelIfInBounds(topleft.x, y, border_col); // left column
		map.setPixelIfInBounds(topleft.x + margin_px + size_info.bitmap_width, y, border_col); // right column
	}
}


// Draws into image map, updates gl texture, updates row info
static CharTexInfo drawIntoRowPosition(AtlasTexInfo* atlas, AtlasRowInfo* row, const Vec2i& topleft, 
	TextRendererFontFace* font, const string_view charstring, const TextRendererFontFace::SizeInfo size_info, bool render_SDF, bool is_emoji)
{
	const Vec2i topleft_plus_margin = topleft + Vec2i(margin_px);

	// Draw character glyph onto image map
	font->drawText(*atlas->imagemap, charstring, topleft_plus_margin.x - size_info.bitmap_left, /*baseline y=*/topleft_plus_margin.y + size_info.bitmap_top, Colour3f(1.f), render_SDF);

	// Draw border around glyph
	if(is_emoji)
	{
		// For Emojis, which seem to have black backgrounds (all of them?), use colour black with alpha 0.
		const uint8 border_col[4] = { 0, 0, 0, 0 };
		drawBorderAroundGlyph(*atlas->imagemap, topleft, size_info, border_col);
	}
	else
	{
		// For non-emoji glyphs, use white colour with alpha 0.  This prevents black edges on fonts due to filtering of colour values.
		const uint8 border_col[4] = { 255, 255, 255, 0 };
		drawBorderAroundGlyph(*atlas->imagemap, topleft, size_info, border_col);
	}


	//TEMP: dump atlas image to disk for debugging
	//PNGDecoder::write(*atlas->imagemap, "d:/files/atlas.png");

	// Update OpenGL texture
	const int region_w = size_info.bitmap_width  + margin_px*2;
	const int region_h = size_info.bitmap_height + margin_px*2;

	const size_t row_stride_B = atlas->imagemap->getWidth() * atlas->imagemap->getN();

	if(atlas->tex.nonNull())
	{
#if !defined(EMSCRIPTEN)
		// NOTE: On web/emscripten this gives the error 'WebGL: INVALID_OPERATION: texSubImage2D: ArrayBufferView not big enough for request' on web, work out what is happening.
		// I can't see any errors on my side.
		atlas->tex->loadRegionIntoExistingTexture(/*mipmap level=*/0, topleft.x, topleft.y, /*z=*/0, /*region w=*/(size_t)region_w, /*region h=*/(size_t)region_h, /*region depth=*/1,
			row_stride_B, ArrayRef<uint8>(atlas->imagemap->getPixel(topleft.x, topleft.y), /*len=*/region_h * atlas->imagemap->getWidth() * atlas->imagemap->getN()), /*bind needed=*/true);
#else
		// TEMP: update whole texture (for debugging)
		atlas->tex->loadIntoExistingTexture(/*mipmap level=*/0, atlas->imagemap->getWidth(), atlas->imagemap->getHeight(), /*row stride (B)=*/atlas->imagemap->getHeight() * atlas->imagemap->getN(), 
			ArrayRef<uint8>(atlas->imagemap->getData(), atlas->imagemap->getDataSize()), /*bind needed=*/true);
#endif

		atlas->tex->buildMipMaps(); // NOTE: could be pretty slow if it rebuilds mipmaps for the entire image.  Could do ourselves.
	}

	CharTexInfo res;
	res.size_info = size_info;
	res.tex = atlas->tex;

	// Get pixel bounds of glyph rectangle.
	const int top_left_x_px  = topleft_plus_margin.x - size_info.bitmap_left + size_info.min_bounds.x;
	const int bot_right_x_px = topleft_plus_margin.x - size_info.bitmap_left + size_info.max_bounds.x;

	const int top_left_y_px  = topleft_plus_margin.y + size_info.bitmap_top - size_info.max_bounds.y;
	const int bot_right_y_px = topleft_plus_margin.y + size_info.bitmap_top - size_info.min_bounds.y;

	res.atlas_glyph_min_texcoords = Vec2f(top_left_x_px  / (float)atlas->imagemap->getWidth(), top_left_y_px  / (float)atlas->imagemap->getHeight());
	res.atlas_glyph_max_texcoords = Vec2f(bot_right_x_px / (float)atlas->imagemap->getWidth(), bot_right_y_px / (float)atlas->imagemap->getHeight());

	// Advance row next_topleft_coords x past the character
	row->next_topleft_coords.x += region_w;
	row->max_y = myMax(row->max_y, row->next_topleft_coords.y + region_h);
	
	return res;
}


CharTexInfo FontCharTexCache::makeCharTexture(Reference<OpenGLEngine> opengl_engine, TextRendererFontFaceSizeSet* text_renderer_fonts_, TextRendererFontFaceSizeSet* text_renderer_emoji_fonts_, 
	const string_view charstring, int font_size_px, bool render_SDF)
{
	const uint32 code_point = UTF8Utils::codePointForUTF8CharString(charstring);
	const bool is_emoji = codePointIsEmoji(code_point);
	TextRendererFontFaceSizeSet* use_font_set = is_emoji ? text_renderer_emoji_fonts_ : text_renderer_fonts_;
	TextRendererFontFaceRef use_font = use_font_set->getFontFaceForSize(font_size_px);

	const TextRendererFontFace::SizeInfo size_info = use_font->getGlyphSize(charstring, render_SDF);

	//bool atlas_created = false;
	while(1)
	{
		// Try and find space in existing atlases
		for(size_t i=0; i<atlases.size(); ++i)
		{
			AtlasTexInfo* atlas = atlases[i].ptr();

			// Try and find space in existing atlas rows

			for(size_t r=0; r<atlas->rows.size(); ++r)
			{
				AtlasRowInfo& row = atlas->rows[r];
				Vec2i botright = row.next_topleft_coords + size_info.bitmapSize() + Vec2i(margin_px*2);

				const int next_row_or_end_y = ((r + 1) < atlas->rows.size()) ? atlas->rows[r + 1].next_topleft_coords.y : (int)atlas->imagemap->getHeight();

				if(botright.x >= atlas->imagemap->getWidth() || botright.y >= next_row_or_end_y)
				{
					// Not enough room in row, continue search to next row
				}
				else
				{
					// Enough room for char
					// Draw character onto image map
					return drawIntoRowPosition(atlas, &row, row.next_topleft_coords, use_font.ptr(), charstring, size_info, render_SDF, is_emoji);
				}
			}

			// If we got here, there was not enough room in any row.  So give up on this atlas and try next atlas (if any)

			// See if we could make a new row where it would fit.
			const Vec2i next_row_start_i(0, atlas->rows.empty() ? 0 : atlas->rows.back().max_y);
			const Vec2i next_row_botright = next_row_start_i + size_info.bitmapSize();
			if(next_row_botright.x < atlas->imagemap->getWidth() && next_row_botright.y < atlas->imagemap->getHeight())
			{
				// Should fit in next row.
				AtlasRowInfo new_row_info;
				new_row_info.next_topleft_coords = next_row_start_i;
				new_row_info.max_y = next_row_start_i.y;
				atlas->rows.push_back(new_row_info);

				return drawIntoRowPosition(atlas, &atlas->rows.back(), next_row_start_i, use_font.ptr(), charstring, size_info, render_SDF, is_emoji);
			}

		}

		// If got here, there was not enough room in any atlas.  So create a new atlas.

		//if(atlas_created) // if we have already created an atlas for this char, error
		if(atlases.size() >= 1) // For now, just have max 1 atlas texture.
		{
			conPrint("Error trying to fit char in atlas!!!");

			// Just return some dummy data
			CharTexInfo res;
			res.size_info = size_info;
			res.tex = atlases[0]->tex;
			res.atlas_glyph_min_texcoords = Vec2f(0, 0);
			res.atlas_glyph_max_texcoords = Vec2f(0.001f, 0.001f);
			return res;
		}

		// Create a new atlas
		Reference<AtlasTexInfo> atlas = new AtlasTexInfo();
		atlas->imagemap = new ImageMapUInt8(1024, 1024, 4, NULL);
		atlas->imagemap->zero();


		TextureParams tex_params;
		tex_params.wrapping = OpenGLTexture::Wrapping_Clamp;
		tex_params.allow_compression = false;
		tex_params.use_mipmaps = true;
		//tex_params.use_sRGB = false; // For SDF
		if(opengl_engine.nonNull())
			atlas->tex = opengl_engine->getOrLoadOpenGLTextureForMap2D(OpenGLTextureKey("atlas_" + toString(atlases.size())), *atlas->imagemap, tex_params);
		atlases.push_back(atlas);

		//atlas_created = true;
	}
}





#if BUILD_TESTS


#include <graphics/PNGDecoder.h>
#include <utils/TaskManager.h>
#include <utils/StringUtils.h>
#include <utils/TestUtils.h>
#include <PlatformUtils.h>


void FontCharTexCache::test()
{
	conPrint("FontCharTexCache::test()");

	//const bool WRITE_IMAGES = true;

	TextRendererRef text_renderer = new TextRenderer();

	TextRendererFontFaceSizeSetRef fonts       = new TextRendererFontFaceSizeSet(text_renderer, PlatformUtils::getFontsDirPath() + "/Segoeui.ttf");
	TextRendererFontFaceSizeSetRef emoji_fonts = new TextRendererFontFaceSizeSet(text_renderer, PlatformUtils::getFontsDirPath() + "/Seguiemj.ttf");

	FontCharTexCacheRef cache = new FontCharTexCache();

	CharTexInfo info;
	info = cache->getCharTexture(/*opengl engine=*/NULL, fonts.ptr(), emoji_fonts.ptr(), "A", 42, /*render SDF=*/true);
	info = cache->getCharTexture(/*opengl engine=*/NULL, fonts.ptr(), emoji_fonts.ptr(), "B", 42, /*render SDF=*/true);

	const std::string grinning_face = UTF8Utils::encodeCodePoint(0x1F600);
	info = cache->getCharTexture(/*opengl engine=*/NULL, fonts.ptr(), emoji_fonts.ptr(), grinning_face, 42, /*render SDF=*/true);

	info = cache->getCharTexture(/*opengl engine=*/NULL, fonts.ptr(), emoji_fonts.ptr(), "C", 42, /*render SDF=*/true);

	info = cache->getCharTexture(/*opengl engine=*/NULL, fonts.ptr(), emoji_fonts.ptr(), "H", 42, /*render SDF=*/false);
	info = cache->getCharTexture(/*opengl engine=*/NULL, fonts.ptr(), emoji_fonts.ptr(), "E", 42, /*render SDF=*/false);
	info = cache->getCharTexture(/*opengl engine=*/NULL, fonts.ptr(), emoji_fonts.ptr(), "L", 42, /*render SDF=*/false);
	info = cache->getCharTexture(/*opengl engine=*/NULL, fonts.ptr(), emoji_fonts.ptr(), "O", 42, /*render SDF=*/false);

	cache->writeAtlasToDiskDebug("test_font_atlas.png");

	conPrint("FontCharTexCache::test() done");
}


#endif // BUILD_TESTS
