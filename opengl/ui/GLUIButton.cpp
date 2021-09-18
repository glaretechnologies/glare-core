/*=====================================================================
GLUIButton.cpp
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "GLUIButton.h"


#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"



GLUIButton::GLUIButton()
:	handler(NULL),
	toggleable(false),
	toggled(false)
{

}


GLUIButton::~GLUIButton()
{
	destroy();
}


// For toggleable buttons:
static const Colour3f toggled_colour(0.7f, 0.8f, 1.f);
static const Colour3f untoggled_colour(1.f);

static const Colour3f mouseover_toggled_colour(0.8f, 0.9f, 1.f);
static const Colour3f mouseover_untoggled_colour(0.9f);

// For non-toggleable buttons:
static const Colour3f button_colour(1.f);
static const Colour3f mouseover_button_colour(0.9f);


void GLUIButton::create(GLUI& glui, Reference<OpenGLEngine>& opengl_engine_, const std::string& tex_path, const Vec2f& botleft, const Vec2f& dims,
	const std::string& tooltip_)
{
	opengl_engine = opengl_engine_;
	tooltip = tooltip_;

	overlay_ob = new OverlayObject();
	overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	overlay_ob->material.albedo_rgb = button_colour;
	overlay_ob->material.albedo_texture = opengl_engine->getTexture(tex_path, /*allow_compression=*/false);
	overlay_ob->material.tex_matrix = Matrix2f(1,0,0,-1);


	rect = Rect2f(botleft, botleft + dims);


	const float y_scale = opengl_engine->getViewPortAspectRatio();

	const float z = -0.9f;
	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);

	opengl_engine->addOverlayObject(overlay_ob);
}


void GLUIButton::destroy()
{
	if(overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(overlay_ob);
	overlay_ob = NULL;
	opengl_engine = NULL;
}





bool GLUIButton::doHandleMouseClick(const Vec2f& coords)
{
	if(rect.inOpenRectangle(coords))
	{
		if(toggleable)
		{
			toggled = !toggled;

			if(toggled)
				overlay_ob->material.albedo_rgb = toggled_colour;
			else
				overlay_ob->material.albedo_rgb = untoggled_colour;
		}

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


bool GLUIButton::doHandleMouseMoved(const Vec2f& coords)
{
	if(overlay_ob.nonNull())
	{
		if(rect.inOpenRectangle(coords)) // If mouse over widget:
		{
			if(toggleable)
			{
				if(toggled)
					overlay_ob->material.albedo_rgb = mouseover_toggled_colour;
				else
					overlay_ob->material.albedo_rgb = mouseover_untoggled_colour;
			}
			else
				overlay_ob->material.albedo_rgb = mouseover_button_colour;

			return true;
		}
		else
		{
			if(toggleable)
			{
				if(toggled)
					overlay_ob->material.albedo_rgb = toggled_colour;
				else
					overlay_ob->material.albedo_rgb = untoggled_colour;
			}
			else
				overlay_ob->material.albedo_rgb = button_colour;
		}
	}
	return false;
}


void GLUIButton::setPosAndDims(const Vec2f& botleft, const Vec2f& dims)
{
	rect = Rect2f(botleft, botleft + dims);

	const float y_scale = opengl_engine->getViewPortAspectRatio();

	const float z = -0.9f;
	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}


void GLUIButton::setToggled(bool toggled_)
{
	toggled = toggled_;

	if(toggleable)
	{
		if(toggled)
			overlay_ob->material.albedo_rgb = toggled_colour;
		else
			overlay_ob->material.albedo_rgb = untoggled_colour;
	}
	else
		overlay_ob->material.albedo_rgb = button_colour;
}
