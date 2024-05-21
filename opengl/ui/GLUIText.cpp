/*=====================================================================
GLUIText.cpp
------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "GLUIText.h"


#include "GLUI.h"
#include "../graphics/SRGBUtils.h"
#include "../OpenGLMeshRenderData.h"
#include "../GLMeshBuilding.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/UTF8Utils.h"
#include "../utils/RuntimeCheck.h"


static inline Vec2f toVec2f(const Vec3f& v)
{
	return Vec2f(v.x, v.y);
}


Reference<OpenGLMeshRenderData> GLUIText::makeMeshDataForText(Reference<OpenGLEngine>& opengl_engine, FontCharTexCache* font_char_text_cache, 
	TextRendererFontFaceSizeSet* fonts, TextRendererFontFaceSizeSet* emoji_fonts, const std::string& text, const int font_size_px, float vert_pos_scale, bool render_SDF, 
	Rect2f& rect_os_out, OpenGLTextureRef& atlas_texture_out, std::vector<CharPositionInfo>& char_positions_font_coords_out)
{
	// Make mesh data
	rect_os_out = Rect2f(Vec2f(0.f), Vec2f(0.f));
	char_positions_font_coords_out.clear();

	const size_t num_codepoints = UTF8Utils::numCodePointsInString(text);

	js::Vector<Vec3f, 16> verts(num_codepoints * 4);
	js::Vector<Vec2f, 16> uvs(num_codepoints * 4);
	js::Vector<uint32, 16> indices(num_codepoints * 6); // num_codepoints * 2 tris/code point * 3 indices/tri

	size_t cur_string_byte_i = 0;
	Vec3f cur_char_pos(0.f);
	for(size_t i=0; i<num_codepoints; ++i)
	{
		runtimeCheck(cur_string_byte_i < text.size());
		const size_t char_num_bytes = UTF8Utils::numBytesForChar(text[cur_string_byte_i]);
		if(cur_string_byte_i + char_num_bytes > text.size())
			throw glare::Exception("Invalid UTF-8 string.");
		const string_view substring(text.data() + cur_string_byte_i, char_num_bytes);
		const CharTexInfo info = font_char_text_cache->getCharTexture(opengl_engine, fonts, emoji_fonts, substring, font_size_px, render_SDF);
		atlas_texture_out = info.tex;
		

		// NOTE: min and max y texcoords are interchanged with increasing y downwards, due to texture effectively being flipped vertically when loaded into OpenGL.
		uvs[i*4 + 0] = Vec2f(info.atlas_glyph_min_texcoords.x, info.atlas_glyph_max_texcoords.y); // bot left vertex
		uvs[i*4 + 1] = Vec2f(info.atlas_glyph_min_texcoords.x, info.atlas_glyph_min_texcoords.y); // top left vertex
		uvs[i*4 + 2] = Vec2f(info.atlas_glyph_max_texcoords.x, info.atlas_glyph_min_texcoords.y); // top right vertex
		uvs[i*4 + 3] = Vec2f(info.atlas_glyph_max_texcoords.x, info.atlas_glyph_max_texcoords.y); // bot right vertex


		const Vec3f min_pos = (cur_char_pos + Vec3f((float)info.size_info.min_bounds.x, (float)info.size_info.min_bounds.y, 0)) * vert_pos_scale;
		const Vec3f max_pos = (cur_char_pos + Vec3f((float)info.size_info.max_bounds.x, (float)info.size_info.max_bounds.y, 0)) * vert_pos_scale;
		
		verts[i*4 + 0] = min_pos;                                // bot left vertex
		verts[i*4 + 1] = Vec3f(min_pos.x, max_pos.y, min_pos.z); // top left vertex
		verts[i*4 + 2] = max_pos;                                // top right vertex
		verts[i*4 + 3] = Vec3f(max_pos.x, min_pos.y, min_pos.z); // bot right vertex


		CharPositionInfo pos_info;
		pos_info.pos = Vec2f(cur_char_pos.x, 0.f);
		pos_info.hori_advance = info.size_info.hori_advance;
		char_positions_font_coords_out.push_back(pos_info);

		cur_char_pos.x += info.size_info.hori_advance;

		if(i == 0)
			rect_os_out = Rect2f(toVec2f(min_pos), toVec2f(max_pos));
		else
		{
			rect_os_out.enlargeToHoldPoint(toVec2f(min_pos));
			rect_os_out.enlargeToHoldPoint(toVec2f(max_pos));
		}

		indices[i*6 + 0] = (uint32)i*4 + 0;
		indices[i*6 + 1] = (uint32)i*4 + 1;
		indices[i*6 + 2] = (uint32)i*4 + 2;
		indices[i*6 + 3] = (uint32)i*4 + 0;
		indices[i*6 + 4] = (uint32)i*4 + 2;
		indices[i*6 + 5] = (uint32)i*4 + 3;

		cur_string_byte_i += char_num_bytes;
	}

	return GLMeshBuilding::buildMeshRenderData(*opengl_engine->vert_buf_allocator, verts, uvs, indices);
}


GLUIText::GLUIText(GLUI& glui, Reference<OpenGLEngine>& opengl_engine_, const std::string& text_, const Vec2f& botleft_, const CreateArgs& args)
{
	opengl_engine = opengl_engine_;

	text_colour = args.colour;
	font_size_px = args.font_size_px;
	botleft = botleft_;
	text = text_;
	z = args.z;

	try
	{
		rect_os = Rect2f(Vec2f(0.f), Vec2f(0.f));
		OpenGLTextureRef atlas_texture;

		Reference<OpenGLMeshRenderData> mesh_data = makeMeshDataForText(opengl_engine_, glui.font_char_text_cache.ptr(), glui.getFonts(), glui.getEmojiFonts(), text, font_size_px, /*vert_pos_scale=*/1.f, 
			/*render_SDF=*/false,
			/*rect os out=*/rect_os, /*atlas texture out=*/atlas_texture, /*char_positions_font_coords_out=*/char_positions_fc);

		const float x_scale = 2.f / opengl_engine->getMainViewPortWidth();
		const float y_scale = 2.f / opengl_engine->getMainViewPortHeight();

	
		overlay_ob = new OverlayObject();
		overlay_ob->mesh_data = mesh_data;
		overlay_ob->material.albedo_linear_rgb = args.colour;
		overlay_ob->material.alpha = args.alpha;
		overlay_ob->material.albedo_texture = atlas_texture;


		this->rect = Rect2f(botleft + rect_os.getMin() * x_scale, botleft + rect_os.getMax() * x_scale);

		overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * opengl_engine->getViewPortAspectRatio(), z) * Matrix4f::scaleMatrix(x_scale, y_scale, 1);

		opengl_engine->addOverlayObject(overlay_ob);
	}
	catch(glare::Exception& e)
	{
		assert(0);
		conPrint("Warning: exception in GLUIText: " + e.what());
	}
}


