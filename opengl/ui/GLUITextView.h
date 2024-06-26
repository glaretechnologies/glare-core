/*=====================================================================
GLUITextView.h
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
GLUITextView
------------
A widget that displays some text
=====================================================================*/
class GLUITextView : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();
		std::string tooltip;
		Colour3f background_colour; // Linear
		float background_alpha;
		Colour3f text_colour; // Linear
		float text_alpha;
		int padding_px;
		int font_size_px; // default = 14

		float line_0_x_offset; // X offset of line 0 from botleft. [0, max_width].  Subsequent lines don't have this offset.
		float max_width; // Max width in UI coords.  Text will word-wrap to not be wider than this.
	};

	GLUITextView(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const std::string& text, const Vec2f& botleft, const CreateArgs& args);
	~GLUITextView();
	

	void setText(GLUI& glui, const std::string& new_text);

	void setColour(const Colour3f& col);
	void setAlpha(float alpha);
	void setBackgroundAlpha(float alpha);

	void setPos(GLUI& glui, const Vec2f& botleft);

	void setClipRegion(const Rect2f& rect);

	void setVisible(bool visible);

	virtual bool isVisible() override;

	//const Vec2f getDims() const;

	const Rect2f getRect() const;

	void viewportResized();

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void handleMouseRelease(MouseEvent& event) override;
	virtual void handleMouseDoubleClick(MouseEvent& event) override;
	virtual void doHandleMouseMoved(MouseEvent& event) override;
	virtual void handleLosingKeyboardFocus() override;
	virtual void updateGLTransform(GLUI& glui) override; // Called when e.g. the viewport changes size

	virtual void handleCutEvent(std::string& clipboard_contents_out) override;
	virtual void handleCopyEvent(std::string& clipboard_contents_out) override;

private:
	GLARE_DISABLE_COPY(GLUITextView);

	void updateOverlayObTransforms();

	GLUI* glui;
	Reference<OpenGLEngine> opengl_engine;
	OverlayObjectRef background_overlay_ob;
	std::vector<GLUITextRef> glui_texts;

	OverlayObjectRef selection_overlay_ob;

	int selection_start, selection_end;
	size_t selection_start_text_index, selection_end_text_index; // TODO: Do multi-line select

	CreateArgs args;
	std::string text;
	bool visible;

	Vec2f botleft; // in GL UI coords
};


typedef Reference<GLUITextView> GLUITextViewRef;
