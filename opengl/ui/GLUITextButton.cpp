/*=====================================================================
GLUITextButton.cpp
------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "GLUITextButton.h"


#include "GLUI.h"
#include <graphics/SRGBUtils.h>
#include "../OpenGLMeshRenderData.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


GLUITextButton::CreateArgs::CreateArgs()
{
	sizing_type_x = GLUIWidget::SizingType::SizingType_FixedSizePx;
	sizing_type_y = GLUIWidget::SizingType::SizingType_FixedSizePx;
	fixed_size = Vec2f(100, std::ceil(GLUI::getDefaultFontSizePx() * 2.4f));

	font_size_px = GLUI::getDefaultFontSizePx();

	background_colour           = Colour3f(1.f);
	toggled_background_colour   = toLinearSRGB(Colour3f(0.7f, 0.8f, 1.f));
	mouseover_background_colour = toLinearSRGB(Colour3f(0.8f));

	text_colour           = toLinearSRGB(Colour3f(0.1f));
	toggled_text_colour   = toLinearSRGB(Colour3f(0.1f));
	mouseover_text_colour = toLinearSRGB(Colour3f(0.1f));

	pressed_background_colour = toLinearSRGB(Colour3f(0.6f));
	pressed_text_colour       = toLinearSRGB(Colour3f(0.1f));

	z = 0.f;
}


GLUITextButton::GLUITextButton(GLUI& glui_, const std::string& button_text_, const Vec2f& botleft, const CreateArgs& args_)
:	handler(NULL),
	toggled(false),
	pressed(false),
	colour_state(ColourState_Normal)
{
	glui = &glui_;
	opengl_engine = glui_.opengl_engine.ptr();
	tooltip = args_.tooltip;
	args = args_;
	m_z = args_.z;
	m_botleft = botleft;
	button_text = button_text_;

	GLUITextView::CreateArgs text_args;
	text_args.background_colour = args.background_colour;
	text_args.text_colour = args.text_colour;
	text_args.padding_px = 8;
	text_args.text_selectable = false;
	text_args.font_size_px = args.font_size_px;
	text_args.z = args_.z;
	text_view = new GLUITextView(glui_, button_text, botleft, text_args);
	glui->addWidget(text_view);

	this->rect = text_view->getRect();// getBackgroundRect();

	this->setFixedDimsPx(max(fixed_size, Vec2f(glui->getDevIndepPixelWidthForUIWidth(rect.getWidths().x), glui->getDevIndepPixelWidthForUIWidth(rect.getWidths().y))));
}


GLUITextButton::~GLUITextButton()
{
	checkRemoveAndDeleteWidget(glui, text_view);
}


void GLUITextButton::handleMousePress(MouseEvent& event)
{
	if(!isVisible())
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inOpenRectangle(coords))
	{
		pressed = true;
		updateColours(/*mouse_in_rect=*/true);

		if(handler)
		{
			GLUICallbackEvent callback_event;
			callback_event.widget = this;
			handler->eventOccurred(callback_event);
			if(callback_event.accepted)
				event.accepted = true;
		}

		if(handler_func)
		{
			GLUICallbackEvent callback_event;
			callback_event.widget = this;
			handler_func(callback_event);
			if(callback_event.accepted)
				event.accepted = true;
		}
	}
}


void GLUITextButton::handleMouseRelease(MouseEvent& event)
{
	if(pressed)
	{
		pressed = false;
		updateColours(rect.inOpenRectangle(glui->UICoordsForOpenGLCoords(event.gl_coords)));
	}
}


// The second click of a double-click arrives as a double-click event instead of a press event, so treat it as a press.
// A release event follows, which clears pressed.
void GLUITextButton::handleMouseDoubleClick(MouseEvent& event)
{
	handleMousePress(event);
}


void GLUITextButton::doHandleMouseMoved(MouseEvent& mouse_event)
{
	const Vec2f coords = glui->UICoordsForOpenGLCoords(mouse_event.gl_coords);

	const bool mouse_in_rect = rect.inOpenRectangle(coords);
	if(mouse_in_rect)
		mouse_event.accepted = true;

	updateColours(mouse_in_rect);
}


