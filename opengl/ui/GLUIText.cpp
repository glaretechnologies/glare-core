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


GLUIText::GLUIText(GLUI& glui, Reference<OpenGLEngine>& opengl_engine_, const std::string& text_, const Vec2f& botleft_, const GLUITextCreateArgs& args)
{
	opengl_engine = opengl_engine_;

	text_colour = args.colour;
	font_size_px = args.font_size_px;
	botleft = botleft_;
	text = text_;
	z = args.z;

	try
	{
		rect_os = Rect2f(Vec2(0.f), Vec2(0.f));

		//TextRendererFontFace* font       = glui.getFont(args.font_size_px, /*emoji=*/false);
		//TextRendererFontFace* emoji_font = glui.getFont(args.font_size_px, /*emoji=*/true);

		// Make mesh data
		const size_t num_codepoints = UTF8Utils::numCodePointsInString(text);

		js::Vector<Vec3f, 16> verts(num_codepoints * 4);
		js::Vector<Vec2f, 16> uvs(num_codepoints * 4);
		js::Vector<uint32, 16> indices(num_codepoints * 6); // num_codepoints * 2 tris/code point * 3 indices/tri

		OpenGLTextureRef atlas_texture;
		size_t cur_string_byte_i = 0;
		Vec3f cur_char_pos(0.f);
		for(size_t i=0; i<num_codepoints; ++i)
		{
			runtimeCheck(cur_string_byte_i < text.size());
			const size_t char_num_bytes = UTF8Utils::numBytesForChar(text[cur_string_byte_i]);
			const string_view substring(text.data() + cur_string_byte_i, char_num_bytes);
			const CharTexInfo info = glui.font_char_text_cache->getCharTexture(opengl_engine_, glui.getFonts(), glui.getEmojiFonts(), substring, font_size_px);
			atlas_texture = info.tex;
		

			// NOTE: min and max y texcoords are interchanged with increasing y downwards, due to texture effectively being flipped vertically when loaded into OpenGL.
			uvs[i*4 + 0] = Vec2f(info.atlas_min_texcoords.x, info.atlas_max_texcoords.y); // bot left vertex
			uvs[i*4 + 1] = Vec2f(info.atlas_min_texcoords.x, info.atlas_min_texcoords.y); // top left vertex
			uvs[i*4 + 2] = Vec2f(info.atlas_max_texcoords.x, info.atlas_min_texcoords.y); // top right vertex
			uvs[i*4 + 3] = Vec2f(info.atlas_max_texcoords.x, info.atlas_max_texcoords.y); // bot right vertex


			const Vec3f min_pos = cur_char_pos + Vec3f((float)info.size_info.min_bounds.x, (float)info.size_info.min_bounds.y, 0);
			const Vec3f max_pos = cur_char_pos + Vec3f((float)info.size_info.max_bounds.x, (float)info.size_info.max_bounds.y, 0);
		
			verts[i*4 + 0] = min_pos;                                // bot left vertex
			verts[i*4 + 1] = Vec3f(min_pos.x, max_pos.y, min_pos.z); // top left vertex
			verts[i*4 + 2] = max_pos;                                // top right vertex
			verts[i*4 + 3] = Vec3f(max_pos.x, min_pos.y, min_pos.z); // bot right vertex

			cur_char_pos.x += info.size_info.hori_advance;

			if(i == 0)
				rect_os = Rect2f(toVec2f(min_pos), toVec2f(max_pos));
			else
			{
				rect_os.enlargeToHoldPoint(toVec2f(min_pos));
				rect_os.enlargeToHoldPoint(toVec2f(max_pos));
			}

			indices[i*6 + 0] = (uint32)i*4 + 0;
			indices[i*6 + 1] = (uint32)i*4 + 1;
			indices[i*6 + 2] = (uint32)i*4 + 2;
			indices[i*6 + 3] = (uint32)i*4 + 0;
			indices[i*6 + 4] = (uint32)i*4 + 2;
			indices[i*6 + 5] = (uint32)i*4 + 3;

			cur_string_byte_i += char_num_bytes;
		}

		Reference<OpenGLMeshRenderData> mesh_data = GLMeshBuilding::buildMeshRenderData(*opengl_engine->vert_buf_allocator, verts, uvs, indices);
	

		overlay_ob = new OverlayObject();
		overlay_ob->mesh_data = mesh_data;
		overlay_ob->material.albedo_linear_rgb = args.colour;
		overlay_ob->material.alpha = args.alpha;
		overlay_ob->material.albedo_texture = atlas_texture;

	

		const float x_scale = 2.f / opengl_engine->getMainViewPortWidth();
		const float y_scale = 2.f / opengl_engine->getMainViewPortHeight();

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
	destroy();
}


void GLUIText::destroy()
{
	if(overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(overlay_ob);
	overlay_ob = NULL;
	opengl_engine = NULL;
}


void GLUIText::setColour(const Colour3f& col)
{
	if(overlay_ob.nonNull())
		overlay_ob->material.albedo_linear_rgb = col;
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


Vec2f GLUIText::getCharPos(GLUI& glui, int char_index) const
{
	try
	{
		TextRendererFontFace* font       = glui.getFont(font_size_px, /*emoji=*/false);
		TextRendererFontFace* emoji_font = glui.getFont(font_size_px, /*emoji=*/true);

		size_t cur_string_byte_i = 0;
		Vec2f cur_char_pos(0.f);
		for(int i=0; i<char_index; ++i)
		{
			//runtimeCheck(cur_string_byte_i < text.size());
			if(cur_string_byte_i < text.size())
			{
				const size_t char_num_bytes = UTF8Utils::numBytesForChar(text[cur_string_byte_i]);
				const string_view charstring(text.data() + cur_string_byte_i, char_num_bytes);

				const uint32 code_point = UTF8Utils::codePointForUTF8CharString(charstring);
				const bool is_emoji = code_point >= 0x1F32; // TODO: improve this.  Emojis are not one continuous block, see https://en.wikipedia.org/wiki/Emoji#In_Unicode
				TextRendererFontFace* use_font = is_emoji ? emoji_font : font;

				const TextRendererFontFace::SizeInfo size_info = use_font->getTextSize(charstring);

				cur_char_pos.x += size_info.hori_advance;

				cur_string_byte_i += char_num_bytes;
			}
		}

		const float x_scale = 2.f / opengl_engine->getMainViewPortWidth();
		//const float y_scale = 2.f / opengl_engine->getMainViewPortHeight();

		return botleft + Vec2f(cur_char_pos.x * x_scale, 0.f);
	}
	catch(glare::Exception& e)
	{
		assert(0);
		conPrint("Warning: exception in GLUIText::getCharPos(): " + e.what());
		return botleft;
	}
}


std::vector<GLUIText::CharPositionInfo> GLUIText::getCharPositions(GLUI& glui) const // Get position to lower left of each unicode character.
{
	try
	{
		TextRendererFontFace* font       = glui.getFont(font_size_px, /*emoji=*/false);
		TextRendererFontFace* emoji_font = glui.getFont(font_size_px, /*emoji=*/true);

		const float x_scale = 2.f / opengl_engine->getMainViewPortWidth();

		std::vector<CharPositionInfo> positions;

		Vec2f cur_char_pos(0.f);
		for(size_t cur_string_byte_i=0; cur_string_byte_i < text.size(); )
		{
			const size_t char_num_bytes = UTF8Utils::numBytesForChar(text[cur_string_byte_i]);
			const string_view charstring(text.data() + cur_string_byte_i, char_num_bytes);

			const uint32 code_point = UTF8Utils::codePointForUTF8CharString(charstring);
			const bool is_emoji = code_point >= 0x1F32; // TODO: improve this.  Emojis are not one continuous block, see https://en.wikipedia.org/wiki/Emoji#In_Unicode
			TextRendererFontFace* use_font = is_emoji ? emoji_font : font;

			const TextRendererFontFace::SizeInfo size_info = use_font->getTextSize(charstring);

			CharPositionInfo pos_info;
			pos_info.pos = botleft + Vec2f(cur_char_pos.x * x_scale, 0.f);
			pos_info.hori_advance = size_info.hori_advance * x_scale;
			positions.push_back(pos_info);

			cur_char_pos.x += size_info.hori_advance;

			cur_string_byte_i += char_num_bytes;
		}

		return positions;
	}
	catch(glare::Exception& e)
	{
		assert(0);
		conPrint("Warning: exception in GLUIText::getCharPositions(): " + e.what());
		return std::vector<GLUIText::CharPositionInfo>();
	}
}
