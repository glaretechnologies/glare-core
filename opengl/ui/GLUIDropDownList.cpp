/*=====================================================================
GLUIDropDownList.cpp
--------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "GLUIDropDownList.h"


#include "GLUI.h"
#include "GLUITextButton.h"
#include "../OpenGLMeshRenderData.h"
#include "../../graphics/SRGBUtils.h"
#include "../../utils/ConPrint.h"
#include "../../utils/StringUtils.h"
#include "../../maths/mathstypes.h"


GLUIDropDownList::CreateArgs::CreateArgs()
:	font_size_px(GLUI::getDefaultFontSizePx()),
	sizing_type_x(GLUIWidget::SizingType_FixedSizePx),
	sizing_type_y(GLUIWidget::SizingType_FixedSizePx),
	fixed_size(150.f, std::ceil(GLUI::getDefaultFontSizePx() * 2.4f)),
	background_colour(toLinearSRGB(Colour3f(0.2f))),
	mouseover_background_colour(toLinearSRGB(Colour3f(0.3f))),
	text_colour(Colour3f(0.9f)),
	z(0.f)
{}


GLUIDropDownList::GLUIDropDownList(GLUI& glui_, const CreateArgs& args_)
{
	glui = &glui_;
	opengl_engine = glui_.opengl_engine.ptr();
	tooltip = args_.tooltip;
	args = args_;

	sizing_type_x = args.sizing_type_x;
	sizing_type_y = args.sizing_type_y;
	fixed_size = args.fixed_size;

	// Set cur_index
	this->cur_index = 0;
	for(size_t i=0; i<args.options.size(); ++i)
		if(args.initial_value == args.options[i])
			this->cur_index = i;

	Vec2f dims;
	dims.x = glui->getUIWidthForDevIndepPixelWidth(fixed_size.x);
	dims.y = glui->getUIWidthForDevIndepPixelWidth(fixed_size.y);
	rect = Rect2f(Vec2f(0.f), dims);

	TextureParams tex_params;
	tex_params.allow_compression = false;

	// Decrement button (right side, bottom half) - down arrow
	open_button_overlay_ob = new OverlayObject();
	open_button_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	open_button_overlay_ob->material.albedo_linear_rgb = Colour3f(1.f);//args.button_colour;
	open_button_overlay_ob->material.albedo_texture = opengl_engine->getTexture(opengl_engine->getDataDir() + "/gl_data/ui/spin_down.png", tex_params);
	open_button_overlay_ob->material.tex_matrix = Matrix2f(1, 0, 0, -1);
	opengl_engine->addOverlayObject(open_button_overlay_ob);


	// Text for option
	{
		GLUITextView::CreateArgs text_args;
		text_args.sizing_type_x = GLUIWidget::SizingType_FixedSizePx;
		text_args.sizing_type_y = GLUIWidget::SizingType_FixedSizePx;
		text_args.fixed_size = fixed_size; // TEMP assuming px

		text_args.z = args.z - 0.01f;
		//text_args.rounded_corner_radius_px = 0;

		//text = new GLUILineEdit(*glui, Vec2f(0.f), text_args);
		//text->setText(this->getCurrentValue());
		
		text = new GLUITextView(*glui, /*text=*/this->getCurrentValue(), Vec2f(0.f), text_args);
	}
	
	updateWidgetTransforms();
}


GLUIDropDownList::~GLUIDropDownList()
{
	checkRemoveOverlayObAndSetRefToNull(opengl_engine, open_button_overlay_ob);

	checkRemoveAndDeleteWidget(glui, text);
	checkRemoveAndDeleteWidget(glui, options_grid);
}


void GLUIDropDownList::updateWidgetTransforms()
{
	const float y_scale = opengl_engine->getViewPortAspectRatio();

	const Vec2f botleft = rect.getMin();
	const Vec2f dims = getDims();

	// Open button (down arrow)
	const float open_but_h = dims.y * 0.6f;
	const float x_padding = glui->getUIWidthForDevIndepPixelWidth(4);
	const float y_padding = (dims.y - open_but_h)*0.5f; // 2*padding = (overall widget height - open button height);
	const float open_but_bot_y = botleft.y + y_padding;
	const float but_w = glui->getUIWidthForDevIndepPixelWidth(16);
	const float open_but_left_x = rect.getMax().x - but_w - x_padding;

	open_button_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(open_but_left_x, open_but_bot_y * y_scale, m_z - 0.02f) *
		Matrix4f::scaleMatrix(but_w, open_but_h * y_scale, 1);

	// Line edit: left portion of widget, excluding button column
	//const float btn_col_w = glui->getUIWidthForDevIndepPixelWidth(16);//dims.y;
	text->setPos(botleft);
	//line_edit->setWidth(dims.x - btn_col_w);


	if(options_grid)
	{
		// Call recomputeLayout to compute dimensions of grid.
		options_grid->recomputeLayout();

		// Now position grid in correct place
		options_grid->setPos(getRect().getMin() - Vec2f(0, options_grid->getDims().y));

		// Call recomputeLayout again to fix clip region for buttons.  TEMP HACK
		options_grid->recomputeLayout();
	}
}


