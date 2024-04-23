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
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/Rect2.h"
#include "../graphics/colour3.h"
#include <string>


class GLUI;


/*=====================================================================
GLUILineEdit
------------
A single line text edit field.
=====================================================================*/
class GLUILineEdit : public GLUIWidget
{
public:
	GLUILineEdit();
	~GLUILineEdit();

	struct GLUILineEditCreateArgs
	{
		GLUILineEditCreateArgs();
		std::string tooltip;
		Colour3f background_colour; // Linear
		float background_alpha;
		Colour3f text_colour; // Linear
		float text_alpha;
		int padding_px;
		int font_size_px; // default = 20

		float width; // In GL UI coords
	};
	void create(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const Vec2f& botleft, const GLUILineEditCreateArgs& args);

	void destroy();

	void setText(GLUI& glui, const std::string& new_text);

	//void setPos(GLUI& glui, const Vec2f& botleft);

	void setVisible(bool visible);

	const Vec2f getDims() const;

	virtual bool doHandleMouseClick(const Vec2f& coords) override;

	virtual void dohandleKeyPressedEvent(const KeyEvent& key_event) override;

	// Called when e.g. the viewport changes size
	virtual void updateGLTransform(GLUI& glui) override;

	GLUI* glui;
	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef background_overlay_ob;

	OverlayObjectRef cursor_overlay_ob;

	GLUITextRef glui_text;

private:
	GLARE_DISABLE_COPY(GLUILineEdit);

	void updateOverlayObTransforms(GLUI& glui);

	GLUILineEditCreateArgs args;
	std::string text;

	Vec2f botleft; // in GL UI coords
	Rect2f rect;
};


typedef Reference<GLUILineEdit> GLUILineEditRef;
