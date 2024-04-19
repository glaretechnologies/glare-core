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


static const float TOOLTIP_Z = -0.999f; // -1 is near clip plane


GLUI::GLUI()
{
}


GLUI::~GLUI()
{
	destroy();
}


void GLUI::create(Reference<OpenGLEngine>& opengl_engine_, float device_pixel_ratio_, const std::vector<TextRendererFontFaceRef>& fonts_, const std::vector<TextRendererFontFaceRef>& emoji_fonts_)
{
	opengl_engine = opengl_engine_;
	device_pixel_ratio = device_pixel_ratio_;
	fonts = fonts_;
	emoji_fonts = emoji_fonts_;

	tooltip_overlay_ob = new OverlayObject();
	tooltip_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	tooltip_overlay_ob->material.albedo_linear_rgb = toLinearSRGB(Colour3f(0.95f));
	tooltip_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(1000, 1000, 0);

	opengl_engine->addOverlayObject(tooltip_overlay_ob);

	font_char_text_cache = new FontCharTexCache();
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
					tex = makeToolTipTexture(widget->tooltip);
					tooltip_textures[widget->tooltip] = tex;
				}
				else
					tex = res->second;

				tooltip_overlay_ob->material.albedo_texture = tex;
				tooltip_overlay_ob->material.tex_matrix = Matrix2f(1,0,0,-1); // Compensate for OpenGL loading textures upside down (row 0 in OpenGL is considered to be at the bottom of texture)
				tooltip_overlay_ob->material.tex_translation = Vec2f(0, 1);

				const float scale_y = 70.0f / opengl_engine->getViewPortWidth() / y_scale; // ~50 px high
				const float scale_x = 70.0f / opengl_engine->getViewPortWidth() * ((float)tex->xRes() / (float)tex->yRes());
					
				//const float scale_x = 0.5f * (float)tex->xRes() / opengl_engine->getViewPortWidth();
				//const float scale_y = 0.5f * (float)tex->yRes() / opengl_engine->getViewPortWidth() / y_scale;

				const float mouse_pointer_h_gl = 40.f / opengl_engine->getViewPortHeight();

				float pos_x = gl_coords.x;
				float pos_y = gl_coords.y - scale_y - mouse_pointer_h_gl;
				
				if(pos_y < -1.f) // If tooltip would go off bottom of screen:
					pos_y = gl_coords.y;

				if(pos_x + scale_x > 1.f) // If tooltip would go off right of screen:
					pos_x = gl_coords.x - scale_x;

				tooltip_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(pos_x, pos_y, TOOLTIP_Z) * Matrix4f::scaleMatrix(scale_x, scale_y, 1);
			}
		}
	}

	return false;
}


void GLUI::viewportResized(int w, int h)
{
	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		widget->updateGLTransform(*this);
	}
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


OpenGLTextureRef GLUI::makeToolTipTexture(const std::string& tooltip_text)
{
	TextRendererFontFaceRef font = fonts[myMin(1, (int)fonts.size() - 1)];
	const TextRendererFontFace::SizeInfo size_info = font->getTextSize(tooltip_text);

	const int use_font_height = size_info.max_bounds.y; //text_renderer_font->getFontSizePixels();
	const int padding_x = (int)(use_font_height * 0.6f);
	const int padding_y = (int)(use_font_height * 0.6f);
	

	ImageMapUInt8Ref map = new ImageMapUInt8(size_info.size.x + padding_x * 2, use_font_height + padding_y * 2, 3);
	map->set(240); // Set to light grey colour

	font->drawText(*map, tooltip_text, padding_x, padding_y + use_font_height, Colour3f(0.05f));


	TextureParams tex_params;
	tex_params.wrapping = OpenGLTexture::Wrapping_Clamp;
	tex_params.allow_compression = false;
	return opengl_engine->getOrLoadOpenGLTextureForMap2D(OpenGLTextureKey("tooltip_" + tooltip_text), *map, tex_params);
}
