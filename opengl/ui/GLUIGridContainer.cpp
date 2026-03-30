/*=====================================================================
GLUIGridContainer.cpp
---------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "GLUIGridContainer.h"


#include "GLUI.h"


GLUIGridContainer::CreateArgs::CreateArgs()
:	background_colour(Colour3f(0.7f)),
	background_alpha(1.f),
	z(0.f),
	interior_cell_x_padding_px(5),
	interior_cell_y_padding_px(5),
	exterior_cell_x_padding_px(0),
	exterior_cell_y_padding_px(0),
	background_consumes_events(false)
{}


GLUIGridContainer::GLUIGridContainer(GLUI& glui_, const CreateArgs& args_)
{
	glui = &glui_;
	opengl_engine = glui_.opengl_engine.ptr();
	args = args_;
	m_z = args_.z;

	sizing_type_x = GLUIWidget::SizingType_Expanding;
	sizing_type_y = GLUIWidget::SizingType_Expanding;

	background_overlay_ob = new OverlayObject();
	background_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	background_overlay_ob->material.albedo_linear_rgb = args.background_colour;
	background_overlay_ob->material.alpha = args.background_alpha;

	opengl_engine->addOverlayObject(background_overlay_ob);

	this->rect = Rect2f(Vec2f(0.f), Vec2f(0.4f));

	updateBackgroundOverlayTransform();
}


GLUIGridContainer::~GLUIGridContainer()
{
	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	for(size_t x=0; x<cell_widgets.getWidth(); ++x)	
		checkRemoveAndDeleteWidget(glui, cell_widgets.elem(x, y));

	checkRemoveOverlayObAndSetRefToNull(opengl_engine, background_overlay_ob);
}


void GLUIGridContainer::handleMousePress(MouseEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIGridContainer::handleMouseRelease(MouseEvent& /*event*/)
{
}


