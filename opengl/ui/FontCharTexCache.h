/*=====================================================================
FontCharTexCache.h
------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "../OpenGLEngine.h"
#include "../../graphics/TextRenderer.h"
#include "../../utils/RefCounted.h"
#include "../../utils/Reference.h"
#include "../../maths/Rect2.h"
#include <string>


class GLUIMouseWheelEvent;


// Information about a character in an atlas
struct CharTexInfo
{
	Reference<OpenGLTexture> tex;
	TextRendererFontFace::SizeInfo size_info;
	Vec2f atlas_glyph_min_texcoords; // top left tex coordinates, corresponding to corner of glyph rectange (not bitmap)
	Vec2f atlas_glyph_max_texcoords; // bottom right tex coordinates, corresponding to corner of glyph rectange (not bitmap)
};


struct AtlasRowInfo
{
	Vec2i next_topleft_coords;
	int max_y;
};


struct AtlasTexInfo : public RefCounted
{
	ImageMapUInt8Ref imagemap;
	Reference<OpenGLTexture> tex;
	std::vector<AtlasRowInfo> rows;
};


/*=====================================================================
FontCharTexCache
----------------
Draws text characters onto an atlas texture map, or retrieves existing 
drawn chars.
=====================================================================*/
class FontCharTexCache : public RefCounted
{
public:
	FontCharTexCache();
	virtual ~FontCharTexCache();

	// Get info about a character drawn onto the atlas texture.  Adds it to atlas if not already on there.
	CharTexInfo getCharTexture(Reference<OpenGLEngine> opengl_engine, TextRendererFontFaceSizeSet* text_renderer_fonts, TextRendererFontFaceSizeSet* text_renderer_emoji_fonts, const string_view charstring, int font_size_px, bool render_SDF);

	void writeAtlasToDiskDebug(const std::string& path);

	static void test();
private:
	CharTexInfo makeCharTexture(Reference<OpenGLEngine> opengl_engine, TextRendererFontFaceSizeSet* text_renderer_fonts, TextRendererFontFaceSizeSet* text_renderer_emoji_fonts, const string_view charstring, int font_size_px, bool render_SDF);


	std::vector<Reference<AtlasTexInfo>> atlases;

	struct FontCharKey
	{
		bool is_emoji_font;
		bool sdf;
		int font_size_px;
		std::string charstring;

		bool operator < (const FontCharKey& b) const
		{
			if(is_emoji_font < b.is_emoji_font)
				return true;
			if(is_emoji_font > b.is_emoji_font)
				return false;
			if(sdf < b.sdf)
				return true;
			if(sdf > b.sdf)
				return false;
			if(font_size_px < b.font_size_px)
				return true;
			if(font_size_px > b.font_size_px)
				return false;
			return charstring < b.charstring;
		}
	};
	std::map<FontCharKey, CharTexInfo> char_to_tex_info_map;
};


typedef Reference<FontCharTexCache> FontCharTexCacheRef;