void GLUIDropDownList::setValue(const std::string& new_val)
{
	// Set cur_index
	this->cur_index = 0;
	for(size_t i=0; i<args.options.size(); ++i)
		if(new_val == args.options[i])
			this->cur_index = i;

	this->text->setText(getCurrentValue());

	updateWidgetTransforms();

	if(handler_func)
	{
		GLUIDropDownListValueChangedEvent event;
		event.widget = this;
		event.index = this->cur_index;
		event.value = getCurrentValue();
		handler_func(event);
	}
}


//void GLUIDropDownList::setValueNoEvent(double new_val)
//{
//	const double clamped = myClamp(new_val, args.min_value, args.max_value);
//	if(clamped == cur_value)
//		return;
//
//	cur_value = clamped;
//	updateOverlayTransforms();
//}


void GLUIDropDownList::handleMousePress(MouseEvent& event)
{
	//if(!background_ob->draw)
	//	return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inOpenRectangle(coords))
	{
		event.accepted = true;

		glui->setKeyboardFocusWidget(this); // Take keyboard focus, so we can handle uo/down/esc keypresses

		// Create options drop-down UI
		openButtonPressed();
	}
	else
	{
		// Click is complete outside widget.  
		if(options_grid && options_grid->getRect().inOpenRectangle(coords))
		{}
		else
			checkRemoveAndDeleteWidget(glui, options_grid); // User didn't click on options grid, so close it if open
	}
}


void GLUIDropDownList::handleMouseRelease(MouseEvent& event)
{
	/*if(open_button_pressed)
	{
		open_button_pressed = false;
		const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
		updateButtonColours(coords);
	}*/
}


void GLUIDropDownList::handleMouseDoubleClick(MouseEvent& event)
{
	// A mouse double-click event is another press and release.
	handleMousePress(event);
	handleMouseRelease(event);
}


void GLUIDropDownList::doHandleMouseMoved(MouseEvent& event)
{
	//if(!background_ob->draw)
	//	return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	updateButtonColours(coords);

	if(rect.inOpenRectangle(coords))
		event.accepted = true;
}


void GLUIDropDownList::updateButtonColours(const Vec2f& mouse_ui_coords)
{
	if(getRect().inOpenRectangle(mouse_ui_coords))
	{
		text->setBackgroundColour(args.mouseover_background_colour);
	}
	else
	{
		text->setBackgroundColour(args.background_colour);
	}
}


static const size_t MAX_NUM_ROWS = 15; // Use a multi-column layout to avoid columns becoming excessively large when there are a lot of options.


void GLUIDropDownList::openButtonPressed()
{
	if(!options_grid)
	{
		//----------------------------- Create options grid and populate with options -----------------------------
		{
			GLUIGridContainer::CreateArgs grid_args;
			grid_args.z = m_z - 0.05f;
			grid_args.background_colour = this->args.mouseover_background_colour; // Colour3f(1, 0, 0);
			grid_args.interior_cell_x_padding_px = 0;
			grid_args.interior_cell_y_padding_px = 0;
			options_grid = new GLUIGridContainer(*glui, grid_args);
		}

		// Add the option buttons
		options_grid->cell_widgets.resize(/*num cols=*/Maths::roundedUpDivide(args.options.size(), MAX_NUM_ROWS), /*num rows=*/myMin(args.options.size(), MAX_NUM_ROWS));
		for(size_t i=0; i<args.options.size(); ++i)
		{
			// Create button
			GLUITextButton::CreateArgs but_args;
			but_args.background_colour           = this->args.background_colour;
			but_args.mouseover_background_colour = this->args.mouseover_background_colour;
			but_args.text_colour                 = this->args.text_colour;
			but_args.mouseover_text_colour       = this->args.text_colour;
			but_args.sizing_type_x     = GLUIWidget::SizingType_Expanding;

			GLUITextButtonRef button = new GLUITextButton(*glui, /*button text=*/args.options[i], Vec2f(0.f), but_args);
			button->handler_func = [this](GLUICallbackEvent& event) { this->optionButtonPressed(event); };

			options_grid->setCellWidget(i / MAX_NUM_ROWS, i % MAX_NUM_ROWS, button);
		}

		glui->addWidget(options_grid);

		updateWidgetTransforms();
	}
	else
	{
		// Close options grid
		checkRemoveAndDeleteWidget(glui, options_grid);
	}
}


