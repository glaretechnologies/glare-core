/*=====================================================================
GLUISlider.cpp
--------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "GLUISlider.h"


#include "GLUI.h"
#include "../OpenGLMeshRenderData.h"
#include "../graphics/SRGBUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


GLUISlider::CreateArgs::CreateArgs()
{
	knob_colour = toLinearSRGB(Colour3f(0.3f, 0.5f, 0.8f));
	grabbed_knob_colour = toLinearSRGB(Colour3f(0.85f));
	mouseover_knob_colour = toLinearSRGB(Colour3f(0.7f));
	track_colour = toLinearSRGB(Colour3f(0.9f));

	min_value = 0.0;
	max_value = 1.0;
	initial_value = 0.5;

	scroll_speed = 1.0;
}


GLUISlider::GLUISlider(GLUI& glui_, Reference<OpenGLEngine>& opengl_engine_, const Vec2f& botleft, const Vec2f& dims, const CreateArgs& args_)
:	handler(nullptr)
{
	glui = &glui_;
	opengl_engine = opengl_engine_;
	tooltip = args_.tooltip;
	args = args_;

	m_botleft = botleft;
	m_dims = dims;

	cur_value = args_.initial_value;

	knob_grabbed = false;
	knob_grab_x = 0;
	knob_grab_value = 0;

	track_ob = new OverlayObject();
	track_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	track_ob->material.albedo_linear_rgb = args.track_colour;
	track_ob->ob_to_world_matrix = Matrix4f::identity();
	opengl_engine->addOverlayObject(track_ob);

	knob_ob = new OverlayObject();
	knob_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	knob_ob->material.albedo_linear_rgb = args.knob_colour;
	knob_ob->ob_to_world_matrix = Matrix4f::identity();
	opengl_engine->addOverlayObject(knob_ob);

	setPosAndDims(botleft, dims);
}


GLUISlider::~GLUISlider()
{
	if(track_ob) opengl_engine->removeOverlayObject(track_ob);
	if(knob_ob)  opengl_engine->removeOverlayObject(knob_ob);
}


void GLUISlider::handleMousePress(MouseEvent& event)
{
	if(!track_ob->draw) // If not visible:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inOpenRectangle(coords))
	{
		const Rect2f knob_rect = computeKnobRect();
		if(knob_rect.inOpenRectangle(coords))
		{
			// conPrint("grabbed knob!");

			knob_grabbed = true;
			knob_grab_x = coords.x;
			knob_grab_value = cur_value;
			updateKnobColour(coords);
		}
		else
		{
			handleClickOnTrack(coords);
		}

		event.accepted = true;
	}
}


void GLUISlider::handleMouseRelease(MouseEvent& event)
{
	if(knob_grabbed)
	{
		knob_grabbed = false;

		const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
		updateKnobColour(coords);
	}
}


void GLUISlider::handleMouseDoubleClick(MouseEvent& event)
{
	if(!track_ob->draw) // If not visible:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inOpenRectangle(coords))
	{
		handleClickOnTrack(coords);
		event.accepted = true;
	}
}


void GLUISlider::handleClickOnTrack(const Vec2f coords)
{
	const Rect2f knob_rect = computeKnobRect();
	const double delta_frac = 0.1;
	if(coords.x < knob_rect.getMin().x)
	{
		// Move knob left
		cur_value = myClamp(cur_value - delta_frac * (args.max_value - args.min_value), args.min_value, args.max_value);
	}
	else if(coords.x > knob_rect.getMax().x)
	{
		// Move knob right
		cur_value = myClamp(cur_value + delta_frac * (args.max_value - args.min_value), args.min_value, args.max_value);
	}

	// Emit slider value changed event
	if(handler)
	{
		GLUISliderValueChangedEvent callback_event;
		callback_event.widget = this;
		callback_event.value = cur_value;
		handler->sliderValueChangedEventOccurred(callback_event);
	}

	// Update knob transform
	setPosAndDims(m_botleft, m_dims);
}


void GLUISlider::doHandleMouseMoved(MouseEvent& mouse_event)
{
	if(!track_ob->draw) // If not visible:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(mouse_event.gl_coords);

	if(knob_grabbed)
	{
		// Move knob
		float delta_x = coords.x - knob_grab_x;
		const float knob_w = glui->getUIWidthForDevIndepPixelWidth(10);
		const float use_track_w = myMax(0.f, this->rect.getWidths().x - knob_w);

		cur_value = myClamp(knob_grab_value + delta_x * (args.max_value - args.min_value) / use_track_w, args.min_value, args.max_value);

		// conPrint("cur_value changed to " + toString(cur_value));

		// Emit slider value changed event
		if(handler)
		{
			GLUISliderValueChangedEvent callback_event;
			callback_event.widget = this;
			callback_event.value = cur_value;
			handler->sliderValueChangedEventOccurred(callback_event);
		}

		// Update knob transform
		setPosAndDims(m_botleft, m_dims);

		mouse_event.accepted = true;
	}

	updateKnobColour(coords);
}


void GLUISlider::doHandleMouseWheelEvent(MouseWheelEvent& event)
{
	if(!track_ob->draw) // If not visible:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inOpenRectangle(coords))
	{
		cur_value = myClamp(cur_value + (args.max_value - args.min_value) * 0.0025 * args.scroll_speed * event.angle_delta.y, args.min_value, args.max_value);

		// Emit slider value changed event
		if(handler)
		{
			GLUISliderValueChangedEvent callback_event;
			callback_event.widget = this;
			callback_event.value = cur_value;
			handler->sliderValueChangedEventOccurred(callback_event);
		}

		// Update knob transform
		setPosAndDims(m_botleft, m_dims);

		event.accepted = true;
	}
}


void GLUISlider::setValue(double new_val) // Set value but don't emit a value changed event.
{
	cur_value = myClamp(new_val, args.min_value, args.max_value);

	// Emit slider value changed event
	if(handler)
	{
		GLUISliderValueChangedEvent callback_event;
		callback_event.widget = this;
		callback_event.value = cur_value;
		handler->sliderValueChangedEventOccurred(callback_event);
	}

	// Update knob transform
	setPosAndDims(m_botleft, m_dims);
}


void GLUISlider::setValueNoEvent(double new_val)
{
	cur_value = myClamp(new_val, args.min_value, args.max_value);

	// Update knob transform
	setPosAndDims(m_botleft, m_dims);
}


void GLUISlider::setPosAndDims(const Vec2f& botleft, const Vec2f& dims)
{
	m_botleft = botleft;
	m_dims = dims;
	rect = Rect2f(botleft, botleft + dims);

	const float y_scale = opengl_engine->getViewPortAspectRatio();

	const float track_z = 0.f;
	const float track_h = glui->getUIWidthForDevIndepPixelWidth(6);
	const float track_vert_margin = (dims.y - track_h) / 2;
	track_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, (botleft.y + track_vert_margin) * y_scale, track_z) * Matrix4f::scaleMatrix(dims.x, track_h * y_scale, 1);

	const Rect2f knob_rect = computeKnobRect();
	const float knob_z = -0.01f;
	knob_ob->ob_to_world_matrix = Matrix4f::translationMatrix(knob_rect.getMin().x, knob_rect.getMin().y * y_scale, knob_z) * Matrix4f::scaleMatrix(knob_rect.getWidths().x, knob_rect.getWidths().y * y_scale, 1);
}


void GLUISlider::setVisible(bool visible)
{
	track_ob->draw = visible;
	knob_ob->draw = visible;
}


bool GLUISlider::isVisible()
{
	return track_ob->draw;
}


void GLUISlider::updateKnobColour(const Vec2f mouse_ui_coords)
{
	if(rect.inOpenRectangle(mouse_ui_coords) && computeKnobRect().inOpenRectangle(mouse_ui_coords)) // If mouse over knob:
	{
		knob_ob->material.albedo_linear_rgb = knob_grabbed ? args.grabbed_knob_colour : args.mouseover_knob_colour;
	}
	else // else if mouse not over knob:
	{
		knob_ob->material.albedo_linear_rgb = knob_grabbed ? args.grabbed_knob_colour : args.knob_colour;
	}
}


Rect2f GLUISlider::computeKnobRect()
{
	const float knob_w = glui->getUIWidthForDevIndepPixelWidth(10);
	const float use_track_w = myMax(0.f, this->rect.getWidths().x - knob_w);
	const float knob_x = this->rect.getMin().x + ((float)cur_value - (float)args.min_value) / (float)(args.max_value - args.min_value) * use_track_w;
	return Rect2f(Vec2f(knob_x, this->rect.getMin().y), Vec2f(knob_x + knob_w, this->rect.getMax().y));
}
