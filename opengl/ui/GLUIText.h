/*=====================================================================
GLUIText.h
----------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUICallbackHandler.h"
#include "../OpenGLEngine.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/Rect2.h"
#include "../graphics/colour3.h"
#include <string>
#include <vector>


class GLUI;
class FontCharTexCache;
class TextRendererFontFaceSizeSet;


/*=====================================================================
GLUIText
--------

=====================================================================*/
class GLUIText : public RefCounted
{
public:
	// botleft is in GL UI coords (see GLUI.h)
	struct CreateArgs
	{
		CreateArgs() : colour(1.f), alpha(1.f), font_size_px(14), z(0) {}

		Colour3f colour;
		float alpha;
		float z; // -1 = near clip plane, 1 = far clip plane
		int font_size_px;
	};

	GLUIText(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& text, const Vec2f& botleft, const CreateArgs& args);
	~GLUIText(); // Removes overlay_ob from opengl engine.


	struct CharPositionInfo
	{
		Vec2f pos;
		float hori_advance;
	};

	static Reference<OpenGLMeshRenderData> makeMeshDataForText(Reference<OpenGLEngine>& opengl_engine, FontCharTexCache* font_char_text_cache, 
		TextRendererFontFaceSizeSet* fonts, TextRendererFontFaceSizeSet* emoji_fonts, const std::string& text, const int font_size_px, float vert_pos_scale, bool render_SDF, 
		Rect2f& rect_os_out, OpenGLTextureRef& atlas_texture_out, std::vector<CharPositionInfo>& char_positions_font_coords_out);


	void setColour(const Colour3f& col);
	void setAlpha(float alpha);

	void setPos(const Vec2f& botleft);

	// Call when e.g. viewport has changed
	void updateGLTransform();

	// Get position to lower left of unicode character with given index.  char_index is allowed to be == text.size(), in which case get position past last character.
	Vec2f getCharPos(GLUI& glui, int char_index) const; 

	// Get position relative to bottom left of text.
	Vec2f getRelativeCharPos(GLUI& glui, int char_index) const; 

	int cursorPosForUICoords(GLUI& glui, const Vec2f& coords);

	Rect2f getRect() const { return rect; }

	const Vec2f getDims() const { return rect.getMax() - rect.getMin(); } // In GL UI coords

	const std::string& getText() const { return text; }

	void setClipRegion(const Rect2f& clip_region_rect);

	void setVisible(bool visible);

private:
	GLARE_DISABLE_COPY(GLUIText);

	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef overlay_ob;

	std::string text;
	Colour3f text_colour;
	int font_size_px;

	Vec2f botleft; // In GL UI coords
	float z;
	Rect2f rect; // In GL UI coords
	Rect2f rect_os; // In font pixel coords

	std::vector<CharPositionInfo> char_positions_fc; // char positions in font coordinates
};


typedef Reference<GLUIText> GLUITextRef;
