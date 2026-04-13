/*=====================================================================
GLUISpinBox.cpp
---------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "GLUISpinBox.h"


#include "GLUI.h"
#include "../OpenGLMeshRenderData.h"
#include "../../graphics/SRGBUtils.h"
#include "../../utils/ConPrint.h"
#include "../../utils/StringUtils.h"
#include "../../maths/mathstypes.h"


static const float BUTTON_W_PX = 16;
static const float BUTTON_PADDING_PX = 4;


GLUISpinBox::CreateArgs::CreateArgs()
:	min_value(-1.0e30),
	max_value( 1.0e30),
	initial_value(0.0),
	step(1.0),
	num_decimal_places(3),
	font_size_px(GLUI::getDefaultFontSizePx()),
	sizing_type_x(GLUIWidget::SizingType_FixedSizePx),
	sizing_type_y(GLUIWidget::SizingType_FixedSizePx),
	fixed_size(100.f, std::ceil(GLUI::getDefaultFontSizePx() * 2.4f)),
	background_colour(toLinearSRGB(Colour3f(0.25f))),
	button_colour(toLinearSRGB(Colour3f(1.f))),
	mouseover_button_colour(toLinearSRGB(Colour3f(0.9f))),
	pressed_button_colour(toLinearSRGB(Colour3f(0.2f, 0.4f, 0.7f))),
	text_colour(Colour3f(0.9f))
{}


GLUISpinBox::GLUISpinBox(GLUI& glui_, const CreateArgs& args_)
:	decrement_pressed(false),
	increment_pressed(false)
{
	glui = &glui_;
	opengl_engine = glui_.opengl_engine.ptr();
	tooltip = args_.tooltip;
	args = args_;

	sizing_type_x = args.sizing_type_x;
	sizing_type_y = args.sizing_type_y;
	fixed_size = args.fixed_size;

	cur_value = myClamp(args.initial_value, args.min_value, args.max_value);

	Vec2f dims;
	dims.x = glui->getUIWidthForDevIndepPixelWidth(fixed_size.x);
	dims.y = glui->getUIWidthForDevIndepPixelWidth(fixed_size.y);
	rect = Rect2f(Vec2f(0.f), dims);

	// Background (full widget)
	background_ob = new OverlayObject();
	background_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	background_ob->material.albedo_linear_rgb = args.background_colour;
	opengl_engine->addOverlayObject(background_ob);

	TextureParams tex_params;
	tex_params.allow_compression = false;

	// Decrement button (right side, bottom half) - down arrow
	decrement_ob = new OverlayObject();
	decrement_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	decrement_ob->material.albedo_linear_rgb = args.button_colour;
	decrement_ob->material.albedo_texture = opengl_engine->getTexture(opengl_engine->getDataDir() + "/gl_data/ui/spin_down.png", tex_params);
	decrement_ob->material.tex_matrix = Matrix2f(1, 0, 0, -1);
	opengl_engine->addOverlayObject(decrement_ob);

	// Increment button (right side, top half) - up arrow
	increment_ob = new OverlayObject();
	increment_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	increment_ob->material.albedo_linear_rgb = args.button_colour;
	increment_ob->material.albedo_texture = opengl_engine->getTexture(opengl_engine->getDataDir() + "/gl_data/ui/spin_up.png", tex_params);
	increment_ob->material.tex_matrix = Matrix2f(1, 0, 0, -1);
	opengl_engine->addOverlayObject(increment_ob);

	// Line edit for the value (editable text field)
	{
		GLUILineEdit::CreateArgs le_args;
		le_args.background_colour    = args.background_colour;
		le_args.mouseover_background_colour = args.background_colour;
		le_args.background_alpha     = 0.f; // Transparent - spinbox background_ob shows through
		le_args.text_colour          = args.text_colour;
		le_args.font_size_px         = args.font_size_px;
		le_args.padding_px           = (int)((args.fixed_size.y - args.font_size_px) / 2);
		le_args.rounded_corner_radius_px = 0.f;
		//le_args.width                = dims.x - dims.y; // Exclude button column
		assert(sizing_type_x == GLUIWidget::SizingType_FixedSizePx); // TEMP: assert fixed size in x for now
		le_args.fixed_size.x         = fixed_size.x - BUTTON_W_PX - BUTTON_PADDING_PX*2;
		le_args.sizing_type_x        = GLUIWidget::SizingType_FixedSizePx;
		le_args.z                    = m_z;
		line_edit = new GLUILineEdit(glui_, Vec2f(0.f), le_args);
		line_edit->on_enter_pressed = line_edit->on_losing_keyboard_focus = [this]()
		{
			try
			{
				const double new_val = stringToDouble(line_edit->getText());
				setValue(new_val); // Clamps, updates display, fires handler
			}
			catch(glare::Exception&) // Non-numeric input - revert display to current value
			{
				updateValueText();
			}
		};
		glui->addWidget(line_edit);
	}

	updateValueText();
	updateOverlayTransforms();
}


GLUISpinBox::~GLUISpinBox()
{
	checkRemoveAndDeleteWidget(glui, line_edit);
	checkRemoveOverlayObAndSetRefToNull(opengl_engine, background_ob);
	checkRemoveOverlayObAndSetRefToNull(opengl_engine, decrement_ob);
	checkRemoveOverlayObAndSetRefToNull(opengl_engine, increment_ob);
}


// The clickable button area
Rect2f GLUISpinBox::decrementButtonRect() const
{
	const float x_padding = glui->getUIWidthForDevIndepPixelWidth(BUTTON_PADDING_PX);
	const float btn_col_w = glui->getUIWidthForDevIndepPixelWidth(BUTTON_W_PX);
	return Rect2f(Vec2f(rect.getMax().x - btn_col_w - x_padding*2, rect.getMin().y), Vec2f(rect.getMax().x, rect.centroid().y));
}


// The clickable button area
Rect2f GLUISpinBox::incrementButtonRect() const
{
	const float x_padding = glui->getUIWidthForDevIndepPixelWidth(BUTTON_PADDING_PX);
	const float btn_col_w = glui->getUIWidthForDevIndepPixelWidth(BUTTON_W_PX);
	return Rect2f(Vec2f(rect.getMax().x - btn_col_w - x_padding*2, rect.centroid().y), rect.getMax());
}


void GLUISpinBox::updateValueText()
{
	line_edit->setText(doubleToStringNDecimalPlaces(cur_value, args.num_decimal_places));
}


void GLUISpinBox::updateOverlayTransforms()
{
	const float y_scale = opengl_engine->getViewPortAspectRatio();
	const float z = m_z;

	const Vec2f botleft = rect.getMin();
	const Vec2f dims = getDims();

	// Full background
	background_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) *
		Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);

	// Compute rectangles of visible icons
	const float x_padding = glui->getUIWidthForDevIndepPixelWidth(BUTTON_PADDING_PX);
	const float btn_w = glui->getUIWidthForDevIndepPixelWidth(BUTTON_W_PX);
	const float btn_h = rect.getWidths().y * 0.5f;
	const float btn_left_x = rect.getMax().x - btn_w - x_padding;

	// Decrement button (down arrow)
	decrement_ob->ob_to_world_matrix = translationMulScaleMatrix(Vec4f(btn_left_x, botleft.y * y_scale, z - 0.001f, 0), /*scale_x=*/btn_w, /*scale_y=*/btn_h * y_scale, /*scale_z=*/1.f);

	// Increment button (up arrow)
	increment_ob->ob_to_world_matrix = translationMulScaleMatrix(Vec4f(btn_left_x, rect.centroid().y * y_scale, z - 0.001f, 0), /*scale_x=*/btn_w, /*scale_y=*/btn_h * y_scale, /*scale_z=*/1.f); 

	// Line edit: left portion of widget, excluding button column
	//const float btn_col_w = glui->getUIWidthForDevIndepPixelWidth(16);//dims.y;
	line_edit->setPos(botleft);
	//line_edit->setWidth(dims.x - btn_col_w);
}


