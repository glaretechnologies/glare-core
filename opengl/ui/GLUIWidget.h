/*=====================================================================
GLUIWidget.h
------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../OpenGLEngine.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../maths/Rect2.h"
#include <string>


class GLUI;
class MouseWheelEvent;
class KeyEvent;
class MouseEvent;
class TextInputEvent;


/*=====================================================================
GLUIWidget
----------

=====================================================================*/
class GLUIWidget : public RefCounted
{
public:
	GLUIWidget();
	virtual ~GLUIWidget();

	void handleMouseMoved(MouseEvent& mouse_event);
	virtual void handleMousePress(MouseEvent& /*event*/) {}
	virtual void handleMouseRelease(MouseEvent& /*event*/) {}
	virtual void handleMouseDoubleClick(MouseEvent& /*event*/) {}
	void handleMouseWheelEvent(MouseWheelEvent& event);
	void handleKeyPressedEvent(KeyEvent& key_event);
	void handleTextInputEvent(TextInputEvent& text_input_event);
	virtual void handleLosingKeyboardFocus() {}

	virtual void handleCutEvent(std::string& /*clipboard_contents_out*/) {}
	virtual void handleCopyEvent(std::string& /*clipboard_contents_out*/) {}

	virtual void doHandleMouseMoved(MouseEvent& /*mouse_event*/) {}
	virtual void doHandleMouseWheelEvent(MouseWheelEvent& /*event*/) {}
	virtual void doHandleKeyPressedEvent(KeyEvent& /*key_event*/) {}
	virtual void doHandleTextInputEvent(TextInputEvent& /*text_input_event*/) {}

	virtual bool isVisible() = 0;

	// Called when e.g. the viewport changes size
	virtual void updateGLTransform(GLUI& glui);

	virtual void think(GLUI& /*glui*/) {}

	virtual bool acceptsTextInput() { return false; }

	std::string client_data;

	std::string tooltip;

	Rect2f rect;
private:
	GLARE_DISABLE_COPY(GLUIWidget);
};


typedef Reference<GLUIWidget> GLUIWidgetRef;
