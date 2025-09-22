/*=====================================================================
GLUI.cpp
--------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "GLUI.h"


#include "GLUIInertWidget.h"
#include <graphics/SRGBUtils.h>
#include "../OpenGLMeshRenderData.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


static const float TOOLTIP_Z = -0.999f; // -1 is near clip plane
static const int tooltip_font_size_px = 12;


GLUI::GLUI()
:	callbacks(NULL),
	mouse_over_text_input_widget(false),
	stack_allocator(NULL),
	last_mouse_ui_coords(0.f)
{
}


GLUI::~GLUI()
{
	destroy();
}


void GLUI::create(Reference<OpenGLEngine>& opengl_engine_, float device_pixel_ratio_, 
	const TextRendererFontFaceSizeSetRef& fonts_, const TextRendererFontFaceSizeSetRef& emoji_fonts_,
	glare::StackAllocator* stack_allocator_)
{
	opengl_engine = opengl_engine_;
	device_pixel_ratio = device_pixel_ratio_;
	ui_scale = 1.0f;
	fonts = fonts_;
	emoji_fonts = emoji_fonts_;
	stack_allocator = stack_allocator_;

	tooltip_overlay_ob = new OverlayObject();
	tooltip_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	tooltip_overlay_ob->material.albedo_linear_rgb = toLinearSRGB(Colour3f(0.95f));
	tooltip_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(1000, 1000, 0);

	opengl_engine->addOverlayObject(tooltip_overlay_ob);

	font_char_text_cache = new FontCharTexCache();
}


void GLUI::destroy()
{
	if(!widgets.empty())
	{
		conPrint("WARNING: " + toString(widgets.size()) + " widgets still in GLUI upon destruction.");
	//	assert(0);
		widgets.clear();
	}

	if(tooltip_overlay_ob.nonNull())
		opengl_engine->removeOverlayObject(tooltip_overlay_ob);
	tooltip_overlay_ob = NULL;

	opengl_engine = NULL;
}


void GLUI::think()
{
	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		widget->think(*this);
	}
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


Vec2f GLUI::OpenGLCoordsForUICoords(const Vec2f& ui_coords)
{
	return Vec2f(ui_coords.x, ui_coords.y * opengl_engine->getViewPortAspectRatio());
}


Rect2f GLUI::OpenGLRectCoordsForUICoords(const Rect2f& ui_coords)
{
	return Rect2f(
		OpenGLCoordsForUICoords(ui_coords.getMin()), 
		OpenGLCoordsForUICoords(ui_coords.getMax())
	);
}


float GLUI::OpenGLYScaleForUIYScale(float y_scale)
{
	return y_scale * opengl_engine->getViewPortAspectRatio();
}


// Sorts objects into ascending z order
struct GLUIWidgetZComparator
{
	inline bool operator() (const GLUIWidget* a, const GLUIWidget* b) const
	{
		return a->m_z < b->m_z;
	}
};


// Get list of widgets whose rectangle contains ui_coords, sorted by widget z.
static void getSortedWidgetsAtEventPos(const std::set<GLUIWidgetRef>& widgets, const Vec2f ui_coords, std::vector<GLUIWidget*>& temp_widgets_out)
{
	temp_widgets_out.clear();
	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		if(widget->rect.inClosedRectangle(ui_coords))
			temp_widgets_out.push_back(widget);
	}

	std::sort(temp_widgets_out.begin(), temp_widgets_out.end(), GLUIWidgetZComparator());
}


void GLUI::handleMousePress(MouseEvent& event)
{
	const Vec2f ui_coords = UICoordsForOpenGLCoords(event.gl_coords);
	
	getSortedWidgetsAtEventPos(widgets, ui_coords, temp_widgets);

	for(size_t i=0; i<temp_widgets.size(); ++i)
	{
		temp_widgets[i]->handleMousePress(event);
		if(event.accepted)
		{
			if(temp_widgets[i] != key_focus_widget.ptr())
				setKeyboardFocusWidget(NULL); // A widget that wasn't the one with keyboard focus accepted the click.  Remove keyboard focus from any widgets that had it.

			return;
		}
	}

	// No widget accepted the click event.  Remove keyboard focus from any widgets that had it.
	setKeyboardFocusWidget(NULL);
}


void GLUI::handleMouseRelease(MouseEvent& event)
{
	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		widget->handleMouseRelease(event);
		if(event.accepted)
			return;
	}
}


void GLUI::handleMouseDoubleClick(MouseEvent& event)
{
	const Vec2f ui_coords = UICoordsForOpenGLCoords(event.gl_coords);
	getSortedWidgetsAtEventPos(widgets, ui_coords, temp_widgets);

	for(size_t i=0; i<temp_widgets.size(); ++i)
	{
		temp_widgets[i]->handleMouseDoubleClick(event);
		if(event.accepted)
			return;
	}
}


bool GLUI::handleMouseWheelEvent(MouseWheelEvent& event)
{
	const Vec2f ui_coords = UICoordsForOpenGLCoords(event.gl_coords);
	
	getSortedWidgetsAtEventPos(widgets, ui_coords, temp_widgets);
	for(size_t i=0; i<temp_widgets.size(); ++i)
	{
		temp_widgets[i]->handleMouseWheelEvent(event);
		if(event.accepted)
			return true;
	}

	return false;
}


bool GLUI::handleMouseMoved(MouseEvent& mouse_event)
{
	const Vec2f coords = UICoordsForOpenGLCoords(mouse_event.gl_coords);

	this->last_mouse_ui_coords = coords;

	tooltip_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(1000, 1000, 0); // Move offscreen by default.

	bool new_mouse_over_text_input_widget = false;
	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		widget->handleMouseMoved(mouse_event);

		// Show tooltip over the widget if the mouse is over it, and it has a tooltip:
		if(widget->rect.inOpenRectangle(coords) && widget->isVisible()) // If the mouse is over the widget:
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

				// This should give 1:1 texel : screen-space pixels.
				const Vec2f ui_dimensions = Vec2f((float)tex->xRes(), (float)tex->yRes()) * 2 / (float)opengl_engine->getViewPortWidth();

				const Vec2f gl_dims = OpenGLCoordsForUICoords(ui_dimensions);
				const float scale_x = gl_dims.x;
				const float scale_y = gl_dims.y;

				const float mouse_pointer_h_gl = 40.f / opengl_engine->getViewPortHeight();

				float pos_x = mouse_event.gl_coords.x;
				float pos_y = mouse_event.gl_coords.y - scale_y - mouse_pointer_h_gl;
				
				if(pos_y < -1.f) // If tooltip would go off bottom of screen:
					pos_y = mouse_event.gl_coords.y;

				if(pos_x + scale_x > 1.f) // If tooltip would go off right of screen:
					pos_x = mouse_event.gl_coords.x - scale_x;

				tooltip_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(pos_x, pos_y, TOOLTIP_Z) * Matrix4f::scaleMatrix(scale_x, scale_y, 1);
			}

			if(widget->acceptsTextInput())
				new_mouse_over_text_input_widget = true;
		}
	}

	// Update mouse_over_text_input_widget
	if(new_mouse_over_text_input_widget != mouse_over_text_input_widget)
	{
		if(callbacks)
			callbacks->setMouseCursor(new_mouse_over_text_input_widget ? GLUICallbacks::MouseCursor_IBeam : GLUICallbacks::MouseCursor_Arrow);

		mouse_over_text_input_widget = new_mouse_over_text_input_widget;
	}

	return false;
}


void GLUI::handleKeyPressedEvent(KeyEvent& key_event)
{
	// Pass event on to the widget with keyboard focus, if any
	if(key_focus_widget.nonNull())
	{
		key_focus_widget->handleKeyPressedEvent(key_event);
	}
}


void GLUI::handleTextInputEvent(TextInputEvent& text_input_event)
{
	// Pass event on to the widget with keyboard focus, if any
	if(key_focus_widget.nonNull())
	{
		key_focus_widget->handleTextInputEvent(text_input_event);
	}
}


void GLUI::handleCutEvent(std::string& clipboard_contents_out)
{
	clipboard_contents_out.clear();

	if(key_focus_widget.nonNull())
	{
		key_focus_widget->handleCutEvent(clipboard_contents_out);
	}
}


void GLUI::handleCopyEvent(std::string& clipboard_contents_out)
{
	clipboard_contents_out.clear();

	if(key_focus_widget.nonNull())
	{
		key_focus_widget->handleCopyEvent(clipboard_contents_out);
	}
}


void GLUI::viewportResized(int /*w*/, int /*h*/)
{
	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		widget->updateGLTransform(*this);
	}
}


