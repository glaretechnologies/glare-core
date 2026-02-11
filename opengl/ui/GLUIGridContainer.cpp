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
	cell_padding_px(10),
	background_consumes_events(false)
{}


GLUIGridContainer::GLUIGridContainer(GLUI& glui, Reference<OpenGLEngine>& opengl_engine_, const CreateArgs& args_)
{
	gl_ui = &glui;
	opengl_engine = opengl_engine_;
	args = args_;
	m_z = args_.z;

	background_overlay_ob = new OverlayObject();
	background_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	background_overlay_ob->material.albedo_linear_rgb = args.background_colour;
	background_overlay_ob->material.alpha = args.background_alpha;

	opengl_engine->addOverlayObject(background_overlay_ob);
}


GLUIGridContainer::~GLUIGridContainer()
{
	if(background_overlay_ob)
		opengl_engine->removeOverlayObject(background_overlay_ob);
}


void GLUIGridContainer::handleMousePress(MouseEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = gl_ui->UICoordsForOpenGLCoords(event.gl_coords);
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

	const Vec2f coords = gl_ui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIGridContainer::doHandleMouseMoved(MouseEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = gl_ui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIGridContainer::doHandleMouseWheelEvent(MouseWheelEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = gl_ui->UICoordsForOpenGLCoords(event.gl_coords);
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
}


void GLUIGridContainer::updateGLTransform()
{
	// Compute row and column widths.

	const float cell_padding = gl_ui->getUIWidthForDevIndepPixelWidth(args.cell_padding_px);

	std::vector<float> column_widths(cell_widgets.getWidth());

	for(size_t x=0; x<cell_widgets.getWidth(); ++x)
	{
		float w = 0;
		for(size_t y=0; y<cell_widgets.getHeight(); ++y)
		{
			GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
			if(widget)
			{
				const float padded_w = widget->rect.getWidths().x + cell_padding * 2;
				w = myMax(w, padded_w);
			}
		}
		column_widths[x] = w;
	}

	std::vector<float> row_heights(cell_widgets.getHeight());

	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	{
		float h = 0;
		for(size_t x=0; x<cell_widgets.getWidth(); ++x)
		{
			GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
			if(widget)
			{
				const float padded_h = widget->rect.getWidths().y + cell_padding * 2;
				h = myMax(h, padded_h);
			}
		}
		row_heights[y] = h;
	}
		
	// Now iterate over all cells and set positions
	float py = 0;
	for(size_t y=0; y<cell_widgets.getHeight(); ++y)
	{
		const float row_height = row_heights[y];
		
		float px = 0;
		for(size_t x=0; x<cell_widgets.getWidth(); ++x)	
		{
			const float column_width = column_widths[x];
			GLUIWidget* widget = cell_widgets.elem(x, y).ptr();
			if(widget)
			{
				const float y_space = myMax(0.f, row_height - cell_padding * 2 - widget->getRect().getWidths().y); // Compute space around widget to vertically center
				widget->setPosAndDims(/*botleft=*/this->rect.getMin() + Vec2f(px, py + y_space*0.5f) + Vec2f(cell_padding), /*dims=*/Vec2f(column_width, row_height) - Vec2f(cell_padding * 2));

				widget->setClipRegion(this->rect);

				widget->updateGLTransform();
			}

			px += column_width;
		}

		py += row_height;
	}
}


void GLUIGridContainer::setPosAndDims(const Vec2f& botleft, const Vec2f& dims)
{
	rect = Rect2f(botleft, botleft + dims);

	const float y_scale = opengl_engine->getViewPortAspectRatio();

	background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, m_z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}


void GLUIGridContainer::setClipRegion(const Rect2f& clip_rect)
{
	// TODO
}


void GLUIGridContainer::setCellWidget(int cell_x, int cell_y, GLUIWidgetRef widget)
{
	// Enlarge cell_widgets array if needed.
	if(cell_x >= cell_widgets.getWidth() || cell_y >= cell_widgets.getHeight())
	{
		cell_widgets.resize(
			myMax(cell_widgets.getWidth(),  (size_t)cell_x + 1), 
			myMax(cell_widgets.getHeight(), (size_t)cell_y + 1)
		);
	}

	gl_ui->addWidget(widget); // Add widget to GL UI if not already added.

	cell_widgets.elem(cell_x, cell_y) = widget;

	widget->setZ(this->getZ() - 0.001f); // Position in front of the container.
}


void GLUIGridContainer::clear()
{
	cell_widgets.resize(0, 0);
}


float GLUIGridContainer::getCellPaddding() const
{
	return gl_ui->getUIWidthForDevIndepPixelWidth(args.cell_padding_px);
}


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
