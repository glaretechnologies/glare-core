/*=====================================================================
GLUI.cpp
--------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "GLUI.h"


#include "GLUIInertWidget.h"
#include "GLUITextView.h"
#include <graphics/SRGBUtils.h>
#include <maths/mathstypes.h>
#include <limits>
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
	hovered_tooltip_widget(nullptr),
	tooltip_shown(false),
	tooltip_suppressed_by_press(false),
	tooltip_hidden(false)
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
	tooltip_overlay_ob->ob_to_world_matrix = Matrix4f::identity();
	tooltip_overlay_ob->draw = false; // Hide initially

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

	tooltip_text_ob = nullptr;
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

	if(hovered_tooltip.empty() || tooltip_suppressed_by_press || tooltip_hidden || (hovered_tooltip_timer.elapsed() < tooltip_delay_s))
	{
		// Hide tooltip
		if(tooltip_text_ob)
			tooltip_text_ob->setVisible(false);
		tooltip_overlay_ob->draw = false;
		tooltip_shown = false;
		return;
	}

	if(tooltip_text_ob.isNull() || (tooltip_text_ob->getText() != hovered_tooltip))
	{
		// Create/Recreate tooltip_text_ob
		// conPrint("Create/Recreate tooltip_text_ob with text: " + hovered_tooltip);
		GLUIText::CreateArgs args;
		args.font_size_px = tooltip_font_size_px; // In device-independent pixels
		args.z = TOOLTIP_Z;
		args.colour = Colour3f(0.f);
		tooltip_text_ob = new GLUIText(*this, hovered_tooltip, /*botleft=*/Vec2f(0,0), args); // Adds text overlay object to OpenGL engine.
	}

	runtimeCheck(tooltip_text_ob.nonNull());
	

	const float text_height_ui_coords = getUIWidthForDevIndepPixelWidth((float)tooltip_font_size_px);
	const float padding_x_ui = text_height_ui_coords * 0.6f;
	const float padding_y_ui = text_height_ui_coords * 0.6f;

	const Vec2f text_dims_ui_coords = tooltip_text_ob->getLineBoxRect().getWidths();
	const float box_w_ui = text_dims_ui_coords.x + padding_x_ui * 2;
	const float box_h_ui = text_dims_ui_coords.y + padding_y_ui * 2;

	// Position the tooltip below the mouse pointer icon (which in turn is below the mouse position)
	const float mouse_pointer_h_ui = getUIWidthForDevIndepPixelWidth(20.f);

	float pos_x_ui = last_mouse_ui_coords.x;
	float pos_y_ui = last_mouse_ui_coords.y - box_h_ui - mouse_pointer_h_ui;

	if(pos_y_ui < -this->getViewportMinMaxY()) // If tooltip would go off bottom of screen:
		pos_y_ui = last_mouse_ui_coords.y;

	if(pos_x_ui + box_w_ui > 1.f) // If tooltip would go off right of screen:
		pos_x_ui = last_mouse_ui_coords.x - box_w_ui;

	//tooltip_text_ob->setPos(Vec2f(pos_x_ui + padding_x_ui, pos_y_ui + padding_y_ui));
	tooltip_text_ob->setLineBoxBotLeftPos(Vec2f(pos_x_ui + padding_x_ui, pos_y_ui + padding_y_ui));
	tooltip_text_ob->setVisible(true);

	const Vec2f pos_gl   = OpenGLCoordsForUICoords(Vec2f(pos_x_ui, pos_y_ui));
	const Vec2f scale_gl = OpenGLCoordsForUICoords(Vec2f(box_w_ui, box_h_ui));
	tooltip_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(pos_gl.x, pos_gl.y, TOOLTIP_Z + 0.001f) * Matrix4f::scaleMatrix(scale_gl.x, scale_gl.y, 1);
	tooltip_overlay_ob->draw = true;

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

float GLUI::UIYCoordForOpenGLYCoord(float y_gl)
{
	return y_gl / opengl_engine->getViewPortAspectRatio();
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
		tooltip_suppressed_by_press = true;
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
	const GLUIWidget* new_hovered_tooltip_widget = nullptr; // The nearest widget with a tooltip that the mouse is over, if any.
	float new_hovered_tooltip_z = std::numeric_limits<float>::infinity();
	for(auto it = widgets.begin(); it != widgets.end(); ++it)
	{
		GLUIWidget* widget = it->ptr();
		widget->handleMouseMoved(glui_event);

		if(widget->rect.inOpenRectangle(coords) && widget->isVisible()) // If the mouse is over the widget:
		{
			// If the widget has a tooltip, and is nearer than any other widget with a tooltip under the mouse.  (e.g. a widget in a grid or window is nearer than the grid or window)
			if(!widget->tooltip.empty() && (widget->getZ() < new_hovered_tooltip_z))
			{
				new_hovered_tooltip_widget = widget;
				new_hovered_tooltip_z = widget->getZ();
			}

			if(widget->acceptsTextInput())
				new_mouse_over_text_input_widget = true;
		}
	}

	if(glui_event.accepted)
		mouse_event.accepted = true;

	// If the mouse moved onto a different widget with a tooltip (or off all widgets with tooltips), restart the tooltip delay.
	// Compare widgets rather than tooltip text, as different widgets can have the same tooltip, e.g. widgets created by the same script.
	if(new_hovered_tooltip_widget != hovered_tooltip_widget)
	{
		hovered_tooltip_widget = new_hovered_tooltip_widget;
		hovered_tooltip_timer.reset();
		tooltip_suppressed_by_press = false;
	}

	// Update the text even if the widget hasn't changed, in case the widget's tooltip has changed.
	if(new_hovered_tooltip_widget)
		hovered_tooltip = new_hovered_tooltip_widget->tooltip;
	else
		hovered_tooltip.clear();

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
	if(widget.ptr() == hovered_tooltip_widget)
	{
		hovered_tooltip_widget = nullptr;
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

	hovered_tooltip_widget = nullptr;
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


void GLUI::hideTooltip()
{
	tooltip_hidden = true;
	updateTooltip(); // Hides the tooltip, as tooltip_hidden is set.
}


void GLUI::unhideTooltip()
{
	tooltip_hidden = false;
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