// The pressed colours take priority over the toggled colours, which take priority over the mouse-over colours.
// The button stays pressed if the mouse moves off it while the mouse button is held down, as with GLUIButton.
// Only sets the text view colours when the colour state changes, since this is called on every mouse move.
void GLUITextButton::updateColours(bool mouse_in_rect)
{
	const ColourState new_state =
		pressed ? ColourState_Pressed :
		(toggled ? ColourState_Toggled : 
		(mouse_in_rect ? ColourState_MouseOver : ColourState_Normal));

	if(new_state != colour_state)
	{
		colour_state = new_state;

		conPrint("GLUITextButton::updateColours: updating col...");

		switch(new_state)
		{
		case ColourState_Normal:
			text_view->setBackgroundColour(args.background_colour);
			text_view->setTextColour(args.text_colour);
			break;
		case ColourState_MouseOver:
			text_view->setBackgroundColour(args.mouseover_background_colour);
			text_view->setTextColour(args.mouseover_text_colour);
			break;
		case ColourState_Toggled:
			text_view->setBackgroundColour(args.toggled_background_colour);
			text_view->setTextColour(args.toggled_text_colour);
			break;
		case ColourState_Pressed:
			text_view->setBackgroundColour(args.pressed_background_colour);
			text_view->setTextColour(args.pressed_text_colour);
			break;
		}
	}
}


void GLUITextButton::viewportResized()
{
	text_view->viewportResized();

	this->rect = text_view->getRect(); // getBackgroundRect();
}


void GLUITextButton::rebuild()
{
	const bool old_visible = text_view->isVisible();

	glui->removeWidget(text_view);
	text_view = nullptr;

	GLUITextView::CreateArgs text_args;
	text_args.background_colour = args.background_colour;
	text_args.text_colour = args.text_colour;
	text_args.padding_px = 8;
	text_args.text_selectable = false;
	text_args.font_size_px = args.font_size_px;
	text_args.z = m_z;
	text_view = new GLUITextView(*glui, button_text, m_botleft, text_args);
	text_view->setVisible(old_visible);
	glui->addWidget(text_view);

	this->rect = text_view->getRect(); // getBackgroundRect();

	// The new text view has the normal colours.  Reapply the colours for the current state.
	colour_state = ColourState_Normal;
	updateColours(rect.inOpenRectangle(glui->getLastMouseUICoords()));
}


void GLUITextButton::setText(const std::string& new_text)
{
	if(new_text == button_text)
		return;

	button_text = new_text;
	text_view->setText(button_text);

	this->rect = text_view->getRect();

	this->setFixedDimsPx(max(args.fixed_size, Vec2f(glui->getDevIndepPixelWidthForUIWidth(rect.getWidths().x), glui->getDevIndepPixelWidthForUIWidth(rect.getWidths().y))));
}


Vec2f GLUITextButton::getMinDims() const
{
	return text_view->getMinDims();
}


void GLUITextButton::setPos(const Vec2f& botleft)
{
	//Vec2f extra = text_vuiewgetRect
	text_view->setPos(botleft/* + Vec2f(text_view->getPaddingWidth())*/);

	rect = text_view->getRect(); // getBackgroundRect();
}


void GLUITextButton::setAvailableRegionDims(const Vec2f& available_dims)
{
	Vec2f dims = this->getDims();

	if(sizing_type_x == SizingType_Expanding)
		dims.x = myMax(dims.x, available_dims.x);
	if(sizing_type_y == SizingType_Expanding)
		dims.y = myMax(dims.y, available_dims.y);

	rect = Rect2f::fromMinAndSpan(this->getRect().getMin(), dims);
	text_view->setFixedDimsUICoords(dims);
}


void GLUITextButton::setZ(float new_z)
{
	this->m_z = new_z;

	text_view->setZ(new_z);
}


void GLUITextButton::setClipRegion(const Rect2f& clip_rect)
{
	text_view->setClipRegion(clip_rect);
}


void GLUITextButton::setToggled(bool toggled_)
{
	this->toggled = toggled_;

	updateColours(rect.inOpenRectangle(glui->getLastMouseUICoords()));
}


void GLUITextButton::setVisible(bool visible)
{
	text_view->setVisible(visible);
}


bool GLUITextButton::isVisible()
{
	return text_view->isVisible();
}