void GLUISpinBox::changeValue(double delta)
{
	setValue(cur_value + delta);
}


void GLUISpinBox::setValue(double new_val)
{
	const double clamped = myClamp(new_val, args.min_value, args.max_value);
	if(clamped != cur_value)
	{
		cur_value = clamped;
		updateValueText();
		updateOverlayTransforms();

		if(handler_func)
		{
			GLUISpinBoxValueChangedEvent event;
			event.widget = this;
			event.value = cur_value;
			handler_func(event);
		}
	}
}


void GLUISpinBox::setValueNoEvent(double new_val)
{
	const double clamped = myClamp(new_val, args.min_value, args.max_value);
	if(clamped != cur_value)
	{
		cur_value = clamped;
		updateValueText();
		updateOverlayTransforms();
	}
}


void GLUISpinBox::handleMousePress(MouseEvent& event)
{
	if(!background_ob->draw)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(!rect.inOpenRectangle(coords))
		return;

	if(decrementButtonRect().inOpenRectangle(coords))
	{
		decrement_pressed = true;
		decrement_ob->material.albedo_linear_rgb = args.pressed_button_colour;
		changeValue(-args.step);
		event.accepted = true;
	}
	else if(incrementButtonRect().inOpenRectangle(coords))
	{
		increment_pressed = true;
		increment_ob->material.albedo_linear_rgb = args.pressed_button_colour;
		changeValue(args.step);
		event.accepted = true;
	}
}


