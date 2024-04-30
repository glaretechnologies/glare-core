/*=====================================================================
GLUILineEdit.h
-------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUICallbackHandler.h"
#include "GLUIText.h"
#include "../OpenGLEngine.h"
#include "../../utils/RefCounted.h"
#include "../../utils/Reference.h"
#include "../../maths/Rect2.h"
#include "../../graphics/colour3.h"
#include <string>


class GLUI;


/*=====================================================================
GLUILineEdit
------------
A single-line text edit field.
=====================================================================*/
class GLUILineEdit : public GLUIWidget
{
public:
	struct GLUILineEditCreateArgs
	{
		GLUILineEditCreateArgs();
		std::string tooltip;
		Colour3f background_colour; // Linear
		Colour3f mouseover_background_colour;
		float background_alpha;
		Colour3f text_colour; // Linear
		float text_alpha;
		int padding_px; // default = 10
		int font_size_px; // default = 14
		float rounded_corner_radius_px;

		float width; // In GL UI coords
	};

	GLUILineEdit(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const Vec2f& botleft, const GLUILineEditCreateArgs& args);
	~GLUILineEdit();

	void destroy(); // Called by destructor

	virtual void think(GLUI& glui) override;

	void setText(GLUI& glui, const std::string& new_text);
	const std::string& getText() const;

	void clear();

	void setVisible(bool visible);

	const Vec2f getDims() const;

	void setPos(const Vec2f& botleft);

	virtual bool doHandleMouseClick(const Vec2f& coords) override;
	virtual bool doHandleMouseMoved(const Vec2f& coords) override;
	virtual void doHandleKeyPressedEvent(KeyEvent& key_event) override;

	virtual void doHandleTextInputEvent(TextInputEvent& text_input_event) override;

	// Called when e.g. the viewport changes size
	virtual void updateGLTransform(GLUI& glui) override;

	virtual bool acceptsTextInput() override { return true; }


	std::function<void()> on_enter_pressed;

private:
	GLARE_DISABLE_COPY(GLUILineEdit);

	void updateOverlayObTransforms();
	void recreateTextWidget();
	void updateTextTransform();

	GLUI* glui;
	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef background_overlay_ob;
	Vec2i last_viewport_dims;

	OverlayObjectRef cursor_overlay_ob;

	GLUITextRef glui_text;

	GLUILineEditCreateArgs args;
	std::string text;
	int cursor_pos; // Index of unicode character/code point that cursor is to the left of.

	Vec2f botleft; // in GL UI coords

	float height_px;

	double last_cursor_update_time;
};


typedef Reference<GLUILineEdit> GLUILineEditRef;
