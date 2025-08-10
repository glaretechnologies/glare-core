/*=====================================================================
GLUIInertWidget.h
-----------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
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
GLUIInertWidget
---------------
Just a widget that does nothing but accept clicks.
Is a solid colour.
=====================================================================*/
class GLUIInertWidget : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();
		
		Colour3f background_colour; // Linear
		float background_alpha;
		float z;
	};

	GLUIInertWidget(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const CreateArgs& args);
	virtual ~GLUIInertWidget();

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void handleMouseRelease(MouseEvent& event) override;
	virtual void handleMouseDoubleClick(MouseEvent& event) override;


	virtual void doHandleMouseMoved(MouseEvent& /*mouse_event*/) override;
	virtual void doHandleMouseWheelEvent(MouseWheelEvent& /*event*/) override;
	virtual void doHandleKeyPressedEvent(KeyEvent& /*key_event*/) override;

	virtual bool isVisible() override;
	virtual void setVisible(bool visible) override;

	// Called when e.g. the viewport changes size
	virtual void updateGLTransform(GLUI& glui) override;

	virtual bool acceptsTextInput() override { return false; }

	void setPosAndDims(const Vec2f& botleft, const Vec2f& dims);

private:
	GLARE_DISABLE_COPY(GLUIInertWidget);
	
	GLUI* gl_ui;
	Reference<OpenGLEngine> opengl_engine;

	CreateArgs args;

	OverlayObjectRef background_overlay_ob;
};


typedef Reference<GLUIInertWidget> GLUIInertWidgetRef;