float GLUI::getViewportMinMaxY()
{
	const float y_scale = opengl_engine->getViewPortAspectRatio();
	return 1 / y_scale;
}


float GLUI::getYScale()
{
	return 1.f / opengl_engine->getViewPortAspectRatio();
}


float GLUI::getUIWidthForDevIndepPixelWidth(float pixel_w)
{
	// 2 factor is because something spanning the full viewport ranges from y=-1 to 1.

	return 2 * device_pixel_ratio * ui_scale * pixel_w / (float)opengl_engine->getViewPortWidth();
}


// ui_width = 2 * device_pixel_ratio * pixel_w / (float)opengl_engine->getViewPortWidth();
// pixel_w = ui_width * opengl_engine->getViewPortWidth() / (2 * device_pixel_ratio)

float GLUI::getDevIndepPixelWidthForUIWidth(float ui_width)
{
	return ui_width * (float)opengl_engine->getViewPortWidth() / (2 * device_pixel_ratio * ui_scale);
}


OpenGLTextureRef GLUI::makeToolTipTexture(const std::string& tooltip_text)
{
	TextRendererFontFaceRef font = fonts->getFontFaceForSize((int)((float)tooltip_font_size_px * this->device_pixel_ratio * ui_scale));

	const TextRendererFontFace::SizeInfo size_info = font->getTextSize(tooltip_text);

	const int use_font_height = size_info.max_bounds.y; //text_renderer_font->getFontSizePixels();
	const int padding_x = (int)(use_font_height * 0.6f);
	const int padding_y = (int)(use_font_height * 0.6f);
	

	ImageMapUInt8Ref map = new ImageMapUInt8(size_info.glyphSize().x + padding_x * 2, use_font_height + padding_y * 2, 3);
	map->set(240); // Set to light grey colour

	font->drawText(*map, tooltip_text, padding_x, padding_y + use_font_height, Colour3f(0.05f), /*render SDF=*/false);


	TextureParams tex_params;
	tex_params.wrapping = OpenGLTexture::Wrapping_Clamp;
	tex_params.allow_compression = false;
	return opengl_engine->getOrLoadOpenGLTextureForMap2D(OpenGLTextureKey("tooltip_" + tooltip_text), *map, tex_params);
}


