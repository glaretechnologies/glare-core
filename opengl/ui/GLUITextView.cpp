/*=====================================================================
GLUITextView.cpp
----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "GLUITextView.h"


#include "GLUI.h"
#include "../graphics/SRGBUtils.h"
#include "../OpenGLMeshRenderData.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


GLUITextView::GLUITextView(GLUI& glui, Reference<OpenGLEngine>& opengl_engine_, const std::string& text_, const Vec2f& botleft_, const GLUITextViewCreateArgs& args_)
{
	args = args_;
	opengl_engine = opengl_engine_;
	tooltip = args.tooltip;

	text = text_;
	botleft = botleft_;

	background_overlay_ob = new OverlayObject();
	background_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	background_overlay_ob->material.albedo_linear_rgb = args.background_colour;
	background_overlay_ob->material.alpha = args.background_alpha;

	GLUIText::GLUITextCreateArgs text_create_args;
	text_create_args.colour = args.text_colour;
	text_create_args.font_size_px = args.font_size_px;
	text_create_args.alpha = args.text_alpha;
	glui_text = new GLUIText(glui, opengl_engine_, text, botleft, text_create_args);

	rect = glui_text->getRect();

	updateBackgroundOverlayObTransform(glui);

	opengl_engine->addOverlayObject(background_overlay_ob);
}


GLUITextView::~GLUITextView()
{
	destroy();
}


void GLUITextView::destroy()
{
	if(glui_text.nonNull())
		glui_text->destroy();
	glui_text = NULL;

	if(background_overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(background_overlay_ob);
	background_overlay_ob = NULL;

	opengl_engine = NULL;
}


void GLUITextView::setText(GLUI& glui, const std::string& new_text)
{
	if(new_text != text)
	{
		text = new_text;

		// Re-create glui_text
		if(glui_text.nonNull())
			glui_text->destroy();

		GLUIText::GLUITextCreateArgs text_create_args;
		text_create_args.font_size_px = args.font_size_px;
		text_create_args.alpha = args.text_alpha;
		glui_text = new GLUIText(glui, opengl_engine, text, botleft, text_create_args);

		updateBackgroundOverlayObTransform(glui);
	}
}


void GLUITextView::updateBackgroundOverlayObTransform(GLUI& glui)
{
	if(background_overlay_ob.nonNull())
	{
		const float margin_x = glui.getUIWidthForDevIndepPixelWidth((float)args.padding_px);
		const float margin_y = margin_x;
		const float background_w = glui_text->getDims().x + margin_x*2;
		const float background_h = glui_text->getDims().y + margin_y*2;

		const Vec2f text_lower_left_pos = glui_text->getRect().getMin();
		const Vec2f lower_left_pos = text_lower_left_pos - Vec2f(margin_x, margin_y);

		const float y_scale = opengl_engine->getViewPortAspectRatio(); // scale from GL UI to opengl coords
		const float z = 0.1f;
		background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(lower_left_pos.x, lower_left_pos.y * y_scale, z) * Matrix4f::scaleMatrix(background_w, background_h * y_scale, 1);
	}
}


void GLUITextView::setColour(const Colour3f& col)
{
	args.text_colour = col;
	
	if(glui_text.nonNull() && glui_text->overlay_ob.nonNull())
		glui_text->overlay_ob->material.albedo_linear_rgb = col;
}


void GLUITextView::updateGLTransform(GLUI& glui)
{
	if(glui_text.nonNull())
		glui_text->updateGLTransform();

	updateBackgroundOverlayObTransform(glui);
}


void GLUITextView::setPos(GLUI& glui, const Vec2f& new_botleft)
{
	botleft = new_botleft;

	if(glui_text.nonNull())
		glui_text->setPos(botleft);

	updateBackgroundOverlayObTransform(glui);
}


void GLUITextView::setVisible(bool visible)
{
	if(glui_text.nonNull() && glui_text->overlay_ob.nonNull())
	{
		if(glui_text->overlay_ob->draw != visible) // Avoid write if hasn't changed
			glui_text->overlay_ob->draw = visible;
	}

	if(background_overlay_ob.nonNull())
	{
		if(background_overlay_ob->draw != visible) // Avoid write if hasn't changed
			background_overlay_ob->draw = visible;
	}
}


const Vec2f GLUITextView::getDims() const
{
	if(glui_text.nonNull())
		return glui_text->getDims();
	else
		return Vec2f(0.f);
}


GLUITextView::GLUITextViewCreateArgs::GLUITextViewCreateArgs():
	background_colour(0.f),
	background_alpha(1),
	text_colour(1.f),
	text_alpha(1.f),
	padding_px(10),
	font_size_px(14)
{}
