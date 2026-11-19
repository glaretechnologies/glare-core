/*=====================================================================
GLUIContextMenu.h
-----------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "GLUIWidget.h"
#include "../../graphics/colour3.h"
#include "../../utils/Reference.h"
#include <functional>
#include <string>
#include <vector>


class GLUI;
class GLUIGridContainer;


/*=====================================================================
GLUIContextMenu
---------------
A popup menu, such as is shown on right-click: an optional title, followed by a vertical list of items.
Items are drawn as plain text on the menu background, highlighted on mouseover.

Create the menu to show it, add it to the GLUI, and give it keyboard focus:
	menu = new GLUIContextMenu(*gl_ui, args, ui_coords);
	gl_ui->addWidget(menu);
	gl_ui->setKeyboardFocusWidget(menu);
To hide it, remove it from the GLUI and destroy it (checkRemoveAndDeleteWidget).

The menu also closes itself when it loses keyboard focus (e.g. when the user clicks elsewhere), when an item is clicked,
when Escape is pressed, or when the viewport is resized.  A closed menu has no contents and isVisible() returns false,
but it stays in the GLUI until the owner removes it.
=====================================================================*/
class GLUIContextMenu final : public GLUIWidget
{
public:
	struct CreateArgs
	{
		CreateArgs();

		std::string title; // No title row if empty.
		std::vector<std::string> items;

		Colour3f background_colour; // Linear
		Colour3f title_text_colour; // Linear
		Colour3f text_colour; // Linear
		Colour3f mouseover_background_colour; // Linear
		Colour3f pressed_background_colour; // Linear
		int font_size_px;
		float z;
	};

	// The menu's top-left corner is placed at top_left_ui_coords, moved if needed so that the menu stays on screen.
	// Tooltips are hidden while the menu is open, as they would otherwise be drawn over the menu.
	GLUIContextMenu(GLUI& glui, const CreateArgs& args, const Vec2f& top_left_ui_coords);
	~GLUIContextMenu();

	// Called when an item is clicked, after the menu has closed, with the index of the item in args.items.
	std::function<void(size_t item_index)> item_selected_func;

	virtual void handleMousePress(GLUIMouseEvent& event) override;
	virtual void handleMouseDoubleClick(GLUIMouseEvent& event) override;
	virtual void doHandleMouseMoved(GLUIMouseEvent& event) override;
	virtual void doHandleMouseWheelEvent(GLUIMouseWheelEvent& event) override;
	virtual void doHandleKeyPressedEvent(KeyEvent& key_event) override;
	virtual void handleLosingKeyboardFocus() override;

	virtual void setPos(const Vec2f& botleft) override;
	virtual void setClipRegion(const Rect2f& clip_rect) override;
	virtual void setZ(float new_z) override;
	virtual void setVisible(bool visible) override; // Setting invisible closes the menu.
	virtual bool isVisible() override; // False once the menu has closed.
	virtual void viewportResized() override; // Closes the menu.

	virtual std::string className() const override { return "GLUIContextMenu"; }

private:
	GLARE_DISABLE_COPY(GLUIContextMenu);

	void itemClicked(size_t item_index);
	void close();

	CreateArgs args;

	Reference<GLUIGridContainer> grid; // Contains the title and item widgets.  Null once the menu has closed.
};


typedef Reference<GLUIContextMenu> GLUIContextMenuRef;
