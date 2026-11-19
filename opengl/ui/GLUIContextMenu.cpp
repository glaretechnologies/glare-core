/*=====================================================================
GLUIContextMenu.cpp
-------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "GLUIContextMenu.h"


#include "GLUI.h"
#include "GLUIGridContainer.h"
#include "GLUITextButton.h"
#include "GLUITextView.h"
#include "../../graphics/SRGBUtils.h"
#include "../../maths/mathstypes.h"


GLUIContextMenu::CreateArgs::CreateArgs()
:	background_colour(toLinearSRGB(Colour3f(0.95f))),
	title_text_colour(toLinearSRGB(Colour3f(0.35f))),
	text_colour(toLinearSRGB(Colour3f(0.1f))),
	mouseover_background_colour(toLinearSRGB(Colour3f(0.85f))),
	pressed_background_colour(toLinearSRGB(Colour3f(0.75f))),
	font_size_px(GLUI::getDefaultFontSizePx()),
	z(-0.9f) // In front of most widgets.  The GLUI tooltip is in front of this.
{}


GLUIContextMenu::GLUIContextMenu(GLUI& glui_, const CreateArgs& args_, const Vec2f& top_left_ui_coords)
{
	glui = &glui_;
	opengl_engine = glui_.opengl_engine.ptr();
	args = args_;
	m_z = args.z;

	GLUIGridContainer::CreateArgs grid_args;
	grid_args.background_colour = args.background_colour;
	grid_args.background_alpha = 1.f;
	grid_args.interior_cell_x_padding_px = 0;
	grid_args.interior_cell_y_padding_px = 0; // Items touch, so they read as a single list.
	grid_args.exterior_cell_x_padding_px = 4;
	grid_args.exterior_cell_y_padding_px = 4;
	grid = new GLUIGridContainer(*glui, grid_args);
	glui->addWidget(grid);

	int next_row = 0;

	if(!args.title.empty())
	{
		GLUITextView::CreateArgs title_args;
		title_args.background_alpha = 0;
		title_args.text_colour = args.title_text_colour;
		title_args.font_size_px = args.font_size_px;
		title_args.text_selectable = false;
		Reference<GLUITextView> title_view = new GLUITextView(*glui, args.title, title_args);
		glui->addWidget(title_view);
		grid->setCellWidget(/*x=*/0, /*y=*/next_row++, title_view);
	}

	GLUITextButton::CreateArgs button_args;
	button_args.background_colour = args.background_colour;
	button_args.mouseover_background_colour = args.mouseover_background_colour;
	button_args.pressed_background_colour = args.pressed_background_colour;
	button_args.text_colour = args.text_colour;
	button_args.mouseover_text_colour = args.text_colour;
	button_args.pressed_text_colour = args.text_colour;
	button_args.font_size_px = args.font_size_px;
	button_args.corner_radius_px = 0;
	button_args.border_width_px = 0;
	button_args.shadow_num_layers = 0;
	button_args.sizing_type_x = GLUIWidget::SizingType_Expanding; // Fill the menu width, so the mouseover highlights are all the same width.

	for(size_t i=0; i<args.items.size(); ++i)
	{
		Reference<GLUITextButton> button = new GLUITextButton(*glui, args.items[i], button_args);
		button->on_pressed = [this, i](GLUICallbackEvent& event) { event.accepted = true; this->itemClicked(i); };
		glui->addWidget(button);
		grid->setCellWidget(/*x=*/0, /*y=*/next_row++, button);
	}

	// The grid is just in front of this widget, and its contents are in front of the grid.
	// Clicks on the grid background (e.g. on the title) go to this widget, which keeps the menu open.
	grid->setZ(m_z - 0.001f);
	grid->recomputeLayout(); // Sets the grid dims from the contents.

	// Place the menu with its top-left corner at top_left_ui_coords, moving it so that it stays on screen.
	const Vec2f dims = grid->getDims();
	const float max_y = glui->getViewportMinMaxY();
	Vec2f botleft(top_left_ui_coords.x, top_left_ui_coords.y - dims.y);
	botleft.x = myMax(-1.f, myMin(botleft.x, 1.f - dims.x));
	botleft.y = myMin(max_y - dims.y, myMax(botleft.y, -max_y));
	setPos(botleft);

	glui->hideTooltip();
}


GLUIContextMenu::~GLUIContextMenu()
{
	close();
}


// Removes the menu contents, and gives up keyboard focus.  The empty menu stays in the GLUI until the owner removes it.
void GLUIContextMenu::close()
{
	if(!grid)
		return;

	glui->unhideTooltip();

	checkRemoveAndDeleteWidget(glui, grid); // The grid destructor removes the title and item widgets from the GLUI.

	rect = Rect2f(Vec2f(-1000.f), Vec2f(-1000.f)); // Offscreen, so the closed menu doesn't get mouse events.

	if(glui->getKeyboardFocusWidget().ptr() == this)
		glui->setKeyboardFocusWidget(NULL); // Calls handleLosingKeyboardFocus(), which calls close(), which returns early as grid is now null.
}


// Called from the clicked button's mouse press handler.
// The GLUI holds references to the button and to this menu (as the menu rect contains the click) while dispatching the event,
// so neither is destroyed if item_selected_func makes the owner remove and destroy the menu.
void GLUIContextMenu::itemClicked(size_t item_index)
{
	close();

	if(item_selected_func)
		item_selected_func(item_index);
}


// Accept mouse events on the menu, so they don't go through to whatever is behind it.
void GLUIContextMenu::handleMousePress(GLUIMouseEvent& event)
{
	if(grid && rect.inClosedRectangle(event.ui_coords))
		event.accepted = true;
}


void GLUIContextMenu::handleMouseDoubleClick(GLUIMouseEvent& event)
{
	handleMousePress(event);
}


void GLUIContextMenu::doHandleMouseMoved(GLUIMouseEvent& event)
{
	handleMousePress(event);
}


void GLUIContextMenu::doHandleMouseWheelEvent(GLUIMouseWheelEvent& event)
{
	if(grid && rect.inClosedRectangle(event.ui_coords))
		event.accepted = true;
}


void GLUIContextMenu::doHandleKeyPressedEvent(KeyEvent& key_event)
{
	if(key_event.key == Key::Key_Escape)
	{
		key_event.accepted = true;
		close();
	}
}


void GLUIContextMenu::handleLosingKeyboardFocus()
{
	close();
}


void GLUIContextMenu::setPos(const Vec2f& botleft)
{
	if(grid)
	{
		grid->setPos(botleft); // Moves the contained widgets as well.
		rect = grid->getRect();
	}
}


void GLUIContextMenu::setClipRegion(const Rect2f& /*clip_rect*/)
{
}


void GLUIContextMenu::setZ(float new_z)
{
	m_z = new_z;
	if(grid)
		grid->setZ(m_z - 0.001f); // Sets z for the contained widgets as well.
}


void GLUIContextMenu::setVisible(bool visible)
{
	if(!visible)
		close();
}


bool GLUIContextMenu::isVisible()
{
	return grid.nonNull();
}


void GLUIContextMenu::viewportResized()
{
	close();
}
