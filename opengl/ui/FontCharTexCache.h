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
	Vec2f atlas_min_texcoords; // top left
	Vec2f atlas_max_texcoords; // bottom right
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

	// Get info about a character drawn onto the atlast texture.  Adds it to atlas if not already on there.
	CharTexInfo getCharTexture(Reference<OpenGLEngine> opengl_engine, TextRendererFontFace* text_renderer_font, TextRendererFontFace* text_renderer_emoji_font, const string_view charstring);

	CharTexInfo makeCharTexture(Reference<OpenGLEngine> opengl_engine, TextRendererFontFace* text_renderer_font, TextRendererFontFace* text_renderer_emoji_font, const string_view charstring);


	std::vector<Reference<AtlasTexInfo>> atlases;

	struct FontCharKey
	{
		TextRendererFontFace* font;
		std::string charstring;

		bool operator < (const FontCharKey& b) const
		{
			if(font < b.font)
				return true;
			else if(font > b.font)
				return false;
			else
				return charstring < b.charstring;
		}
	};
	std::map<FontCharKey, CharTexInfo> char_to_tex_info_map;
};


typedef Reference<FontCharTexCache> FontCharTexCacheRef;