GLUIText::~GLUIText()
{
	if(overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(overlay_ob);
}


void GLUIText::setColour(const Colour3f& col)
{
	if(overlay_ob.nonNull())
		overlay_ob->material.albedo_linear_rgb = col;
}


void GLUIText::setAlpha(float alpha)
{
	if(overlay_ob.nonNull())
		overlay_ob->material.alpha = alpha;
}


void GLUIText::setPos(const Vec2f& botleft_)
{
	botleft = botleft_;

	updateGLTransform();
}


void GLUIText::updateGLTransform()
{
	const float x_scale = 2.f / opengl_engine->getMainViewPortWidth();
	const float y_scale = 2.f / opengl_engine->getMainViewPortHeight();

	this->rect = Rect2f(botleft + rect_os.getMin() * x_scale, botleft + rect_os.getMax() * x_scale);

	if(overlay_ob.nonNull())
		overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * opengl_engine->getViewPortAspectRatio(), z) * Matrix4f::scaleMatrix(x_scale, y_scale, 1);
}


// Get position to lower left of unicode character with given index.  char_index is allowed to be == text.size(), in which case get position past last character.
Vec2f GLUIText::getCharPos(GLUI& /*glui*/, int char_index) const
{
	const float x_scale = 2.f / opengl_engine->getMainViewPortWidth();

	if(char_index >= 0 && char_index < (int)char_positions_fc.size())
	{
		return botleft + Vec2f(char_positions_fc[char_index].pos.x * x_scale, 0);
	}
	else if(char_index >= (int)char_positions_fc.size())
	{
		if(char_positions_fc.empty())
			return botleft;
		else
			return botleft + Vec2f((char_positions_fc.back().pos.x + char_positions_fc.back().hori_advance) * x_scale, 0);
	}
	else
		return botleft;
}


int GLUIText::cursorPosForUICoords(GLUI& /*glui*/, const Vec2f& coords)
{
	const float x_scale = 2.f / opengl_engine->getMainViewPortWidth();

	int best_cursor_pos = 0;
	float closest_dist = 1000000;
	for(size_t i=0; i<char_positions_fc.size(); ++i)
	{
		Vec2f char_pos = botleft + Vec2f(char_positions_fc[i].pos.x * x_scale, 0);

		const float char_left_x = char_pos.x;
		const float dist = fabs(char_left_x - coords.x);
		if(dist < closest_dist)
		{
			best_cursor_pos = (int)i;
			closest_dist = dist;
		}

		if(char_pos.x < coords.x) // If character i left coord is to the left of the mouse click position
			best_cursor_pos = (int)i;
	}

	if(!char_positions_fc.empty())
	{
		Vec2f last_char_pos = botleft + Vec2f(char_positions_fc.back().pos.x * x_scale, 0);
		float last_char_hori_advance = char_positions_fc.back().hori_advance * x_scale;

		const float last_char_right_x = last_char_pos.x + last_char_hori_advance;
		const float dist = fabs(last_char_right_x - coords.x);
		if(dist < closest_dist)
		{
			best_cursor_pos = (int)char_positions_fc.size();
			assert(best_cursor_pos == UTF8Utils::numCodePointsInString(text));
			closest_dist = dist;
		}
	}

	return best_cursor_pos;
}


void GLUIText::setClipRegion(const Rect2f& clip_region_rect)
{
	if(overlay_ob.nonNull())
		overlay_ob->clip_region = clip_region_rect;
}


void GLUIText::setVisible(bool visible)
{
	if(overlay_ob.nonNull())
		overlay_ob->draw = visible;
}
