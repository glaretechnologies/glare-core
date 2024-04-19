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


CharTexInfo FontCharTexCache::getCharTexture(Reference<OpenGLEngine> opengl_engine, TextRendererFontFace* font, TextRendererFontFace* emoji_font, const string_view charstring)
{
	const uint32 code_point = UTF8Utils::codePointForUTF8CharString(charstring);
	const bool is_emoji = code_point >= 0x1F32; // TODO: improve this.  Emojis are not one continuous block, see https://en.wikipedia.org/wiki/Emoji#In_Unicode
	TextRendererFontFace* use_font = is_emoji ? emoji_font : font;

	FontCharKey key;
	key.charstring = toString(charstring);
	key.font = use_font;

	auto res = char_to_tex_info_map.find(key);
	if(res != char_to_tex_info_map.end())
		return res->second;
	else
	{
		CharTexInfo tex_info = makeCharTexture(opengl_engine, font, emoji_font, charstring);
		char_to_tex_info_map[key] = tex_info;
		return tex_info;
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
	Reference<OpenGLEngine> opengl_engine, TextRendererFontFace* font, const string_view charstring, const TextRendererFontFace::SizeInfo size_info)
{
	const Vec2i topleft_plus_margin = topleft + Vec2i(margin_px);

	// Draw character onto image map
	font->drawText(*atlas->imagemap, charstring, topleft_plus_margin.x - size_info.min_bounds.x, /*baseline y=*/topleft_plus_margin.y + size_info.max_bounds.y, Colour3f(1.f));

	//TEMP: dump atlas image to disk for debugging
	//PNGDecoder::write(*atlas->imagemap, "d:/files/atlas.png");

	// Update OpenGL texture
	const size_t region_w = size_info.size.x + margin_px*2;
	const size_t region_h = size_info.size.y + margin_px*2;
	atlas->tex->loadRegionIntoExistingTexture(/*mipmap level=*/0, topleft.x, topleft.y, /*region w=*/region_w, /*region h=*/region_h, 
		atlas->imagemap->getWidth() * atlas->imagemap->getN(), // row stride (B)
		ArrayRef<uint8>(atlas->imagemap->getPixel(topleft.x, topleft.y), /*len=*/region_h * atlas->imagemap->getWidth() * atlas->imagemap->getN()), /*bind needed=*/true);

	// TEMP: update whole texture (for debugging)
	//atlas->tex->loadIntoExistingTexture(/*mipmap level=*/0, atlas->imagemap->getWidth(), atlas->imagemap->getHeight(), /*row stride (B)=*/atlas->imagemap->getHeight() * atlas->imagemap->getN(), 
	//	ArrayRef<uint8>(atlas->imagemap->getData(), atlas->imagemap->getDataSize()), /*bind needed=*/true);

	CharTexInfo res;
	res.size_info = size_info;
	res.tex = atlas->tex;
	res.atlas_min_texcoords = Vec2f(topleft_plus_margin.x                      / (float)atlas->imagemap->getWidth(), topleft_plus_margin.y                      / (float)atlas->imagemap->getHeight());
	res.atlas_max_texcoords = Vec2f((topleft_plus_margin.x + size_info.size.x) / (float)atlas->imagemap->getWidth(), (topleft_plus_margin.y + size_info.size.y) / (float)atlas->imagemap->getHeight());

	// Advance row next_topleft_coords x past the character
	row->next_topleft_coords.x += size_info.size.x + margin_px*2;
	row->max_y = myMax(row->max_y, row->next_topleft_coords.y + size_info.size.y + margin_px*2);
	
	return res;
}


CharTexInfo FontCharTexCache::makeCharTexture(Reference<OpenGLEngine> opengl_engine, TextRendererFontFace* text_renderer_font_, TextRendererFontFace* text_renderer_emoji_font_, const string_view charstring)
{
	const uint32 code_point = UTF8Utils::codePointForUTF8CharString(charstring);
	const bool is_emoji = code_point >= 0x1F32; // TODO: improve this.  Emojis are not one continuous block, see https://en.wikipedia.org/wiki/Emoji#In_Unicode
	TextRendererFontFace* use_font = is_emoji ? text_renderer_emoji_font_ : text_renderer_font_;

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
				Vec2i botright = row.next_topleft_coords + size_info.size + Vec2i(margin_px*2);
				if(botright.x >= atlas->imagemap->getWidth() || botright.y >= atlas->imagemap->getHeight())
				{
					// Not enough room in row.
				}
				else
				{
					// Enough room for char
					// Draw character onto image map
					return drawIntoRowPosition(atlas, &row, row.next_topleft_coords, opengl_engine, use_font, charstring, size_info);
				}
			}

			// If we got here, there was not enough room in any row.  So give up on this atlas and try next atlas (if any)

			// See if we could make a new row where it would fit.
			const Vec2i next_row_start_i(0, atlas->rows.empty() ? 0 : atlas->rows.back().max_y);
			const Vec2i next_row_botright = next_row_start_i + size_info.size;
			if(next_row_botright.x < atlas->imagemap->getWidth() && next_row_botright.y < atlas->imagemap->getHeight())
			{
				// Should fit in next row.
				AtlasRowInfo new_row_info;
				new_row_info.next_topleft_coords = next_row_start_i;
				new_row_info.max_y = next_row_start_i.y;
				atlas->rows.push_back(new_row_info);

				return drawIntoRowPosition(atlas, &atlas->rows.back(), next_row_start_i, opengl_engine, use_font, charstring, size_info);
			}

		}

		// If got here, there was not enough room in any atlas.  So create a new atlas.

		//if(atlas_created) // if we have already created an atlas for this char, error
		if(atlases.size() >= 1) // For now, just have max 1 atlas texture.
		{
			conPrint("Error trying to fit char!!!");

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
		atlas->imagemap = new ImageMapUInt8(512, 512, 4, NULL);
		atlas->imagemap->zero();

		TextureParams tex_params;
		tex_params.wrapping = OpenGLTexture::Wrapping_Clamp;
		tex_params.allow_compression = false;
		tex_params.use_mipmaps = false; // We will be setting only the base mipmap
		atlas->tex = opengl_engine->getOrLoadOpenGLTextureForMap2D(OpenGLTextureKey("atlas_" + toString(atlases.size())), *atlas->imagemap, tex_params);
		atlases.push_back(atlas);

		//atlas_created = true;
	}
}
