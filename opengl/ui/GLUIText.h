/*=====================================================================
GLUIText.h
----------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "../OpenGLEngine.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/Rect2.h"
#include "../graphics/colour3.h"
#include <string>
#include <vector>


class OpenGLEngine;
class OpenGLMeshRenderData;
class OpenGLTexture;
struct OverlayObject;
class GLUI;
class FontCharTexCache;
class TextRendererFontFaceSizeSet;
namespace glare { class StackAllocator; }


/*=====================================================================
GLUIText
--------
Displays some text in the opengl engine.
Just draws text, not any background behind it.
Can draw multiple lines of text.
=====================================================================*/
class GLUIText final : public RefCounted
{
public:
	struct CreateArgs
	{
		CreateArgs() : colour(1.f), alpha(1.f), font_size_px(14), z(0) {}

		Colour3f colour;
		float alpha;
		float z; // -1 = near clip plane, 1 = far clip plane
		int font_size_px; // In device-independent pixels. (e.g. will be multiplied by device pixel ratio to get actual pixel size)
	};

	// botleft is in GL UI coords (see GLUI.h)
	GLUIText(GLUI& glui, const std::string& text, const Vec2f& line_0_baseline_pos, const CreateArgs& args);
	~GLUIText(); // Removes overlay_ob from opengl engine.


	struct CharPositionInfo
	{
		Vec2f pos;
		float hori_advance;
	};

	// line_spacing_px is the distance between the baselines of consecutive lines, in font pixels (before scaling by vert_pos_scale).
	static Reference<OpenGLMeshRenderData> makeMeshDataForText(OpenGLEngine* opengl_engine, FontCharTexCache* font_char_text_cache,
		TextRendererFontFaceSizeSet* fonts, TextRendererFontFaceSizeSet* emoji_fonts, const std::string& text, const int font_size_px, float line_spacing_px, float vert_pos_scale, bool render_SDF,
		glare::StackAllocator& stack_allocator,
		Rect2f& rect_os_out, Rect2f& line_box_rect_os_out, Reference<OpenGLTexture>& atlas_texture_out, std::vector<CharPositionInfo>& char_positions_font_coords_out); // line_box_rect_os_out: see getLineBoxRect().


	void setColour(const Colour3f& col);
	void setAlpha(float alpha);

	void setLine0BaselinePos(const Vec2f& botleft);
	Vec2f getLine0BaselinePos() const { return line_0_baseline_pos; }

	void setZ(float new_z);

	// Call when e.g. viewport has changed
	void updateGLTransform();

	// Get position to lower left of unicode character with given index.  char_index is allowed to be == text.size(), in which case get position past last character.
	Vec2f getCharPos(GLUI& glui, int char_index) const; 

	// Get position relative to bottom left of text.
	Vec2f getRelativeCharPos(GLUI& glui, int char_index) const; 

	int cursorPosForUICoords(GLUI& glui, const Vec2f& coords);

	// Returns a rectangle around the text, in GL UI coords, with a height that depends only on the font size and the number of lines, not on which characters are in the text.
	// It goes from the baseline of the last line, up to the cap height above the baseline of the first line, so descenders, and any parts of characters above the cap height such as accents, extend outside it.
	// For placing text in a box with even padding.
	Rect2f getLineBoxRect() const;

	// Moves the text so that the bottom-left of getLineBoxRect() is at line_box_botleft (in GL UI coords).  E.g. to place multi-line text in a box with even padding.
	void setLineBoxBotLeftPos(const Vec2f& line_box_botleft);

	const std::string& getText() const { return text; }

	void setClipRegion(const Rect2f& clip_region_rect);

	void setVisible(bool visible);

	void setGLUI(GLUI* new_glui) { gl_ui = new_glui; } // Set on unremoved widgets when gl_ui is about to be destroyed, so that glui is not a dangling pointer to gl_ui.
	void setOpenGLEngine(OpenGLEngine* new_opengl_engine) { opengl_engine = new_opengl_engine; } // Set on unremoved widgets when gl_ui is about to be destroyed, so we don't have a dangling pointer to opengl_engine.

private:
	GLARE_DISABLE_COPY(GLUIText);

	GLUI* gl_ui;
	OpenGLEngine* opengl_engine;
	Reference<OverlayObject> overlay_ob;

	std::string text;
	Colour3f text_colour;
	int font_size_px;

	Vec2f line_0_baseline_pos; // In GL UI coords
	float z;
	Rect2f rect_os; // In font pixel coords
	Rect2f line_box_rect_os; // In font pixel coords.  See getLineBoxRect().

	std::vector<CharPositionInfo> char_positions_fc; // char positions in font coordinates
};


typedef Reference<GLUIText> GLUITextRef;