void GLUIDropDownList::optionButtonPressed(GLUICallbackEvent& event)
{
	if(!options_grid)
		return;

	for(size_t i=0; i<args.options.size(); ++i)
	{
		const size_t cell_x = i / MAX_NUM_ROWS;
		const size_t cell_y = i % MAX_NUM_ROWS;

		if((cell_x < options_grid->cell_widgets.getWidth()) && (cell_y < options_grid->cell_widgets.getHeight()) && (options_grid->cell_widgets.elem(cell_x, cell_y).ptr() == event.widget))
		{
			this->cur_index = i;

			this->text->setText(getCurrentValue());

			// call handler for this widget
			if(this->handler_func)
			{
				GLUIDropDownListValueChangedEvent new_event;
				new_event.widget = this;
				new_event.index = i;
				new_event.value = getCurrentValue();
				this->handler_func(new_event);
			}
		}
	}

	checkRemoveAndDeleteWidget(glui, options_grid); // Close/delete options grid
}


void GLUIDropDownList::setNewCurrentIndex(size_t new_cur_index)
{
	if((new_cur_index < args.options.size()) && (new_cur_index != this->cur_index))
	{
		this->cur_index = new_cur_index;

		this->text->setText(args.options[cur_index]);

		// call handler for this widget
		if(this->handler_func)
		{
			GLUIDropDownListValueChangedEvent new_event;
			new_event.widget = this;
			new_event.index = new_cur_index;
			new_event.value = getCurrentValue();
			this->handler_func(new_event);
		}
	}
}


void GLUIDropDownList::doHandleMouseWheelEvent(MouseWheelEvent& event)
{
	if(!open_button_overlay_ob->draw) // If this widget is hidden:
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inOpenRectangle(coords))
	{
		if(event.angle_delta.y > 0.0)
		{
			setNewCurrentIndex((size_t)myMax((int)this->cur_index - 1, 0));
		}
		else if(event.angle_delta.y < 0.0)
		{
			if(args.options.size() > 0)
				setNewCurrentIndex(myMin(this->cur_index + 1, args.options.size() - 1));
		}

		event.accepted = true;
	}
}


void GLUIDropDownList::doHandleKeyPressedEvent(KeyEvent& key_event)
{
	// TODO: highlight buttons selected by up/down keys.

	if(key_event.key == Key::Key_Up)
	{
		setNewCurrentIndex((size_t)myMax((int)this->cur_index - 1, 0));
	}
	else if(key_event.key == Key::Key_Down)
	{
		if(args.options.size() > 0)
			setNewCurrentIndex(myMin(this->cur_index + 1, args.options.size() - 1));
	}
	else if(key_event.key == Key::Key_Escape)
	{
		checkRemoveAndDeleteWidget(glui, options_grid); // Close options if open
	}
}


void GLUIDropDownList::handleLosingKeyboardFocus()
{
	checkRemoveAndDeleteWidget(glui, options_grid); // User didn't click on options grid, so close it if open
}


Vec2f GLUIDropDownList::getMinDims() const
{
	return text->getMinDims();
}


void GLUIDropDownList::setPos(const Vec2f& botleft)
{
	const Vec2f dims = getDims();
	rect = Rect2f(botleft, botleft + dims);
	updateWidgetTransforms();
}


void GLUIDropDownList::setClipRegion(const Rect2f& clip_rect)
{
	open_button_overlay_ob->clip_region  = glui->OpenGLRectCoordsForUICoords(clip_rect);
	text->setClipRegion(clip_rect);
}


void GLUIDropDownList::setZ(float new_z)
{
	m_z = new_z;
	text->setZ(new_z - 0.01f);
	updateWidgetTransforms();
}


void GLUIDropDownList::setVisible(bool visible)
{
	open_button_overlay_ob->draw  = visible;
	text->setVisible(visible);
}


bool GLUIDropDownList::isVisible()
{
	return open_button_overlay_ob->draw;
}


void GLUIDropDownList::viewportResized()
{
	Vec2f dims = getDims();
	if(sizing_type_x == SizingType_FixedSizePx)
		dims.x = glui->getUIWidthForDevIndepPixelWidth(fixed_size.x);
	if(sizing_type_y == SizingType_FixedSizePx)
		dims.y = glui->getUIWidthForDevIndepPixelWidth(fixed_size.y);

	const Vec2f botleft = rect.getMin();
	rect = Rect2f(botleft, botleft + dims);

	text->viewportResized();

	updateWidgetTransforms();
}
