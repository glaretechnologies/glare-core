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
                            |                   |
                 topleft.y--|                 x |
     topleft_plus_margin.y--|           topleft |   _          --- size_info.max_bounds.y
                            |                   | /   \
                            |                   ||  O  |
                            |                   | \    |
                            |                   |____|_|__________> font x
                            |                      / /
                            |                     --
                            |                 | |
                            |                 | topleft_plus_margin.x
                            |               topleft.x
                            |
                            |
                            v
                          map y

*/

static const int margin_px = 1;

// Draws into image map, updates gl texture, updates row info
static CharTexInfo drawIntoRowPosition(AtlasTexInfo* atlas, AtlasRowInfo* row, const Vec2i& topleft, 
	Reference<OpenGLEngine> opengl_engine, TextRendererFontFace* font, const string_view charstring, const TextRendererFontFace::SizeInfo size_info, bool render_SDF)
{
	const Vec2i topleft_plus_margin = topleft + Vec2i(margin_px);

	// Draw character onto image map
	font->drawText(*atlas->imagemap, charstring, topleft_plus_margin.x - size_info.min_bounds.x, /*baseline y=*/topleft_plus_margin.y + size_info.max_bounds.y, Colour3f(1.f), render_SDF);

	//TEMP: dump atlas image to disk for debugging
	//PNGDecoder::write(*atlas->imagemap, "d:/files/atlas.png");

	// Update OpenGL texture
	const size_t region_w = size_info.getSize().x + margin_px*2;
	const size_t region_h = size_info.getSize().y + margin_px*2;

	const size_t row_stride_B = atlas->imagemap->getWidth() * atlas->imagemap->getN();

#if !defined(EMSCRIPTEN)
	// NOTE: On web/emscripten this gives the error 'WebGL: INVALID_OPERATION: texSubImage2D: ArrayBufferView not big enough for request' on web, work out what is happening.
	// I can't see any errors on my side.
	atlas->tex->loadRegionIntoExistingTexture(/*mipmap level=*/0, topleft.x, topleft.y, /*region w=*/region_w, /*region h=*/region_h, 
		row_stride_B, ArrayRef<uint8>(atlas->imagemap->getPixel(topleft.x, topleft.y), /*len=*/region_h * atlas->imagemap->getWidth() * atlas->imagemap->getN()), /*bind needed=*/true);
#else
	// TEMP: update whole texture (for debugging)
	atlas->tex->loadIntoExistingTexture(/*mipmap level=*/0, atlas->imagemap->getWidth(), atlas->imagemap->getHeight(), /*row stride (B)=*/atlas->imagemap->getHeight() * atlas->imagemap->getN(), 
		ArrayRef<uint8>(atlas->imagemap->getData(), atlas->imagemap->getDataSize()), /*bind needed=*/true);
#endif

	atlas->tex->buildMipMaps(); // NOTE: could be pretty slow if it rebuilds mipmaps for the entire image.  Could do ourselves.

	CharTexInfo res;
	res.size_info = size_info;
	res.tex = atlas->tex;
	res.atlas_min_texcoords = Vec2f(topleft_plus_margin.x                           / (float)atlas->imagemap->getWidth(), topleft_plus_margin.y                           / (float)atlas->imagemap->getHeight());
	res.atlas_max_texcoords = Vec2f((topleft_plus_margin.x + size_info.getSize().x) / (float)atlas->imagemap->getWidth(), (topleft_plus_margin.y + size_info.getSize().y) / (float)atlas->imagemap->getHeight());

	// Advance row next_topleft_coords x past the character
	row->next_topleft_coords.x += size_info.getSize().x + margin_px*2;
	row->max_y = myMax(row->max_y, row->next_topleft_coords.y + size_info.getSize().y + margin_px*2);
	
	return res;
}


CharTexInfo FontCharTexCache::makeCharTexture(Reference<OpenGLEngine> opengl_engine, TextRendererFontFaceSizeSet* text_renderer_fonts_, TextRendererFontFaceSizeSet* text_renderer_emoji_fonts_, 
	const string_view charstring, int font_size_px, bool render_SDF)
{
	const uint32 code_point = UTF8Utils::codePointForUTF8CharString(charstring);
	const bool is_emoji = codePointIsEmoji(code_point);
	TextRendererFontFaceSizeSet* use_font_set = is_emoji ? text_renderer_emoji_fonts_ : text_renderer_fonts_;
	TextRendererFontFaceRef use_font = use_font_set->getFontFaceForSize(font_size_px);

	const TextRendererFontFace::SizeInfo size_info = use_font->getTextSize(charstring);

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
				Vec2i botright = row.next_topleft_coords + size_info.getSize() + Vec2i(margin_px*2);

				const int next_row_or_end_y = ((r + 1) < atlas->rows.size()) ? atlas->rows[r + 1].next_topleft_coords.y : (int)atlas->imagemap->getHeight();

				if(botright.x >= atlas->imagemap->getWidth() || botright.y >= next_row_or_end_y)
				{
					// Not enough room in row, continue search to next row
				}
				else
				{
					// Enough room for char
					// Draw character onto image map
					return drawIntoRowPosition(atlas, &row, row.next_topleft_coords, opengl_engine, use_font.ptr(), charstring, size_info, render_SDF);
				}
			}

			// If we got here, there was not enough room in any row.  So give up on this atlas and try next atlas (if any)

			// See if we could make a new row where it would fit.
			const Vec2i next_row_start_i(0, atlas->rows.empty() ? 0 : atlas->rows.back().max_y);
			const Vec2i next_row_botright = next_row_start_i + size_info.getSize();
			if(next_row_botright.x < atlas->imagemap->getWidth() && next_row_botright.y < atlas->imagemap->getHeight())
			{
				// Should fit in next row.
				AtlasRowInfo new_row_info;
				new_row_info.next_topleft_coords = next_row_start_i;
				new_row_info.max_y = next_row_start_i.y;
				atlas->rows.push_back(new_row_info);

				return drawIntoRowPosition(atlas, &atlas->rows.back(), next_row_start_i, opengl_engine, use_font.ptr(), charstring, size_info, render_SDF);
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
			res.atlas_min_texcoords = Vec2f(0, 0);
			res.atlas_max_texcoords = Vec2f(0.001f, 0.001f);
			return res;
		}

		// Create a new atlas
		Reference<AtlasTexInfo> atlas = new AtlasTexInfo();
		atlas->imagemap = new ImageMapUInt8(1024, 1024, 4, NULL);
		const uint8 vals[] = {255, 255, 255, 0 };
		atlas->imagemap->setFromComponentValues(vals);


		TextureParams tex_params;
		tex_params.wrapping = OpenGLTexture::Wrapping_Clamp;
		tex_params.allow_compression = false;
		tex_params.use_mipmaps = true;
		//tex_params.use_sRGB = false; // For SDF
		atlas->tex = opengl_engine->getOrLoadOpenGLTextureForMap2D(OpenGLTextureKey("atlas_" + toString(atlases.size())), *atlas->imagemap, tex_params);
		atlases.push_back(atlas);

		//atlas_created = true;
	}
}