void GLUISpinBox::handleMouseRelease(MouseEvent& event)
{
	if(decrement_pressed || increment_pressed)
	{
		decrement_pressed = false;
		increment_pressed = false;
		const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
		updateButtonColours(coords);
	}
}


void GLUISpinBox::handleMouseDoubleClick(MouseEvent& event)
{
	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(!rect.inOpenRectangle(coords))
		return;

	// A mouse double-click event is another press and release.
	if(decrementButtonRect().inOpenRectangle(coords))
	{
		changeValue(-args.step);
		event.accepted = true;
	}
	else if(incrementButtonRect().inOpenRectangle(coords))
	{
		changeValue(args.step);
		event.accepted = true;
	}

	// Change colours to reflect mouse release.
	if(decrement_pressed || increment_pressed)
	{
		decrement_pressed = false;
		increment_pressed = false;
		updateButtonColours(coords);
	}
}


void GLUISpinBox::doHandleMouseMoved(MouseEvent& event)
{
	if(!background_ob->draw)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	updateButtonColours(coords);

	if(rect.inOpenRectangle(coords))
		event.accepted = true;
}


void GLUISpinBox::updateButtonColours(const Vec2f& mouse_ui_coords)
{
	if(decrement_pressed)
	{
		decrement_ob->material.albedo_linear_rgb = args.pressed_button_colour;
	}
	else if(decrementButtonRect().inOpenRectangle(mouse_ui_coords))
	{
		decrement_ob->material.albedo_linear_rgb = args.mouseover_button_colour;
	}
	else
	{
		decrement_ob->material.albedo_linear_rgb = args.button_colour;
	}

	if(increment_pressed)
	{
		increment_ob->material.albedo_linear_rgb = args.pressed_button_colour;
	}
	else if(incrementButtonRect().inOpenRectangle(mouse_ui_coords))
	{
		increment_ob->material.albedo_linear_rgb = args.mouseover_button_colour;
	}
	else
	{
		increment_ob->material.albedo_linear_rgb = args.button_colour;
	}
}


void GLUISpinBox::doHandleMouseWheelEvent(MouseWheelEvent& event)
{
	if(!background_ob->draw)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inOpenRectangle(coords))
	{
		changeValue(args.step * event.angle_delta.y * 0.1);
		event.accepted = true;
	}
}


void GLUISpinBox::handleLosingKeyboardFocus()
{
}


void GLUISpinBox::setPos(const Vec2f& botleft)
{
	const Vec2f dims = getDims();
	rect = Rect2f(botleft, botleft + dims);
	updateOverlayTransforms();
}


void GLUISpinBox::setClipRegion(const Rect2f& clip_rect)
{
	background_ob->clip_region = glui->OpenGLRectCoordsForUICoords(clip_rect);
	decrement_ob->clip_region  = glui->OpenGLRectCoordsForUICoords(clip_rect);
	increment_ob->clip_region  = glui->OpenGLRectCoordsForUICoords(clip_rect);
	line_edit->setClipRegion(clip_rect);
}


void GLUISpinBox::setZ(float new_z)
{
	m_z = new_z;
	line_edit->setZ(new_z - 0.01f);
	updateOverlayTransforms();
}


void GLUISpinBox::setVisible(bool visible)
{
	background_ob->draw = visible;
	decrement_ob->draw  = visible;
	increment_ob->draw  = visible;
	line_edit->setVisible(visible);
}


bool GLUISpinBox::isVisible()
{
	return background_ob->draw;
}


void GLUISpinBox::viewportResized()
{
	Vec2f dims = getDims();
	if(sizing_type_x == SizingType_FixedSizePx)
		dims.x = glui->getUIWidthForDevIndepPixelWidth(fixed_size.x);
	if(sizing_type_y == SizingType_FixedSizePx)
		dims.y = glui->getUIWidthForDevIndepPixelWidth(fixed_size.y);

	const Vec2f botleft = rect.getMin();
	rect = Rect2f(botleft, botleft + dims);

	updateOverlayTransforms();
}
