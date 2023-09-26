/*=====================================================================
GLUIImage.cpp
-------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "GLUIImage.h"


#include <graphics/SRGBUtils.h>
#include "../OpenGLMeshRenderData.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"



GLUIImage::GLUIImage()
{
}


GLUIImage::~GLUIImage()
{
	destroy();
}


static const Colour3f button_colour(1.f);
static const Colour3f mouseover_button_colour = toLinearSRGB(Colour3f(0.9f));


void GLUIImage::create(GLUI& glui, Reference<OpenGLEngine>& opengl_engine_, const std::string& tex_path, const Vec2f& botleft, const Vec2f& dims,
	const std::string& tooltip_)
{
	opengl_engine = opengl_engine_;
	tooltip = tooltip_;

	overlay_ob = new OverlayObject();
	overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	overlay_ob->material.albedo_linear_rgb = button_colour;
	if(!tex_path.empty())
	{
		TextureParams tex_params;
		tex_params.allow_compression = false;
		overlay_ob->material.albedo_texture = opengl_engine->getTexture(tex_path, tex_params);
	}
	overlay_ob->material.tex_matrix = Matrix2f(1,0,0,-1);


	rect = Rect2f(botleft, botleft + dims);


	const float y_scale = opengl_engine->getViewPortAspectRatio();

	const float z = -0.999f;
	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);

	opengl_engine->addOverlayObject(overlay_ob);
}


void GLUIImage::destroy()
{
	if(overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(overlay_ob);
	overlay_ob = NULL;
	opengl_engine = NULL;
}


bool GLUIImage::doHandleMouseMoved(const Vec2f& coords)
{
	if(overlay_ob.nonNull())
	{
		if(rect.inOpenRectangle(coords)) // If mouse over widget:
		{
			overlay_ob->material.albedo_linear_rgb = mouseover_button_colour;
			return true;
		}
		else
		{
			overlay_ob->material.albedo_linear_rgb = button_colour;
		}
	}
	return false;
}


void GLUIImage::setPosAndDims(const Vec2f& botleft, const Vec2f& dims, float z)
{
	rect = Rect2f(botleft, botleft + dims);

	const float y_scale = opengl_engine->getViewPortAspectRatio();

	//const float z = -0.9f;
	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}
