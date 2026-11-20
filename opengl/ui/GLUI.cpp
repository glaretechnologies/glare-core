/*=====================================================================
GLUI.cpp
--------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "GLUI.h"


#include "GLUIInertWidget.h"
#include "GLUITextView.h"
#include <graphics/SRGBUtils.h>
#include "../OpenGLMeshRenderData.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


static const float TOOLTIP_Z = -0.999f; // -1 is near clip plane
static const int tooltip_font_size_px = 12;
static const double tooltip_delay_s = 0.5; // Time the mouse must be over a widget before its tooltip is shown.


int GLUI::default_font_size_px = 12;


GLUI::GLUI()
:	callbacks(NULL),
	mouse_over_text_input_widget(false),
	stack_allocator(NULL),
	last_mouse_ui_coords(0.f),
	last_mouse_gl_coords(0.f),
	tooltip_shown(false),
	tooltip_suppressed(false)
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
	checkRemoveAndDeleteWidget(this, debug_overlay);

	if(!widgets.empty())
	{
		conPrint("WARNING: " + toString(widgets.size()) + " widgets still in GLUI upon destruction.");
	//	assert(0);
		for(auto it = widgets.begin(); it != widgets.end(); ++it)
		{
			GLUIWidget* widget = it->ptr();
			widget->setGLUI(nullptr); // Set on unremoved widgets when gl_ui is about to be destroyed, so that glui is not a dangling pointer to gl_ui.
		}

		widgets.clear();
	}

	for(int i=0; i<NUM_MOUSE_BUTTONS; ++i)
		pressed_widgets[i] = nullptr;

	checkRemoveOverlayObAndSetRefToNull(opengl_engine.ptr(), tooltip_overlay_ob);

	opengl_engine = NULL;
}


void GLUI::think()
{
	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		widget->think(*this);
	}

	// Show the tooltip once the mouse has been over the widget for long enough.
	if(!tooltip_shown && !hovered_tooltip.empty())
		updateTooltip();
}


// Shows the tooltip for hovered_tooltip at the mouse position, if the mouse has been over the widget for long enough, and no mouse button has been pressed since moving onto it.
// Otherwise moves the tooltip offscreen.
void GLUI::updateTooltip()
{
	if(!tooltip_overlay_ob)
		return;

	if(hovered_tooltip.empty() || tooltip_suppressed || (hovered_tooltip_timer.elapsed() < tooltip_delay_s))
	{
		tooltip_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(1000, 1000, 0); // Move offscreen.
		tooltip_shown = false;
		return;
	}

	// Get or create tooltip texture
	OpenGLTextureRef tex;
	auto res = tooltip_textures.find(hovered_tooltip);
	if(res == tooltip_textures.end())
	{
		tex = makeToolTipTexture(hovered_tooltip);
		tooltip_textures[hovered_tooltip] = tex;
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

	float pos_x = last_mouse_gl_coords.x;
	float pos_y = last_mouse_gl_coords.y - scale_y - mouse_pointer_h_gl;

	if(pos_y < -1.f) // If tooltip would go off bottom of screen:
		pos_y = last_mouse_gl_coords.y;

	if(pos_x + scale_x > 1.f) // If tooltip would go off right of screen:
		pos_x = last_mouse_gl_coords.x - scale_x;

	tooltip_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(pos_x, pos_y, TOOLTIP_Z) * Matrix4f::scaleMatrix(scale_x, scale_y, 1);
	tooltip_shown = true;
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
	inline bool operator() (const GLUIWidgetRef& a, const GLUIWidgetRef b) const
	{
		return a->m_z < b->m_z;
	}
};


// Get list of widgets whose rectangle contains ui_coords, sorted into ascending widget z order (e.g. near to far)
// Use GLUIWidgetRef instead of raw pointers, because some widgets may be removed in event handlers (and have no other references), and therefore we may get dangling references to them
// if not using references.
static void getSortedWidgetsAtEventPos(const std::set<GLUIWidgetRef>& widgets, const Vec2f ui_coords, std::vector<GLUIWidgetRef>& temp_widgets_out)
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


// Returns true if widget is ancestor, or is contained in it, e.g. a button in a grid in ancestor.  Returns false if ancestor is null.
static bool isWidgetOrDescendantOf(const GLUIWidget* widget, const GLUIWidget* ancestor)
{
	for(const GLUIWidget* w = widget; w; w = w->getParent())
		if(w == ancestor)
			return true;
	return false;
}


void GLUI::handleMousePress(MouseEvent& event)
{
	// Hide the tooltip, and don't show it again until the mouse moves onto a different widget, so that it doesn't get in the way when using the widget, e.g. dragging a slider.
	if(!hovered_tooltip.empty())
	{
		tooltip_suppressed = true;
		updateTooltip();
	}

	GLUIMouseEvent glui_event(event, UICoordsForOpenGLCoords(event.gl_coords));

	getSortedWidgetsAtEventPos(widgets, glui_event.ui_coords, temp_widgets);

	// A right-click is first sent to the visible widgets under the mouse as a context menu request, nearest widget first.
	if(glui_event.button == MouseButton::Right)
	{
		for(size_t i=0; i<temp_widgets.size(); ++i)
		{
			if(temp_widgets[i]->isVisible())
			{
				// Hold a reference to the old focus widget, so that it can't be destroyed and a new widget, e.g. a new context menu, created at the same address while handling the request.
				const GLUIWidgetRef old_key_focus_widget = key_focus_widget;

				temp_widgets[i]->contextMenuRequested(glui_event);
				if(glui_event.accepted)
				{
					event.accepted = true;

					if(key_focus_widget == old_key_focus_widget) // If the handler didn't give keyboard focus to a widget, such as a context menu:
						setKeyboardFocusWidget(NULL); // Remove keyboard focus from any widgets that had it.

					temp_widgets.clear();
					return;
				}
			}
		}
	}

	for(size_t i=0; i<temp_widgets.size(); ++i)
	{
		temp_widgets[i]->handleMousePress(glui_event);
		if(glui_event.accepted)
		{
			event.accepted = true;

			setPressedWidgetForMouseButton(glui_event.button, temp_widgets[i]); // This widget gets the following release of the button.

			// If a widget outside the one with keyboard focus accepted the click, remove keyboard focus from it.
			// Widgets inside the focus widget, e.g. the items of an open menu, don't remove focus, as that would close the menu before the item is clicked on release.
			if(!isWidgetOrDescendantOf(temp_widgets[i].ptr(), key_focus_widget.ptr()))
				setKeyboardFocusWidget(NULL);

			temp_widgets.clear();
			return;
		}
	}
	temp_widgets.clear();

	// No widget accepted the click event.  Remove keyboard focus from any widgets that had it.
	setKeyboardFocusWidget(NULL);
}


// Returns the index into pressed_widgets for the button, or -1 if presses of the button aren't tracked, e.g. for MouseButton::None.
static int mouseButtonIndex(MouseButton button)
{
	switch(button)
	{
	case MouseButton::Left:    return 0;
	case MouseButton::Middle:  return 1;
	case MouseButton::Right:   return 2;
	case MouseButton::Back:    return 3;
	case MouseButton::Forward: return 4;
	default:                   return -1;
	}
}


// Makes the widget receive the following release of the button.
// If another widget is still waiting for the release of an earlier press of the same button, e.g. because that release was never received, cancels that press.
void GLUI::setPressedWidgetForMouseButton(MouseButton button, const GLUIWidgetRef& widget)
{
	const int index = mouseButtonIndex(button);
	if(index >= 0)
	{
		if(pressed_widgets[index] && (pressed_widgets[index] != widget))
		{
			// Take the reference out of the array first, which also keeps the widget alive if the handler removes it from the UI.
			const GLUIWidgetRef old_widget = pressed_widgets[index];
			pressed_widgets[index] = NULL;
			old_widget->handleMousePressCancelled();
		}

		pressed_widgets[index] = widget;
	}
}


void GLUI::cancelMousePress()
{
	for(int i=0; i<NUM_MOUSE_BUTTONS; ++i)
	{
		// Take the reference out of the array first, which also keeps the widget alive if the handler removes it from the UI.
		const GLUIWidgetRef widget = pressed_widgets[i];
		pressed_widgets[i] = NULL;

		if(widget)
			widget->handleMousePressCancelled();
	}
}


void GLUI::handleMouseRelease(MouseEvent& event)
{
	// The release only goes to the widget that accepted the press of the same button, e.g. so that a button pressed and then released elsewhere still gets the release.
	const int index = mouseButtonIndex(event.button);
	if((index >= 0) && pressed_widgets[index])
	{
		GLUIMouseEvent glui_event(event, UICoordsForOpenGLCoords(event.gl_coords));

		// Take the reference out of the array first, which also keeps the widget alive if a release handler removes it from the UI.
		const GLUIWidgetRef widget = pressed_widgets[index];
		pressed_widgets[index] = NULL;

		widget->handleMouseRelease(glui_event);
		if(glui_event.accepted)
			event.accepted = true;
	}
}


void GLUI::handleMouseDoubleClick(MouseEvent& event)
{
	GLUIMouseEvent glui_event(event, UICoordsForOpenGLCoords(event.gl_coords));

	getSortedWidgetsAtEventPos(widgets, glui_event.ui_coords, temp_widgets);

	for(size_t i=0; i<temp_widgets.size(); ++i)
	{
		temp_widgets[i]->handleMouseDoubleClick(glui_event);
		if(glui_event.accepted)
		{
			event.accepted = true;
			setPressedWidgetForMouseButton(glui_event.button, temp_widgets[i]); // This widget gets the following release of the button.
			break;
		}
	}
	temp_widgets.clear();
}


bool GLUI::handleMouseWheelEvent(MouseWheelEvent& event)
{
	GLUIMouseWheelEvent glui_event(event, UICoordsForOpenGLCoords(event.gl_coords));

	getSortedWidgetsAtEventPos(widgets, glui_event.ui_coords, temp_widgets);
	for(size_t i=0; i<temp_widgets.size(); ++i)
	{
		temp_widgets[i]->handleMouseWheelEvent(glui_event);
		if(glui_event.accepted)
		{
			event.accepted = true;
			temp_widgets.clear();
			return true;
		}
	}
	temp_widgets.clear();

	return false;
}


bool GLUI::handleMouseMoved(MouseEvent& mouse_event)
{
	const Vec2f coords = UICoordsForOpenGLCoords(mouse_event.gl_coords);

	this->last_mouse_ui_coords = coords;

	this->last_mouse_gl_coords = mouse_event.gl_coords;

	GLUIMouseEvent glui_event(mouse_event, coords);

	bool new_mouse_over_text_input_widget = false;
	const std::string* new_hovered_tooltip = NULL; // Tooltip of the widget the mouse is over, if any.
	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		widget->handleMouseMoved(glui_event);

		if(widget->rect.inOpenRectangle(coords) && widget->isVisible()) // If the mouse is over the widget:
		{
			if(!widget->tooltip.empty()) // And the widget has a tooltip:
				new_hovered_tooltip = &widget->tooltip;

			if(widget->acceptsTextInput())
				new_mouse_over_text_input_widget = true;
		}
	}

	if(glui_event.accepted)
		mouse_event.accepted = true;

	// If the mouse moved onto a widget with a different tooltip (or off any widget with a tooltip), restart the tooltip delay.
	static const std::string empty_string;
	const std::string& new_tooltip = new_hovered_tooltip ? *new_hovered_tooltip : empty_string;
	if(new_tooltip != hovered_tooltip)
	{
		hovered_tooltip = new_tooltip;
		hovered_tooltip_timer.reset();
		tooltip_suppressed = false;
	}

	updateTooltip();

	// Update mouse_over_text_input_widget
	if(new_mouse_over_text_input_widget != mouse_over_text_input_widget)
	{
		if(callbacks)
			callbacks->setMouseCursor(new_mouse_over_text_input_widget ? GLUICallbacks::MouseCursor_IBeam : GLUICallbacks::MouseCursor_Arrow);

		mouse_over_text_input_widget = new_mouse_over_text_input_widget;
	}


	// Update debug overlay with details of the closest widget under the mouse pointer.
	if(debug_overlay)
	{
		getSortedWidgetsAtEventPos(widgets, coords, temp_widgets);

		if(!temp_widgets.empty())
		{
			GLUIWidget* widget = temp_widgets[0].ptr();

			std::string str = widget->className() + " ('" + widget->debug_name + "')\n";
			str += "min: " + widget->getRect().getMin().toStringMaxNDecimalPlaces(3) + "\n";
			str += "dims: " + widget->getDims().toStringMaxNDecimalPlaces(3) + "\n";
			debug_overlay->setText(str);
		}
		else
		{
			debug_overlay->setText("");
		}
		debug_overlay->setPos(this->viewportTopRight() - debug_overlay->getDims()); // Position at top right of viewport


		temp_widgets.clear();
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
		widget->viewportResized();
	}
}


void GLUI::removeWidget(const GLUIWidgetRef& widget)
{
	if(widget == key_focus_widget)
		removeKeyboardFocusForWidgetBeingRemoved();

	for(int i=0; i<NUM_MOUSE_BUTTONS; ++i)
		if(pressed_widgets[i] == widget)
			pressed_widgets[i] = nullptr; // A removed widget doesn't get mouse releases.

	// Hide the tooltip if it is for this widget, so that it isn't left showing until the mouse moves.
	if(!hovered_tooltip.empty() && (widget->tooltip == hovered_tooltip))
	{
		hovered_tooltip.clear();
		updateTooltip();
	}

	widgets.erase(widget);
}


void GLUI::removeAllWidgets()
{
	if(key_focus_widget)
		removeKeyboardFocusForWidgetBeingRemoved();

	for(int i=0; i<NUM_MOUSE_BUTTONS; ++i)
		pressed_widgets[i] = nullptr;

	hovered_tooltip.clear();
	updateTooltip();

	widgets.clear();
}


// Unlike setKeyboardFocusWidget(NULL), doesn't call handleLosingKeyboardFocus() on the widget, as it is being removed.
// This is to avoid handlers like GLUILineEdit::on_losing_keyboard_focus firing as an object is removed from the GLUI and destroyed.
void GLUI::removeKeyboardFocusForWidgetBeingRemoved()
{
	key_focus_widget = nullptr;

	if(callbacks)
		callbacks->stopTextInput();
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


Vec2f GLUI::getUIWidthForDevIndepPixelWidths(Vec2f pixel_w)
{
	return Vec2f(getUIWidthForDevIndepPixelWidth(pixel_w.x), getUIWidthForDevIndepPixelWidth(pixel_w.y));
}


// ui_width = 2 * device_pixel_ratio * pixel_w / (float)opengl_engine->getViewPortWidth();
// pixel_w = ui_width * opengl_engine->getViewPortWidth() / (2 * device_pixel_ratio)

float GLUI::getDevIndepPixelWidthForUIWidth(float ui_width)
{
	return ui_width * (float)opengl_engine->getViewPortWidth() / (2 * device_pixel_ratio * ui_scale);
}


OpenGLTextureRef GLUI::makeToolTipTexture(const std::string& tooltip_text)
{
	TextRendererFontFaceRef font       = fonts      ->getFontFaceForSize((int)((float)tooltip_font_size_px * this->device_pixel_ratio * ui_scale));
	TextRendererFontFaceRef emoji_font = emoji_fonts->getFontFaceForSize((int)((float)tooltip_font_size_px * this->device_pixel_ratio * ui_scale));

	const TextRenderer::SizeInfo size_info = font->renderer->getTextSize(tooltip_text, font.ptr(), emoji_font.ptr());

	const int use_font_height = size_info.max_bounds.y; //text_renderer_font->getFontSizePixels();
	const int padding_x = (int)(use_font_height * 0.6f);
	const int padding_y = (int)(use_font_height * 0.6f);
	

	ImageMapUInt8Ref map = new ImageMapUInt8(size_info.glyphSize().x + padding_x * 2, use_font_height + padding_y * 2, 3);
	map->set(240); // Set to light grey colour

	font->renderer->drawText(*map, tooltip_text, padding_x, padding_y + use_font_height, Colour3f(0.05f), /*render SDF=*/false, font.ptr(), emoji_font.ptr());


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
		widget->viewportResized();
	}
}


void GLUI::setDebugOverlayEnabled(bool enabled)
{
	if(enabled)
	{
		GLUITextView::CreateArgs args;
		debug_overlay = new GLUITextView(*this, "debug overlay", args);
		this->addWidget(debug_overlay);
	}
	else
	{
		checkRemoveAndDeleteWidget(this, debug_overlay);
	}
}
