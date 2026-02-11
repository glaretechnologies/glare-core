/*=====================================================================
GLUIGridContainer.h
-------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
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
GLUIGridContainer
-----------------
A grid of widgets.
=====================================================================*/
class GLUIGridContainer : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();
		
		Colour3f background_colour; // Linear
		float background_alpha;
		float z;

		float cell_padding_px; // Default is 10 px.

		bool background_consumes_events; // Should the background behind the grid consume click events etc.?
	};

	GLUIGridContainer(GLUI& glui, Reference<OpenGLEngine>& opengl_engine, const CreateArgs& args);
	virtual ~GLUIGridContainer();

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

	virtual bool acceptsTextInput() override { return false; }

	void setPosAndDims(const Vec2f& botleft, const Vec2f& dims) override;
	void setClipRegion(const Rect2f& rect) override;

	void setCellWidget(int cell_x, int cell_y, GLUIWidgetRef widget);

	void clear(); // Remove all widgets from grid, resize cell_widgets to zero.

	float getCellPaddding() const; // in UI coords

	Rect2f getClippedContentRect() const; // Get the rectangle around any non-null widgets in the cells, intersected with the container rectangle (in case of overflow).

private:
	GLARE_DISABLE_COPY(GLUIGridContainer);
	
	GLUI* gl_ui;
	Reference<OpenGLEngine> opengl_engine;

	CreateArgs args;

public:
	Array2D<GLUIWidgetRef> cell_widgets; // (0, 0) is the bottom left cell.
private:
	OverlayObjectRef background_overlay_ob;
};


typedef Reference<GLUIGridContainer> GLUIGridContainerRef;
