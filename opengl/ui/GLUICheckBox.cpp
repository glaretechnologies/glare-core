/*=====================================================================
GLUICheckBox.cpp
----------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "GLUICheckBox.h"


#include "GLUI.h"
#include <graphics/SRGBUtils.h>
#include "../utils/ConPrint.h"


GLUICheckBox::CreateArgs::CreateArgs()
{
	tick_colour = Colour3f(1.f);
	box_colour           = toLinearSRGB(Colour3f(0.3f));
	mouseover_box_colour = toLinearSRGB(Colour3f(0.4f));
	pressed_colour       = toLinearSRGB(Colour3f(0.5f));

	checked = false;
}


GLUICheckBox::GLUICheckBox(GLUI& glui_, Reference<OpenGLEngine>& opengl_engine_, const std::string& tick_texture_path, const Vec2f& botleft, const Vec2f& dims, const CreateArgs& args_)
:	handler(NULL),
	checked(args_.checked),
	pressed(false)
{
	glui = &glui_;
	opengl_engine = opengl_engine_;
	tooltip = args_.tooltip;
	args = args_;
	immutable_dims = false;

	
	tick_overlay_ob = new OverlayObject();
	tick_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	tick_overlay_ob->material.albedo_linear_rgb = args.tick_colour;
	TextureParams tex_params;
	tex_params.allow_compression = false;
	tick_overlay_ob->material.albedo_texture = opengl_engine->getTexture(tick_texture_path, tex_params);
	tick_overlay_ob->material.tex_matrix = Matrix2f(1,0,0,-1);
	tick_overlay_ob->draw = checked;
	
	opengl_engine->addOverlayObject(tick_overlay_ob);


	box_overlay_ob = new OverlayObject();
	box_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	box_overlay_ob->material.albedo_linear_rgb = args.box_colour;

	opengl_engine->addOverlayObject(box_overlay_ob);


	rect = Rect2f(botleft, botleft + dims);

	updateTransforms();
}


GLUICheckBox::~GLUICheckBox()
{
	opengl_engine->removeOverlayObject(tick_overlay_ob);
	opengl_engine->removeOverlayObject(box_overlay_ob);
}


void GLUICheckBox::handleMousePress(MouseEvent& event)
{
	if(!box_overlay_ob->draw) // If not visible:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inOpenRectangle(coords))
	{
		pressed = true;

		checked = !checked;

		tick_overlay_ob->draw = checked;

		if(handler)
		{
			GLUICallbackEvent callback_event;
			callback_event.widget = this;
			handler->eventOccurred(callback_event);
			if(callback_event.accepted)
				event.accepted = true;
		}
	}

	updateColour(coords);
}


void GLUICheckBox::handleMouseDoubleClick(MouseEvent& event)
{
	// Similar to handleMousePress() above except don't set pressed to true.

	if(!box_overlay_ob->draw) // If not visible:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inOpenRectangle(coords))
	{
		checked = !checked;

		pressed = false;

		tick_overlay_ob->draw = checked;

		if(handler)
		{
			GLUICallbackEvent callback_event;
			callback_event.widget = this;
			handler->eventOccurred(callback_event);
			if(callback_event.accepted)
				event.accepted = true;
		}
	}

	updateColour(coords);
}


void GLUICheckBox::handleMouseRelease(MouseEvent& event)
{
	if(pressed)
	{
		pressed = false;

		const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);

		updateColour(coords);
	}
}


void GLUICheckBox::doHandleMouseMoved(MouseEvent& mouse_event)
{
	if(!box_overlay_ob->draw) // If not visible:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(mouse_event.gl_coords);

	if(box_overlay_ob)
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

	updateColour(coords);
}


void GLUICheckBox::setDims(const Vec2f& new_dims)
{
	const Vec2f botleft = rect.getMin();
	rect = Rect2f(botleft, botleft + new_dims);

	updateTransforms();
}


void GLUICheckBox::setPosAndDims(const Vec2f& botleft, const Vec2f& new_dims)
{
	const Vec2f dims = immutable_dims ? rect.getWidths() : new_dims;

	rect = Rect2f(botleft, botleft + dims);

	updateTransforms();
}


void GLUICheckBox::setClipRegion(const Rect2f& clip_rect)
{
	tick_overlay_ob->clip_region = glui->OpenGLRectCoordsForUICoords(clip_rect);
	box_overlay_ob ->clip_region = glui->OpenGLRectCoordsForUICoords(clip_rect);
}


void GLUICheckBox::setChecked(bool checked_)
{
	checked = checked_;

	tick_overlay_ob->draw = isVisible() && checked;
}


void GLUICheckBox::setVisible(bool visible)
{
	tick_overlay_ob->draw = visible && checked;
	box_overlay_ob ->draw = visible;
}


bool GLUICheckBox::isVisible()
{
	return box_overlay_ob->draw;
}


void GLUICheckBox::updateColour(const Vec2f mouse_ui_coords)
{
	// TODO: work out pressed stuff, was having issues with pressed being stuck on after triple clicks etc.
	
	//if(pressed)
	//{
	//	box_overlay_ob->material.albedo_linear_rgb = args.pressed_colour;
	//	conPrint("Setting col to pressed");
	//}
	//else
	{
		if(rect.inOpenRectangle(mouse_ui_coords)) // If mouse over widget:
		{
			box_overlay_ob->material.albedo_linear_rgb = args.mouseover_box_colour;
			//conPrint("Setting col to mouseover");
		}
		else // else if mouse not over widget:
		{
			box_overlay_ob->material.albedo_linear_rgb = args.box_colour;
			//conPrint("Setting col to standard");
		}
	}
}


void GLUICheckBox::updateTransforms()
{
	const float y_scale = opengl_engine->getViewPortAspectRatio();

	const Vec2f botleft = this->getRect().getMin();
	const Vec2f dims    = this->getRect().getWidths();

	tick_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, m_z - 0.0001f) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
	box_overlay_ob ->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, m_z          ) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}
