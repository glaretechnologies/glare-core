/*=====================================================================
GLUIButton.cpp
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "GLUIButton.h"


#include "GLUI.h"
#include <graphics/SRGBUtils.h>
#include "../OpenGLMeshRenderData.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


GLUIButton::CreateArgs::CreateArgs()
{
	toggled_colour = toLinearSRGB(Colour3f(0.7f, 0.8f, 1.f));
	untoggled_colour = Colour3f(1.f);

	mouseover_toggled_colour = toLinearSRGB(Colour3f(0.8f, 0.9f, 1.f));
	mouseover_untoggled_colour = toLinearSRGB(Colour3f(0.9f));

	// For non-toggleable buttons:
	button_colour = Colour3f(1.f);
	pressed_colour = toLinearSRGB(Colour3f(0.7f, 0.8f, 1.f));
	mouseover_button_colour = toLinearSRGB(Colour3f(0.9f));
}


GLUIButton::GLUIButton(GLUI& glui_, Reference<OpenGLEngine>& opengl_engine_, const std::string& tex_path, const Vec2f& botleft, const Vec2f& dims, const CreateArgs& args_)
:	handler(NULL),
	toggleable(false),
	toggled(false),
	pressed(false)
{
	glui = &glui_;
	opengl_engine = opengl_engine_;
	tooltip = args_.tooltip;
	args = args_;
	immutable_dims = false;

	overlay_ob = new OverlayObject();
	overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	overlay_ob->material.albedo_linear_rgb = args.button_colour;
	TextureParams tex_params;
	tex_params.allow_compression = false;
	overlay_ob->material.albedo_texture = opengl_engine->getTexture(tex_path, tex_params);
	overlay_ob->material.tex_matrix = Matrix2f(1,0,0,-1);


	rect = Rect2f(botleft, botleft + dims);


	const float y_scale = opengl_engine->getViewPortAspectRatio();

	const float z = -0.999f;
	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);

	opengl_engine->addOverlayObject(overlay_ob);
}


GLUIButton::~GLUIButton()
{
	if(overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(overlay_ob);
}


void GLUIButton::handleMousePress(MouseEvent& event)
{
	if(!overlay_ob->draw) // If not visible:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inOpenRectangle(coords))
	{
		pressed = true;

		if(toggleable)
			toggled = !toggled;

		if(handler)
		{
			GLUICallbackEvent callback_event;
			callback_event.widget = this;
			handler->eventOccurred(callback_event);
			if(callback_event.accepted)
				event.accepted = true;
		}
	}

	updateButtonColour(coords);
}


void GLUIButton::handleMouseRelease(MouseEvent& event)
{
	if(pressed)
	{
		pressed = false;

		const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);

		updateButtonColour(coords);
	}
}


void GLUIButton::doHandleMouseMoved(MouseEvent& mouse_event)
{
	if(!overlay_ob->draw) // If not visible:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(mouse_event.gl_coords);

	if(overlay_ob.nonNull())
	{
		if(rect.inOpenRectangle(coords)) // If mouse over widget:
		{
			mouse_event.accepted = true;
		}
		else
		{
			// For now, allow button to remain pressed even when mouse pointer moves off it.
			// pressed = false;
		}
	}

	updateButtonColour(coords);
}


void GLUIButton::setDims(const Vec2f& new_dims)
{
	const Vec2f botleft = rect.getMin();
	rect = Rect2f(botleft, botleft + new_dims);

	const float y_scale = opengl_engine->getViewPortAspectRatio();

	const float z = -0.9f;
	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(new_dims.x, new_dims.y * y_scale, 1);
}


void GLUIButton::setPosAndDims(const Vec2f& botleft, const Vec2f& new_dims)
{
	const Vec2f dims = immutable_dims ? rect.getWidths() : new_dims;

	rect = Rect2f(botleft, botleft + dims);

	const float y_scale = opengl_engine->getViewPortAspectRatio();

	const float z = -0.9f;
	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}


void GLUIButton::setClipRegion(const Rect2f& clip_rect)
{
	overlay_ob->clip_region = glui->OpenGLRectCoordsForUICoords(clip_rect);
}


void GLUIButton::setToggled(bool toggled_)
{
	toggled = toggled_;

	updateButtonColour(glui->getLastMouseUICoords());
}


void GLUIButton::setVisible(bool visible)
{
	if(overlay_ob.nonNull())
		overlay_ob->draw = visible;
}


bool GLUIButton::isVisible()
{
	return overlay_ob->draw;
}


void GLUIButton::updateButtonColour(const Vec2f mouse_ui_coords)
{
	if(overlay_ob)
	{
		if(rect.inOpenRectangle(mouse_ui_coords)) // If mouse over widget:
		{
			if(toggleable)
				overlay_ob->material.albedo_linear_rgb = toggled ? args.mouseover_toggled_colour : args.mouseover_untoggled_colour;
			else
				overlay_ob->material.albedo_linear_rgb = pressed ? args.pressed_colour : args.mouseover_button_colour;
		}
		else // else if mouse not over widget:
		{
			if(toggleable)
				overlay_ob->material.albedo_linear_rgb = toggled ? args.toggled_colour : args.untoggled_colour;
			else
				overlay_ob->material.albedo_linear_rgb = pressed ? args.pressed_colour : args.button_colour;
		}
	}
}
