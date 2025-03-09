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
	mouseover_background_colour = toLinearSRGB(Colour3f(0.8f));

	text_colour           = toLinearSRGB(Colour3f(0.1f));
	mouseover_text_colour = toLinearSRGB(Colour3f(0.1f));
}


GLUITextButton::GLUITextButton(GLUI& glui_, Reference<OpenGLEngine>& opengl_engine_, const std::string& button_text, const Vec2f& botleft, const Vec2f& /*dims*/, const CreateArgs& args_)
:	handler(NULL)
{
	glui = &glui_;
	opengl_engine = opengl_engine_;
	tooltip = args_.tooltip;
	args = args_;

	GLUITextView::CreateArgs text_args;
	text_args.background_colour = args.background_colour;
	text_args.text_colour = args.text_colour;
	text_args.padding_px = 8;
	text_args.text_selectable = false;
	text_args.font_size_px = args.font_size_px;
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
		text_view->setBackgroundColour(args.mouseover_background_colour);
		text_view->setTextColour(args.mouseover_text_colour);

		mouse_event.accepted = true;
	}
	else
	{
		text_view->setBackgroundColour(args.background_colour);
		text_view->setTextColour(args.text_colour);
	}
}


void GLUITextButton::updateGLTransform(GLUI& gl_ui)
{
	text_view->updateGLTransform(gl_ui);

	this->rect = text_view->getBackgroundRect();
}


void GLUITextButton::setPos(const Vec2f& botleft)
{
	//Vec2f extra = text_vuiewgetRect
	text_view->setPos(*glui, botleft + Vec2f(text_view->getPaddingWidth()));

	rect = text_view->getBackgroundRect();
}


void GLUITextButton::setVisible(bool visible)
{
	text_view->setVisible(visible);
}


bool GLUITextButton::isVisible()
{
	return text_view->isVisible();
}
