/*=====================================================================
GLUIButton.cpp
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "GLUIButton.h"


#include "GLUI.h"
#include <graphics/SRGBUtils.h>
#include "../OpenGLEngine.h"
#include "../OpenGLMeshRenderData.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


GLUIButton::CreateArgs::CreateArgs()
{
	sizing_type_x = GLUIWidget::SizingType::SizingType_FixedSizeUICoords;
	sizing_type_y = GLUIWidget::SizingType::SizingType_FixedSizeUICoords;
	fixed_size = Vec2f(0.05f, 0.05f);

	z = 0.f;

	toggled_colour = toLinearSRGB(Colour3f(0.7f, 0.8f, 1.f));
	untoggled_colour = Colour3f(1.f);

	mouseover_toggled_colour = toLinearSRGB(Colour3f(0.8f, 0.9f, 1.f));
	mouseover_untoggled_colour = toLinearSRGB(Colour3f(0.9f));

	// For non-toggleable buttons:
	button_colour = Colour3f(1.f);
	pressed_colour = toLinearSRGB(Colour3f(0.7f, 0.8f, 1.f));
	mouseover_button_colour = toLinearSRGB(Colour3f(0.9f));
}


GLUIButton::GLUIButton(GLUI& glui_, const std::string& tex_path, const CreateArgs& args_)
:	toggleable(false),
	toggled(false),
	pressed(false)
{
	glui = &glui_;
	opengl_engine = glui_.opengl_engine.ptr();
	tooltip = args_.tooltip;
	args = args_;

	sizing_type_x = args.sizing_type_x;
	sizing_type_y = args.sizing_type_y;
	fixed_size    = args.fixed_size;
	m_z           = args.z;

	overlay_ob = new OverlayObject();
	overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	overlay_ob->material.albedo_linear_rgb = args.button_colour;
	TextureParams tex_params;
	tex_params.allow_compression = false;
	if(!tex_path.empty())
		overlay_ob->material.albedo_texture = opengl_engine->getTexture(tex_path, tex_params);
	overlay_ob->material.tex_matrix = Matrix2f(1,0,0,-1);
	overlay_ob->material.tex_translation = Vec2f(0, 1);


	const Vec2f botleft(0.f);
	const Vec2f dims = computeDims(/*old dims=*/Vec2f(0.1f));
	rect = Rect2f(botleft, botleft + dims);


	const float y_scale = opengl_engine->getViewPortAspectRatio();

	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, m_z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);

	opengl_engine->addOverlayObject(overlay_ob);


	updateOverlayTransform();
}


GLUIButton::~GLUIButton()
{
	checkRemoveOverlayObAndSetRefToNull(opengl_engine, overlay_ob);
}


void GLUIButton::setTexture(const std::string& tex_path)
{
	TextureParams tex_params;
	tex_params.allow_compression = false;

	if(overlay_ob)
		overlay_ob->material.albedo_texture = opengl_engine->getTexture(tex_path, tex_params);
}


void GLUIButton::setTexture(const OpenGLTextureRef& tex)
{
	if(overlay_ob)
		overlay_ob->material.albedo_texture = tex;
}


void GLUIButton::handleMousePress(GLUIMouseEvent& event)
{
	if(!overlay_ob->draw) // If not visible:
		return;

	if((event.button == MouseButton::Left) && rect.inOpenRectangle(event.ui_coords)) // Only the left button presses the button.
	{
		pressed = true; // The button is clicked when the mouse button is released over it, see handleMouseRelease().
		event.accepted = true;
	}

	updateButtonColour(event.ui_coords);
}


// The button is clicked if the mouse button is released over it after being pressed on it.  Releasing elsewhere cancels the click.
void GLUIButton::handleMouseRelease(GLUIMouseEvent& event)
{
	if(pressed)
	{
		pressed = false;

		if(overlay_ob->draw && rect.inOpenRectangle(event.ui_coords)) // If visible and released over the button:
		{
			event.accepted = true;

			if(toggleable)
				toggled = !toggled;

			if(handler)
			{
				GLUICallbackEvent callback_event;
				callback_event.widget = this;
				handler->eventOccurred(callback_event);
			}
		}

		updateButtonColour(event.ui_coords);
	}
}


// The second click of a double-click arrives as a double-click event instead of a press event, so treat it as a press.
// A release event follows, which clears pressed.
void GLUIButton::handleMousePressCancelled()
{
	pressed = false;
	updateButtonColour(glui->getLastMouseUICoords());
}


void GLUIButton::handleMouseDoubleClick(GLUIMouseEvent& event)
{
	handleMousePress(event);
}


void GLUIButton::doHandleMouseMoved(GLUIMouseEvent& mouse_event)
{
	if(!overlay_ob->draw) // If not visible:
		return;

	if(overlay_ob.nonNull())
	{
		if(rect.inOpenRectangle(mouse_event.ui_coords)) // If mouse over widget:
		{
			mouse_event.accepted = true;
		}
		else
		{
			// For now, allow button to remain pressed even when mouse pointer moves off it.
			// pressed = false;
		}
	}

	updateButtonColour(mouse_event.ui_coords);
}


void GLUIButton::viewportResized()
{
	const Vec2f botleft = getRect().getMin();
	const Vec2f dims = computeDims(getRect().getWidths());
	rect = Rect2f(botleft, botleft + dims);

	updateOverlayTransform();
}


void GLUIButton::setPos(const Vec2f& botleft)
{
	const Vec2f dims = this->getDims();
	rect = Rect2f(botleft, botleft + dims);

	updateOverlayTransform();
}


void GLUIButton::setDims(const Vec2f& new_dims)
{
	const Vec2f botleft = rect.getMin();
	rect = Rect2f(botleft, botleft + new_dims);

	updateOverlayTransform();
}


void GLUIButton::setPosAndDims(const Vec2f& botleft, const Vec2f& new_dims)
{
	rect = Rect2f(botleft, botleft + new_dims);

	updateOverlayTransform();
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


void GLUIButton::updateOverlayTransform()
{
	const Vec2f botleft = this->getRect().getMin();
	const Vec2f dims    = this->getRect().getWidths();

	const float y_scale = opengl_engine->getViewPortAspectRatio();
	const float z = m_z; // -0.9f;
	overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}


void GLUIButton::setButtonColour(const Colour3f& col)
{
	args.button_colour = col;
	updateButtonColour(glui->getLastMouseUICoords());
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
