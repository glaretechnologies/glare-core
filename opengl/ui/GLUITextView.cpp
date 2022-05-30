/*=====================================================================
GLUITextView.cpp
----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "GLUITextView.h"


#include "GLUI.h"
#include "../OpenGLMeshRenderData.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"



GLUITextView::GLUITextView()
:	handler(NULL)
{

}


GLUITextView::~GLUITextView()
{
	destroy();
}


// For non-toggleable buttons:
static const Colour3f default_widget_colour(1.f);
static const Colour3f default_mouseover_widget_colour(0.9f);


void GLUITextView::create(GLUI& glui, Reference<OpenGLEngine>& opengl_engine_, const std::string& text_, const Vec2f& botleft, const Vec2f& dims,
	const std::string& tooltip_)
{
	opengl_engine = opengl_engine_;
	tooltip = tooltip_;

	text = text_;

	widget_colour = default_widget_colour;
	mouseover_widget_colour = default_mouseover_widget_colour;

	overlay_ob = new OverlayObject();
	overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	overlay_ob->material.albedo_rgb = widget_colour;
	overlay_ob->material.albedo_texture = glui.text_renderer->makeToolTipTexture(text);


	rect = Rect2f(botleft, botleft + dims);


	const float y_scale = opengl_engine->getViewPortAspectRatio();

	const float z = -0.999f;
	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);

	opengl_engine->addOverlayObject(overlay_ob);
}


void GLUITextView::destroy()
{
	if(overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(overlay_ob);
	overlay_ob = NULL;
	opengl_engine = NULL;
}


void GLUITextView::setText(GLUI& glui, const std::string& new_text)
{
	if(new_text != text)
	{
		text = new_text;

		overlay_ob->material.albedo_texture = glui.text_renderer->makeToolTipTexture(text);
	}
}


Vec2f GLUITextView::getTextureDimensions() const
{
	if(overlay_ob->material.albedo_texture.nonNull())
		return Vec2f((float)overlay_ob->material.albedo_texture->xRes(), (float)overlay_ob->material.albedo_texture->yRes());
	else
		return Vec2f(1.f);
}


void GLUITextView::setColour(const Colour3f& col)
{
	widget_colour = col;
	mouseover_widget_colour = col;

	if(overlay_ob.nonNull())
	{
		overlay_ob->material.albedo_rgb = widget_colour;
	}
}


bool GLUITextView::doHandleMouseClick(const Vec2f& coords)
{
	if(rect.inOpenRectangle(coords))
	{
		if(handler)
		{
			GLUICallbackEvent event;
			event.widget = this;
			handler->eventOccurred(event);
			if(event.accepted)
				return true;
		}
	}

	return false;
}


bool GLUITextView::doHandleMouseMoved(const Vec2f& coords)
{
	if(overlay_ob.nonNull())
	{
		if(rect.inOpenRectangle(coords)) // If mouse over widget:
		{
			overlay_ob->material.albedo_rgb = mouseover_widget_colour;

			return true;
		}
		else
		{
			overlay_ob->material.albedo_rgb = widget_colour;
		}
	}
	return false;
}


void GLUITextView::setPosAndDims(const Vec2f& botleft, const Vec2f& dims)
{
	rect = Rect2f(botleft, botleft + dims);

	const float y_scale = opengl_engine->getViewPortAspectRatio();

	const float z = -0.9f;
	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}