void GLUI::hideTooltip()
{
	if(tooltip_overlay_ob)
		tooltip_overlay_ob->draw = false;
}


void GLUI::unhideTooltip()
{
	if(tooltip_overlay_ob)
		tooltip_overlay_ob->draw = true;
}


TextRendererFontFace* GLUI::getFont(int font_size_px, bool emoji)
{
	if(emoji)
		return emoji_fonts->getFontFaceForSize(font_size_px).ptr();
	else
		return fonts->getFontFaceForSize(font_size_px).ptr();
}


void GLUI::setKeyboardFocusWidget(GLUIWidgetRef widget)
{
	if(widget != key_focus_widget) // If changed:
	{
		if(key_focus_widget.nonNull())
			key_focus_widget->handleLosingKeyboardFocus();

		key_focus_widget = widget;

		if(callbacks)
		{
			if(widget.nonNull())
				callbacks->startTextInput();
			else
				callbacks->stopTextInput();
		}
	}
}


void GLUI::setCurrentDevicePixelRatio(float new_device_pixel_ratio)
{
	// conPrint("GLUI::setCurrentDevicePixelRatio: new_device_pixel_ratio: " + toString(new_device_pixel_ratio));

	device_pixel_ratio = new_device_pixel_ratio;

	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		widget->updateGLTransform(*this);
	}
}