void GLUIGridContainer::handleMouseDoubleClick(MouseEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIGridContainer::doHandleMouseMoved(MouseEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIGridContainer::doHandleMouseWheelEvent(MouseWheelEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIGridContainer::doHandleKeyPressedEvent(KeyEvent& /*event*/)
{
}


bool GLUIGridContainer::isVisible()
{
	return background_overlay_ob->draw;
}


void GLUIGridContainer::setVisible(bool visible)
{
	background_overlay_ob->draw = visible;

	// Set contained object visibility
	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	for(size_t x=0; x<cell_widgets.getWidth(); ++x)
	{
		GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
		if(widget)
			widget->setVisible(visible);
	}
}


void GLUIGridContainer::updateGLTransform()
{
	// Call on children first
	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	for(size_t x=0; x<cell_widgets.getWidth(); ++x)
	{
		GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
		if(widget)
			widget->updateGLTransform();
	}

	recomputeLayout();
}


void GLUIGridContainer::recomputeLayout() // For grid containers - call recursively on contained widgets and then place each contained widget at final location.
{
	// Compute row and column widths.

	const float interior_cell_x_padding = glui->getUIWidthForDevIndepPixelWidth(args.interior_cell_x_padding_px);
	const float interior_cell_y_padding = glui->getUIWidthForDevIndepPixelWidth(args.interior_cell_y_padding_px);
	const float exterior_cell_x_padding = glui->getUIWidthForDevIndepPixelWidth(args.exterior_cell_x_padding_px);
	const float exterior_cell_y_padding = glui->getUIWidthForDevIndepPixelWidth(args.exterior_cell_y_padding_px);

	std::vector<float> column_widths(cell_widgets.getWidth());
	float total_width = 0;

	for(size_t x=0; x<cell_widgets.getWidth(); ++x)
	{
		float max_padded_w = 0; // Maximum padded width over all widgets on column x.
		for(size_t y=0; y<cell_widgets.getHeight(); ++y)
		{
			GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
			if(widget)
			{
				widget->recomputeLayout();
				const float left_x_padding  = ((x == 0)                           ? exterior_cell_x_padding : interior_cell_x_padding);
				const float right_x_padding = ((x == cell_widgets.getWidth() - 1) ? exterior_cell_x_padding : interior_cell_x_padding);
				const float padded_w = widget->getDims().x + left_x_padding + right_x_padding;
				max_padded_w = myMax(max_padded_w, padded_w);
			}
		}
		column_widths[x] = max_padded_w;
		total_width += max_padded_w;
	}

	std::vector<float> row_heights(cell_widgets.getHeight());
	float total_height = 0;

	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	{
		float max_padded_h = 0; // Maximum padded height over all widgets on row y
		for(size_t x=0; x<cell_widgets.getWidth(); ++x)
		{
			GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
			if(widget)
			{
				const float top_y_padding   = ((y == 0)                            ? exterior_cell_y_padding : interior_cell_y_padding);
				const float bot_y_padding   = ((y == cell_widgets.getHeight() - 1) ? exterior_cell_y_padding : interior_cell_y_padding);
				const float padded_h = widget->getDims().y + top_y_padding + bot_y_padding;
				max_padded_h = myMax(max_padded_h, padded_h);
			}
		}
		row_heights[y] = max_padded_h;
		total_height += max_padded_h;
	}
		
	// Now iterate over all cells and set positions (from top row to bottom row)
	float py = total_height; // y coordinates relative to this->rect.getMin() of top of row.
	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	{
		const float row_height = row_heights[y];
		//const float row_bot_y = py - row_height;

		float px = 0; // x coordinates relative to this->rect.getMin() of left of column.
		for(size_t x=0; x<cell_widgets.getWidth(); ++x)	
		{
			if((x < (int)col_min_x_px_vals.size()) && col_min_x_px_vals[x] > 0) // If we have a min x pixel val for this column:
				px = myMax(px, glui->getUIWidthForDevIndepPixelWidth(col_min_x_px_vals[x]));

			const float column_width = column_widths[x];
			GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
			if(widget)
			{
				const float left_x_padding  = ((x == 0)                            ? exterior_cell_x_padding : interior_cell_x_padding);
				const float right_x_padding = ((x == cell_widgets.getWidth() - 1)  ? exterior_cell_x_padding : interior_cell_x_padding);
				const float top_y_padding   = ((y == 0)                            ? exterior_cell_y_padding : interior_cell_y_padding);
				//const float bot_y_padding   = ((y == cell_widgets.getHeight() - 1) ? exterior_cell_y_padding : interior_cell_y_padding);
				Vec2f widget_bot_left;
				if(true) // If vert align to top:
				{
					widget_bot_left = this->rect.getMin() + Vec2f(px + left_x_padding, py - top_y_padding - widget->getDims().y);
				}
				else
				{
				//	const float y_space = myMax(0.f, row_height - cell_y_padding * 2 - widget->getDims().y); // Compute space around widget to vertically center
				//	widget_bot_left = this->rect.getMin() + Vec2f(px, row_bot_y + y_space*0.5f) + Vec2f(cell_x_padding, cell_y_padding);
				}

				if(widget->sizing_type_x == SizingType_Expanding) // NOTE: bit of a hack, just checking x sizing type and not y.
				{
					float dims_x = myMax(0.f, column_width - left_x_padding - right_x_padding);
					float dims_y = myMax(0.f, this->rect.getMin().y + py - top_y_padding - widget_bot_left.y);

					widget->setPosAndDims(/*botleft=*/widget_bot_left, /*dims=*/Vec2f(dims_x, dims_y));//max(Vec2f(column_width, row_height) - Vec2f(cell_x_padding * 2, cell_y_padding * 2), Vec2f(0.f)));
				}
				else
					widget->setPos(/*botleft=*/widget_bot_left);
			}

			px += column_width;
		}

		py -= row_height;
	}

	// Update dimensions
	Vec2 dims = this->getDims();
	if(sizing_type_x == GLUIWidget::SizingType_Expanding)
		dims.x = total_width;
	if(sizing_type_y == GLUIWidget::SizingType_Expanding)
		dims.y = total_height;

	const Vec2f botleft = this->getRect().getMin();
	this->rect = Rect2f(botleft, botleft + dims);


	// Set clip region on child widgets
	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	for(size_t x=0; x<cell_widgets.getWidth();  ++x)
	{
		GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
		if(widget)
			widget->setClipRegion(this->rect);
	}


	updateBackgroundOverlayTransform();
}


void GLUIGridContainer::setPos(const Vec2f& botleft)
{
	const Vec2f delta = botleft - getRect().getMin();

	const Vec2f dims = getDims();
	this->rect = Rect2f(botleft, botleft + dims);


	// Move contained items.  Just translate by delta instead of triggering a recomputeLayout.
	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	for(size_t x=0; x<cell_widgets.getWidth(); ++x)	
	{
		GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
		if(widget)
			widget->setPos(widget->getRect().getMin() + delta);
	}


	updateBackgroundOverlayTransform();
}


void GLUIGridContainer::setPosAndDims(const Vec2f& botleft, const Vec2f& dims)
{
	const Vec2f delta = botleft - getRect().getMin();

	rect = Rect2f(botleft, botleft + dims);


	// Move contained items.  Just translate by delta instead of triggering a recomputeLayout.
	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	for(size_t x=0; x<cell_widgets.getWidth(); ++x)	
	{
		GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
		if(widget)
			widget->setPos(widget->getRect().getMin() + delta);
	}


	updateBackgroundOverlayTransform();
}


void GLUIGridContainer::setClipRegion(const Rect2f& clip_rect)
{
	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	for(size_t x=0; x<cell_widgets.getWidth(); ++x)	
	{
		GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
		if(widget)
			widget->setClipRegion(clip_rect);
	}

	if(background_overlay_ob)
		background_overlay_ob->clip_region = glui->OpenGLRectCoordsForUICoords(clip_rect);
}


void GLUIGridContainer::setZ(float new_z)
{
	this->m_z = new_z;

	// Set z on contents also
	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	for(size_t x=0; x<cell_widgets.getWidth(); ++x)	
	{
		if(cell_widgets.elem(x, y))
			cell_widgets.elem(x, y)->setZ(new_z - 0.01f); // Position nearer to cam
	}
}


void GLUIGridContainer::setCellWidget(int cell_x, int cell_y, GLUIWidgetRef widget)
{
	assert(cell_x >= 0 && cell_y >= 0);

	// Enlarge cell_widgets array if needed.
	if(cell_x >= cell_widgets.getWidth() || cell_y >= cell_widgets.getHeight())
	{
		cell_widgets.resize(
			myMax(cell_widgets.getWidth(),  (size_t)cell_x + 1), 
			myMax(cell_widgets.getHeight(), (size_t)cell_y + 1)
		);
	}

	glui->addWidget(widget); // Add widget to GL UI if not already added.

	if(cell_widgets.elem(cell_x, cell_y))
		cell_widgets.elem(cell_x, cell_y)->setParent(nullptr);

	cell_widgets.elem(cell_x, cell_y) = widget;

	widget->setParent(this);

	widget->setZ(this->getZ() - 0.01f); // Position in front of the container.
}


void GLUIGridContainer::addWidgetOnNewRow(GLUIWidgetRef widget)
{
	setCellWidget(0, (int)cell_widgets.getHeight(), widget);
}


void GLUIGridContainer::clear()
{
	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	for(size_t x=0; x<cell_widgets.getWidth(); ++x)	
	{
		GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
		if(widget)
			widget->setParent(nullptr);

		checkRemoveAndDeleteWidget(glui, widget);
	}

	cell_widgets.resize(0, 0);
}


void GLUIGridContainer::containedWidgetChangedSize()
{
	if(m_parent)
		m_parent->containedWidgetChangedSize();
	else
		recomputeLayout();
}


//float GLUIGridContainer::getCellPaddding() const
//{
//	return glui->getUIWidthForDevIndepPixelWidth(args.cell_padding_px);
//}


// Get the rectangle around any non-null widgets in the cells, intersected with the container rectangle (in case of overflow).
Rect2f GLUIGridContainer::getClippedContentRect() const
{
	bool content_rect_inited = false;
	Rect2f content_rect(Vec2f(0.f), Vec2f(0.f));

	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	for(size_t x=0; x<cell_widgets.getWidth(); ++x)	
	{
		GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
		if(widget)
		{
			if(!content_rect_inited)
			{
				content_rect = widget->getRect();
				content_rect_inited = true;
			}
			else
				content_rect.enlargeToHoldRect(widget->getRect());
		}
	}

	if(!content_rect_inited)
		return Rect2f(this->getRect().getMin(), this->getRect().getMin());
	else
		return intersection(content_rect, this->getRect());
}


void GLUIGridContainer::setColumnMinXPx(int column_i, float col_min_x_px)
{
	col_min_x_px_vals.resize(column_i + 1, /*default val=*/-1.f);
	col_min_x_px_vals[column_i] = col_min_x_px;
}


void GLUIGridContainer::updateBackgroundOverlayTransform()
{
	const Vec2f botleft = getRect().getMin();
	const Vec2f dims    = getDims();

	const float y_scale = opengl_engine->getViewPortAspectRatio();
	background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, m_z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}
