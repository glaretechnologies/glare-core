/*=====================================================================
GLUIGridContainer.h
-------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "../graphics/colour3.h"
#include "../utils/Array2D.h"


struct OverlayObject;
class GLUI;
class MouseWheelEvent;
class KeyEvent;
class MouseEvent;
class TextInputEvent;


/*=====================================================================
GLUIGridContainer
-----------------
A grid of widgets.
(0, 0) is top left widget.
=====================================================================*/
class GLUIGridContainer final : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();
		
		Colour3f background_colour; // Linear
		float background_alpha;
		float z;

		float interior_cell_x_padding_px; // Default is 5 px.
		float interior_cell_y_padding_px; // Default is 5 px.
		float exterior_cell_x_padding_px; // Default is 0 px.
		float exterior_cell_y_padding_px; // Default is 0 px.

		bool background_consumes_events; // Should the background behind the grid consume click events etc.?  Defaults to false.
	};

	GLUIGridContainer(GLUI& glui, const CreateArgs& args);
	virtual ~GLUIGridContainer();

	virtual void handleMousePress(GLUIMouseEvent& event) override;
	virtual void handleMouseDoubleClick(GLUIMouseEvent& event) override;


	virtual void doHandleMouseMoved(GLUIMouseEvent& /*mouse_event*/) override;
	virtual void doHandleMouseWheelEvent(GLUIMouseWheelEvent& /*event*/) override;
	virtual void doHandleKeyPressedEvent(KeyEvent& /*key_event*/) override;

	virtual bool isVisible() override;
	virtual void setVisible(bool visible) override;

	virtual void viewportResized() override;

	virtual void recomputeLayout() override; // For grid containers - call recursively on contained widgets and then place each contained widget at final location.

	virtual bool acceptsTextInput() override { return false; }

	virtual Vec2f getMinDims() const override; // Return the natural or minimum dimensions of the widget.

	virtual void setPos(const Vec2f& botleft) override;
	void setClipRegion(const Rect2f& rect) override;

	virtual void setZ(float new_z) override;

	void setCellWidget(int cell_x, int cell_y, GLUIWidgetRef widget);

	bool isInserted(GLUIWidgetRef widget) const;

	// Removes the widget from its cell, if it is in the grid, and clears its parent.  Doesn't remove the widget from the GLUI.
	void removeWidget(GLUIWidgetRef widget);

	void addWidgetOnNewRow(GLUIWidgetRef widget); // Adds new row at bottom of grid.

	void clear(); // Remove all widgets from grid, resize cell_widgets to zero.

	virtual void containedWidgetChangedSize() override; // For containers - a widget in the container has changed size (e.g. group box collapsed or expanded), so a relayout is probably needed.

	virtual std::string className() const override { return "GLUIGridContainer"; }

	//float getCellPaddding() const; // in UI coords

	Rect2f getClippedContentRect() const; // Get the rectangle around any non-null widgets in the cells, intersected with the container rectangle (in case of overflow).

	// Set the minimium x pixel value (from left of grid) for the given column
	void setColumnMinXPx(int column_i, float col_min_x_px);
private:
	GLARE_DISABLE_COPY(GLUIGridContainer);

	void updateBackgroundOverlayTransform();
	
	CreateArgs args;
public:
	Array2D<GLUIWidgetRef> cell_widgets;
private:
	Reference<OverlayObject> background_overlay_ob;

	std::vector<float> col_min_x_px_vals;
};


typedef Reference<GLUIGridContainer> GLUIGridContainerRef;
