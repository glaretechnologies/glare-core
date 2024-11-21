/*=====================================================================
GLUIImage.cpp
-------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "GLUIImage.h"


#include "GLUI.h"
#include <graphics/SRGBUtils.h>
#include "../OpenGLMeshRenderData.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


static const Colour3f default_colour(1.f);
static const Colour3f default_mouseover_colour = toLinearSRGB(Colour3f(0.9f));


GLUIImage::GLUIImage(GLUI& glui_, Reference<OpenGLEngine>& opengl_engine_, const std::string& tex_path, const Vec2f& botleft_, const Vec2f& dims,
	const std::string& tooltip_, float z_)
:	handler(NULL)
{
	glui = &glui_;
	opengl_engine = opengl_engine_;
	tooltip = tooltip_;

	pos = botleft_;
	z = z_;
	colour = default_colour;
	mouseover_colour = default_mouseover_colour;

	overlay_ob = new OverlayObject();
	overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	overlay_ob->material.albedo_linear_rgb = colour;
	if(!tex_path.empty())
	{
		TextureParams tex_params;
		tex_params.allow_compression = false;
		overlay_ob->material.albedo_texture = opengl_engine->getTexture(tex_path, tex_params);
	}
	overlay_ob->material.tex_matrix = Matrix2f(1,0,0,-1);


	rect = Rect2f(pos, pos + dims);


	const float y_scale = opengl_engine->getViewPortAspectRatio();

	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(pos.x, pos.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);

	opengl_engine->addOverlayObject(overlay_ob);
}


GLUIImage::~GLUIImage()
{
	if(overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(overlay_ob);
}


void GLUIImage::setColour(Colour3f colour_)
{
	colour = colour_;
	if(overlay_ob.nonNull())
		overlay_ob->material.albedo_linear_rgb = colour_;
}


void GLUIImage::setAlpha(float alpha)
{
	if(overlay_ob.nonNull())
		overlay_ob->material.alpha = alpha;
}


void GLUIImage::setMouseOverColour(Colour3f colour_)
{
	mouseover_colour = colour_;
}


void GLUIImage::doHandleMouseMoved(MouseEvent& mouse_event)
{
	const Vec2f coords = glui->UICoordsForOpenGLCoords(mouse_event.gl_coords);
	if(overlay_ob.nonNull())
	{
		if(rect.inOpenRectangle(coords)) // If mouse over widget:
		{
			overlay_ob->material.albedo_linear_rgb = mouseover_colour;
			mouse_event.accepted = true;
		}
		else
		{
			overlay_ob->material.albedo_linear_rgb = colour;
		}
	}
}


bool GLUIImage::doHandleMouseWheelEvent(const Vec2f& coords, const GLUIMouseWheelEvent& wheel_event)
{
	if(rect.inOpenRectangle(coords))
	{
		if(handler)
		{
			GLUICallbackMouseWheelEvent event;
			event.widget = this;
			event.wheel_event = &wheel_event;
			handler->mouseWheelEventOccurred(event);
			if(event.accepted)
				return true;
		}
	}
	return false;
}


void GLUIImage::setDims(const Vec2f& dims)
{
	rect = Rect2f(pos, pos + dims);
	const float y_scale = opengl_engine->getViewPortAspectRatio();

	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(pos.x, pos.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}


void GLUIImage::setPosAndDims(const Vec2f& botleft, const Vec2f& dims, float z_)
{
	pos = botleft;
	z = z_;
	rect = Rect2f(botleft, botleft + dims);

	const float y_scale = opengl_engine->getViewPortAspectRatio();

	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}


void GLUIImage::setTransform(const Vec2f& botleft, const Vec2f& dims, float rotation, float z_)
{
	pos = botleft;
	z = z_;
	rect = Rect2f(botleft, botleft + dims); // NOTE: rectangle-based mouse-over detection will be wrong for non-zero rotation.

	const float y_scale = opengl_engine->getViewPortAspectRatio();

	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * 
		Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1) * Matrix4f::translationMatrix(0.5f, 0.5f, 0) * Matrix4f::rotationAroundZAxis(rotation) * 
		Matrix4f::translationMatrix(-0.5f, -0.5f, 0); // Transform so that rotation rotates around centre of object.
}


void GLUIImage::setVisible(bool visible)
{
	if(overlay_ob.nonNull())
		overlay_ob->draw = visible;
}


bool GLUIImage::isVisible()
{
	return overlay_ob->draw;
}
