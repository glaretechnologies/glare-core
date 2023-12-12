/*=====================================================================
GLUI.cpp
--------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "GLUI.h"


#include <graphics/SRGBUtils.h>
#include "../OpenGLMeshRenderData.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


GLUI::GLUI()
:	text_renderer(NULL)
{
}


GLUI::~GLUI()
{
	destroy();
}


void GLUI::create(Reference<OpenGLEngine>& opengl_engine_, float device_pixel_ratio_, GLUITextRendererCallback* text_renderer_)
{
	opengl_engine = opengl_engine_;
	device_pixel_ratio = device_pixel_ratio_;
	text_renderer = text_renderer_;

	tooltip_overlay_ob = new OverlayObject();
	tooltip_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	tooltip_overlay_ob->material.albedo_linear_rgb = toLinearSRGB(Colour3f(0.95f));
	tooltip_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(1000, 1000, 0);

	opengl_engine->addOverlayObject(tooltip_overlay_ob);
}


void GLUI::destroy()
{
	if(tooltip_overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(tooltip_overlay_ob);
	tooltip_overlay_ob = NULL;

	opengl_engine = NULL;
}


Vec2f GLUI::UICoordsForWindowPixelCoords(const Vec2f& pixel_coords)
{
	const Vec2f gl_coords(
		-1.f + 2 * pixel_coords.x / (float)opengl_engine->getViewPortWidth(),
		 1.f - 2 * pixel_coords.y / (float)opengl_engine->getViewPortHeight()
	);

	return UICoordsForOpenGLCoords(gl_coords);
}


Vec2f GLUI::UICoordsForOpenGLCoords(const Vec2f& gl_coords)
{
	// Convert from gl_coords to UI x/y coords
	return Vec2f(gl_coords.x, gl_coords.y / opengl_engine->getViewPortAspectRatio());
}


bool GLUI::handleMouseClick(const Vec2f& gl_coords)
{
	const Vec2f coords = UICoordsForOpenGLCoords(gl_coords);

	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		const bool accepted = widget->handleMouseClick(coords);
		if(accepted)
			return true;

		//if(widget->rect.inOpenRectangle(coords)) // If the mouse is over the widget:
		//{
		//	return true;
		//}
	}

	return false;
}


bool GLUI::handleMouseWheelEvent(const Vec2f& gl_coords, const GLUIMouseWheelEvent& event)
{
	const Vec2f coords = UICoordsForOpenGLCoords(gl_coords);

	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		const bool accepted = widget->handleMouseWheelEvent(coords, event);
		if(accepted)
			return true;
	}

	return false;
}


bool GLUI::handleMouseMoved(const Vec2f& gl_coords)
{
	// Convert from gl_coords to UI x/y coords
	const float y_scale = 1 / opengl_engine->getViewPortAspectRatio();
	const Vec2f coords(gl_coords.x, gl_coords.y * y_scale);

	tooltip_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(1000, 1000, 0); // Move offscreen by defualt.

	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		widget->handleMouseMoved(coords);

		// Show tooltip over the widget if the mouse is over it, and it has a tooltip:
		if(widget->rect.inOpenRectangle(coords)) // If the mouse is over the widget:
		{
			if(!widget->tooltip.empty()) // And the widget has a tooltip:
			{
				// Get or create tooltip texture
				OpenGLTextureRef tex;
				auto res = tooltip_textures.find(widget->tooltip);
				if(res == tooltip_textures.end())
				{
					tex = text_renderer->makeToolTipTexture(widget->tooltip);
					tooltip_textures[widget->tooltip] = tex;
				}
				else
					tex = res->second;

				tooltip_overlay_ob->material.albedo_texture = tex;

				const float scale_x = 1.f * (float)tex->xRes() / opengl_engine->getViewPortWidth();
				const float scale_y = 1.f * (float)tex->yRes() / opengl_engine->getViewPortWidth() / y_scale;

				const float mouse_pointer_h_gl = 40.f / opengl_engine->getViewPortHeight();

				float pos_x = gl_coords.x;
				float pos_y = gl_coords.y - scale_y - mouse_pointer_h_gl;
				
				if(pos_y < -1.f) // If tooltip would go off bottom of screen:
					pos_y = gl_coords.y;

				if(pos_x + scale_x > 1.f) // If tooltip would go off right of screen:
					pos_x = gl_coords.x - scale_x;

				tooltip_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(pos_x, pos_y, -0.8f) * Matrix4f::scaleMatrix(scale_x, scale_y, 1);
			}
		}
	}

	return false;
}


void GLUI::viewportResized(int w, int h)
{

}


float GLUI::getViewportMinMaxY(Reference<OpenGLEngine>& opengl_engine)
{
	const float y_scale = opengl_engine->getViewPortAspectRatio();
	return 1 / y_scale;
}


float GLUI::getUIWidthForDevIndepPixelWidth(float pixel_w)
{
	// 2 factor is because something spanning the full viewport ranges from y=-1 to 1.

	return 2 * device_pixel_ratio * pixel_w / (float)opengl_engine->getViewPortWidth();
}
