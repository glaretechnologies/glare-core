/*=====================================================================
GLUICollapsableGroupBox.h
-------------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "GLUITextView.h"
#include "GLUIButton.h"
#include "../OpenGLEngine.h"
#include "../utils/RefCounted.h"
#include "../utils/Reference.h"
#include "../utils/Array2D.h"
#include "../maths/Rect2.h"
#include <string>


class GLUI;
class MouseWheelEvent;
class KeyEvent;
class MouseEvent;
class TextInputEvent;


/*=====================================================================
GLUICollapsableGroupBox
-----------------------
A widget with a title and collapse/expand button.
Has a single contained body widget, as well as a background overlay object (flat colour rectangle).

Expanded:

-----------------------------------------
| v       TitleTextWidget               |
| ------------------------------------- |
| |                                   | | 
| |                                   | |
| |                                   | |
| |       body_widget                 | |
| |                                   | |
| |                                   | |
| |                                   | |
| |                                   | |
| |___________________________________| |
|_______________________________________|

Collapsed:

-----------------------------------------
| >       TitleTextWidget               |
-----------------------------------------

=====================================================================*/
class GLUICollapsableGroupBox final : public GLUIWidget, public GLUICallbackHandler
{
public:
	struct CreateArgs
	{
		CreateArgs();

		std::string title;
		
		Colour3f title_text_colour; // Linear
		Colour3f background_colour; // Linear
		float background_alpha;
		float z;

		float padding_px; // left, right, bottom padding/margin around body_widget, in pixels.

		bool background_consumes_events; // Should the background around the body widget consume click events etc.?  Defaults to false.
	};

	GLUICollapsableGroupBox(GLUI& glui, const CreateArgs& args);
	virtual ~GLUICollapsableGroupBox();

	void setBodyWidget(const GLUIWidgetRef body_widget);

	virtual void handleMousePress(MouseEvent& event) override;
	virtual void handleMouseRelease(MouseEvent& event) override;
	virtual void handleMouseDoubleClick(MouseEvent& event) override;


	virtual void doHandleMouseMoved(MouseEvent& /*mouse_event*/) override;
	virtual void doHandleMouseWheelEvent(MouseWheelEvent& /*event*/) override;
	virtual void doHandleKeyPressedEvent(KeyEvent& /*key_event*/) override;

	virtual bool isVisible() override;
	virtual void setVisible(bool visible) override;

	// Called when e.g. the viewport changes size
	virtual void updateGLTransform() override;

	virtual void recomputeLayout() override; // For containers - call recursively on contained widgets and then place each contained widget at final location.

	virtual bool acceptsTextInput() override { return false; }

	virtual void setPos(const Vec2f& botleft) override;
	void setPosAndDims(const Vec2f& botleft, const Vec2f& dims) override;
	void setClipRegion(const Rect2f& rect) override;

	virtual void setZ(float new_z) override;

	virtual void containedWidgetChangedSize() override; // For containers - a widget in the container has changed size (e.g. group box collapsed or expanded), so a relayout is probably needed.

	virtual void eventOccurred(GLUICallbackEvent& /*event*/) override; // From GLUICallbackHandler

	GLUICallbackHandler* handler; // For close event

private:
	GLARE_DISABLE_COPY(GLUICollapsableGroupBox);

	void updateWidgetTransforms();
	void updateBackgroundOverlayTransform();
	
	CreateArgs args;

	GLUITextViewRef title_text;
	GLUIButtonRef collapse_expand_button;
	GLUIWidgetRef body_widget;
	
	OverlayObjectRef background_overlay_ob;

	bool expanded;
};


typedef Reference<GLUICollapsableGroupBox> GLUICollapsableGroupBoxRef;
