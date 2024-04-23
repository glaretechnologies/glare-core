/*=====================================================================
GLUILineEdit.cpp
----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "GLUILineEdit.h"


#include "GLUI.h"
#include "../graphics/SRGBUtils.h"
#include "../OpenGLMeshRenderData.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


GLUILineEdit::GLUILineEdit()
{

}


GLUILineEdit::~GLUILineEdit()
{
	destroy();
}


void GLUILineEdit::create(GLUI& glui_, Reference<OpenGLEngine>& opengl_engine_, const Vec2f& botleft_, 
	const GLUILineEditCreateArgs& args_)
{
	glui = &glui_;

	args = args_;
	opengl_engine = opengl_engine_;
	tooltip = args.tooltip;

	botleft = botleft_;

	background_overlay_ob = new OverlayObject();
	background_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	background_overlay_ob->material.albedo_linear_rgb = args.background_colour;
	background_overlay_ob->material.alpha = args.background_alpha;

	GLUIText::GLUITextCreateArgs text_create_args;
	text_create_args.colour = args.text_colour;
	text_create_args.font_size_px = args.font_size_px;
	text_create_args.alpha = args.text_alpha;
	glui_text = new GLUIText(glui_, opengl_engine_, text, botleft, text_create_args);

	//rect = glui_text->getRect();
	rect = Rect2f(botleft, botleft + Vec2f(args.width, glui_.getUIWidthForDevIndepPixelWidth(args.font_size_px)));

	opengl_engine->addOverlayObject(background_overlay_ob);

	cursor_overlay_ob = new OverlayObject();
	cursor_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	cursor_overlay_ob->material.albedo_linear_rgb = Colour3f(1,0,0);
	cursor_overlay_ob->material.alpha = 1;

	opengl_engine->addOverlayObject(cursor_overlay_ob);

	updateOverlayObTransforms(glui_);
}


void GLUILineEdit::destroy()
{
	if(glui_text.nonNull())
		glui_text->destroy();
	glui_text = NULL;

	if(background_overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(background_overlay_ob);
	background_overlay_ob = NULL;
	
	if(cursor_overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(cursor_overlay_ob);
	cursor_overlay_ob = NULL;

	opengl_engine = NULL;
}


void GLUILineEdit::setText(GLUI& glui, const std::string& new_text)
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

		updateOverlayObTransforms(glui);
	}
}


void GLUILineEdit::updateOverlayObTransforms(GLUI& glui)
{
	const float margin_x = glui.getUIWidthForDevIndepPixelWidth((float)args.padding_px);
	const float margin_y = margin_x;

	if(background_overlay_ob.nonNull())
	{
		const float background_w = args.width;//glui_text->getDims().x + margin_x*2;
		const float background_h = glui_text->getDims().y + margin_y*2;

		const Vec2f text_lower_left_pos = botleft;//glui_text->getRect().getMin();
		const Vec2f lower_left_pos = text_lower_left_pos - Vec2f(margin_x, margin_y);

		const float y_scale = opengl_engine->getViewPortAspectRatio(); // scale from GL UI to opengl coords
		const float z = 0.1f;
		background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(lower_left_pos.x, lower_left_pos.y * y_scale, z) * Matrix4f::scaleMatrix(background_w, background_h * y_scale, 1);
	}

	// Update cursor ob
	if(cursor_overlay_ob.nonNull())
	{
		const float w = 0.005f;
		const float h = (float)glui.getUIWidthForDevIndepPixelWidth(args.font_size_px);
		const Vec2f text_lower_left_pos = botleft;//glui_text->getRect().getMin();
		const Vec2f lower_left_pos = text_lower_left_pos - Vec2f(margin_x, margin_y);

		const float y_scale = opengl_engine->getViewPortAspectRatio(); // scale from GL UI to opengl coords
		const float z = -0.1f;
		background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(lower_left_pos.x, lower_left_pos.y * y_scale, z) * Matrix4f::scaleMatrix(w, h * y_scale, 1);
	}
}


bool GLUILineEdit::doHandleMouseClick(const Vec2f& coords)
{
	if(rect.inClosedRectangle(coords))
	{
		glui->setKeyboardFocusWidget(this);
		return true;
	}
	return false;
}


void GLUILineEdit::dohandleKeyPressedEvent(const KeyEvent& key_event)
{
	text += key_event.text;

	// Re-create glui_text
	if(glui_text.nonNull())
		glui_text->destroy();

	GLUIText::GLUITextCreateArgs text_create_args;
	text_create_args.font_size_px = args.font_size_px;
	text_create_args.alpha = args.text_alpha;
	glui_text = new GLUIText(*glui, opengl_engine, text, botleft, text_create_args);
}


void GLUILineEdit::updateGLTransform(GLUI& glui)
{
	if(glui_text.nonNull())
		glui_text->updateGLTransform();

	updateOverlayObTransforms(glui);
}


//void GLUILineEdit::setPos(GLUI& glui, const Vec2f& new_botleft)
//{
//	botleft = new_botleft;
//
//	if(glui_text.nonNull())
//		glui_text->setPos(botleft);
//
//	updateBackgroundOverlayObTransform(glui);
//}


void GLUILineEdit::setVisible(bool visible)
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


const Vec2f GLUILineEdit::getDims() const
{
	if(glui_text.nonNull())
		return glui_text->getDims();
	else
		return Vec2f(0.f);
}


GLUILineEdit::GLUILineEditCreateArgs::GLUILineEditCreateArgs():
	background_colour(0.f),
	background_alpha(1),
	text_colour(1.f),
	text_alpha(1.f),
	padding_px(10),
	font_size_px(20),
	width(0.2f)
{}
