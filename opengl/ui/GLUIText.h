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


/*=====================================================================
GLUIText
--------

=====================================================================*/
class GLUIText : public RefCounted
{
public:
	// botleft is in GL UI coords (see GLUI.h)
	struct GLUITextCreateArgs
	{
		GLUITextCreateArgs() : colour(1.f), alpha(1.f), font_size_px(14), z(0) {}

		Colour3f colour;
		float alpha;
		float z; // -1 = near clip plane, 1 = far clip plane
		int font_size_px;
	};

	GLUIText(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& text, const Vec2f& botleft, const GLUITextCreateArgs& args);
	~GLUIText();

	void destroy(); // Called by destructor

	void setColour(const Colour3f& col);

	void setPos(const Vec2f& botleft);

	// Call when e.g. viewport has changed
	void updateGLTransform();

	Vec2f getCharPos(GLUI& glui, int char_index) const; // Get position to lower left of unicode character with given index.

	struct CharPositionInfo
	{
		Vec2f pos;
		float hori_advance;
	};
	std::vector<CharPositionInfo> getCharPositions(GLUI& glui) const; // Get position to lower left of each unicode character.

	int cursorPosForUICoords(GLUI& glui, const Vec2f& coords);

	Rect2f getRect() const { return rect; }

	const Vec2f getDims() const { return rect.getMax() - rect.getMin(); } // In GL UI coords

	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef overlay_ob;

private:
	GLARE_DISABLE_COPY(GLUIText);

	std::string text;
	Colour3f text_colour;
	int font_size_px;

	Vec2f botleft; // In GL UI coords
	float z;
	Rect2f rect; // In GL UI coords
	Rect2f rect_os; // In font pixel coords
};


typedef Reference<GLUIText> GLUITextRef;
