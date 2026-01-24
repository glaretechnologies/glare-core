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
	font_size_px = 14;

	background_colour           = Colour3f(1.f);
	toggled_background_colour   = toLinearSRGB(Colour3f(0.7f, 0.8f, 1.f));
	mouseover_background_colour = toLinearSRGB(Colour3f(0.8f));

	text_colour           = toLinearSRGB(Colour3f(0.1f));
	toggled_text_colour   = toLinearSRGB(Colour3f(0.1f));
	mouseover_text_colour = toLinearSRGB(Colour3f(0.1f));

	z = 0.f;
}


GLUITextButton::GLUITextButton(GLUI& glui_, Reference<OpenGLEngine>& opengl_engine_, const std::string& button_text_, const Vec2f& botleft, const CreateArgs& args_)
:	handler(NULL),
	toggled(false)
{
	glui = &glui_;
	opengl_engine = opengl_engine_;
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
	text_args.z = args.z;
	text_view = new GLUITextView(glui_, opengl_engine_, button_text, botleft, text_args);
	glui->addWidget(text_view);

	this->rect = text_view->getBackgroundRect();
}


GLUITextButton::~GLUITextButton()
{
	glui->removeWidget(text_view);
}


void GLUITextButton::handleMousePress(MouseEvent& event)
{
	if(!isVisible())
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inOpenRectangle(coords))
	{
		if(handler)
		{
			GLUICallbackEvent callback_event;
			callback_event.widget = this;
			handler->eventOccurred(callback_event);
			if(callback_event.accepted)
				event.accepted = true;
		}
	}
}


void GLUITextButton::doHandleMouseMoved(MouseEvent& mouse_event)
{
	const Vec2f coords = glui->UICoordsForOpenGLCoords(mouse_event.gl_coords);

	if(rect.inOpenRectangle(coords)) // If mouse over widget:
	{
		if(toggled)
		{
			text_view->setBackgroundColour(args.toggled_background_colour);
			text_view->setTextColour(args.toggled_text_colour);
		}
		else
		{
			text_view->setBackgroundColour(args.mouseover_background_colour);
			text_view->setTextColour(args.mouseover_text_colour);
		}

		mouse_event.accepted = true;
	}
	else
	{
		if(toggled)
		{
			text_view->setBackgroundColour(args.toggled_background_colour);
			text_view->setTextColour(args.toggled_text_colour);
		}
		else
		{
			text_view->setBackgroundColour(args.background_colour);
			text_view->setTextColour(args.text_colour);
		}
	}
}


void GLUITextButton::updateGLTransform()
{
	text_view->updateGLTransform();

	this->rect = text_view->getBackgroundRect();
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
	text_args.z = args.z;
	text_view = new GLUITextView(*glui, opengl_engine, button_text, m_botleft, text_args);
	text_view->setVisible(old_visible);
	glui->addWidget(text_view);

	this->rect = text_view->getBackgroundRect();
}


void GLUITextButton::setPos(const Vec2f& botleft)
{
	//Vec2f extra = text_vuiewgetRect
	text_view->setPos(botleft + Vec2f(text_view->getPaddingWidth()));

	rect = text_view->getBackgroundRect();
}


void GLUITextButton::setPosAndDims(const Vec2f& botleft, const Vec2f& dims)
{
	setPos(botleft);
}


void GLUITextButton::setClipRegion(const Rect2f& clip_rect)
{
	text_view->setClipRegion(clip_rect);
}


void GLUITextButton::setToggled(bool toggled_)
{
	this->toggled = toggled_;

	// NOTE: ignoring mouseover state
	text_view->setBackgroundColour(toggled ? args.toggled_background_colour : args.background_colour);
	text_view->setTextColour(toggled ? args.toggled_text_colour : args.text_colour);
}


void GLUITextButton::setVisible(bool visible)
{
	text_view->setVisible(visible);
}


bool GLUITextButton::isVisible()
{
	return text_view->isVisible();
}
