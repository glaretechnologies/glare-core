/*=====================================================================
GLUICollapsableGroupBox.cpp
---------------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "GLUICollapsableGroupBox.h"


#include "GLUI.h"


GLUICollapsableGroupBox::CreateArgs::CreateArgs()
:	title_text_colour(Colour3f(0.9f)),
	background_colour(Colour3f(0.7f)),
	background_alpha(1.f),
	z(0.f),
	padding_px(10),
	background_consumes_events(false)
{}


GLUICollapsableGroupBox::GLUICollapsableGroupBox(GLUI& glui_, const CreateArgs& args_)
:	handler(nullptr)
{
	glui = &glui_;
	opengl_engine = glui_.opengl_engine.ptr();
	args = args_;
	m_z = args_.z;
	expanded = true;

	sizing_type_x = GLUIWidget::SizingType_Expanding;
	sizing_type_y = GLUIWidget::SizingType_Expanding;

	//--------------- Create background ob ---------------
	background_overlay_ob = new OverlayObject();
	background_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	background_overlay_ob->material.albedo_linear_rgb = args.background_colour;
	background_overlay_ob->material.alpha = args.background_alpha;

	opengl_engine->addOverlayObject(background_overlay_ob);

	//--------------- Create title text view ---------------
	{
		GLUITextView::CreateArgs text_view_args;
		text_view_args.background_alpha = 0;
		text_view_args.text_colour = args.title_text_colour;
		text_view_args.z = m_z - 0.01f;

		title_text = new GLUITextView(*glui, args.title, Vec2f(0), text_view_args);

		glui->addWidget(title_text);
	}

	//--------------- Create collapse and expand buttons ---------------
	{
		GLUIButton::CreateArgs button_args;
		button_args.sizing_type_x = GLUIWidget::SizingType_FixedSizePx;
		button_args.sizing_type_y = GLUIWidget::SizingType_FixedSizePx;
		button_args.fixed_size = Vec2f(22.f); 
		button_args.tooltip = "Collapse";
		collapse_expand_button = new GLUIButton(*glui, opengl_engine->getDataDir() + "/gl_data/ui/expanded.png", button_args);
		collapse_expand_button->setZ(m_z - 0.01f);
		collapse_expand_button->handler = this;
		glui->addWidget(collapse_expand_button);
	}


	this->rect = Rect2f(Vec2f(0.f), Vec2f(0.4f));

	updateWidgetTransforms();

	updateBackgroundOverlayTransform();
}


GLUICollapsableGroupBox::~GLUICollapsableGroupBox()
{
	checkRemoveAndDeleteWidget(glui, body_widget);
	checkRemoveAndDeleteWidget(glui, title_text);
	checkRemoveAndDeleteWidget(glui, collapse_expand_button);

	checkRemoveOverlayObAndSetRefToNull(opengl_engine, background_overlay_ob);
}


void GLUICollapsableGroupBox::setBodyWidget(const GLUIWidgetRef body_widget_)
{
	body_widget = body_widget_;

	body_widget->setParent(this);
	body_widget->setZ(m_z - 0.01f);

	glui->addWidget(body_widget); // Add body_widget to GL UI if not already added.

	recomputeLayout();
}


void GLUICollapsableGroupBox::handleMousePress(MouseEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUICollapsableGroupBox::handleMouseRelease(MouseEvent& /*event*/)
{
}


void GLUICollapsableGroupBox::handleMouseDoubleClick(MouseEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUICollapsableGroupBox::doHandleMouseMoved(MouseEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUICollapsableGroupBox::doHandleMouseWheelEvent(MouseWheelEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUICollapsableGroupBox::doHandleKeyPressedEvent(KeyEvent& /*event*/)
{
}


bool GLUICollapsableGroupBox::isVisible()
{
	return background_overlay_ob->draw;
}


void GLUICollapsableGroupBox::setVisible(bool visible)
{
	background_overlay_ob->draw = visible;

	if(body_widget)
		body_widget->setVisible(visible);

	title_text->setVisible(visible);
	collapse_expand_button->setVisible(visible);
}


void GLUICollapsableGroupBox::viewportResized()
{
	if(body_widget)
		body_widget->viewportResized();

	recomputeLayout();
}


static const float title_bar_h_px = 32;


void GLUICollapsableGroupBox::recomputeLayout()
{
	if(body_widget)
		body_widget->recomputeLayout();

	//const Vec2f top_left = Vec2f(this->getRect().getMin().x, this->getRect().getMax().y);

	const float title_bar_w = collapse_expand_button->getDims().x + title_text->getMinDims().x * 1.3f;
	const float title_bar_h = glui->getUIWidthForDevIndepPixelWidth(title_bar_h_px);
	const float padding = glui->getUIWidthForDevIndepPixelWidth(args.padding_px);

	const Vec2f body_dims = (body_widget && expanded) ? (body_widget->getDims() + Vec2f(padding * 2)) : Vec2f(0.f);

	const float width  = myMax(title_bar_w, body_dims.x);
	const float height = title_bar_h + body_dims.y;


	// Update dimensions
	Vec2 dims = this->getDims();
	if(sizing_type_x == GLUIWidget::SizingType_Expanding)
		dims.x = width;
	if(sizing_type_y == GLUIWidget::SizingType_Expanding)
		dims.y = height;

	const Vec2f botleft = this->getRect().getMin();
	this->rect = Rect2f(botleft, botleft + dims);

	updateWidgetTransforms();
}


Vec2f GLUICollapsableGroupBox::getMinDims() const
{
	const float title_bar_w = collapse_expand_button->getDims().x + title_text->getMinDims().x * 1.3f;
	const float title_bar_h = glui->getUIWidthForDevIndepPixelWidth(title_bar_h_px);
	const float padding = glui->getUIWidthForDevIndepPixelWidth(args.padding_px);

	const Vec2f body_min_dims = (body_widget && expanded) ? (body_widget->getMinDims() + Vec2f(padding * 2)) : Vec2f(0.f);

	const float width  = myMax(title_bar_w, body_min_dims.x);
	const float height = title_bar_h + body_min_dims.y;

	return Vec2f(width, height);
}


void GLUICollapsableGroupBox::setPos(const Vec2f& botleft)
{
	const Vec2f dims = getDims();
	this->rect = Rect2f(botleft, botleft + dims);

	updateWidgetTransforms();
}


void GLUICollapsableGroupBox::setClipRegion(const Rect2f& /*clip_rect*/)
{
	// TODO
}


void GLUICollapsableGroupBox::setZ(float new_z)
{
	this->m_z = new_z;

	if(title_text)
		title_text->setZ(new_z - 0.01f); // Position nearer to cam

	if(collapse_expand_button)
		collapse_expand_button->setZ(new_z - 0.01f); // Position nearer to cam

	if(body_widget)
		body_widget->setZ(new_z - 0.01f); // Position nearer to cam
}


void GLUICollapsableGroupBox::containedWidgetChangedSize()
{
	if(m_parent)
		m_parent->containedWidgetChangedSize();
	else
		recomputeLayout();
}


void GLUICollapsableGroupBox::eventOccurred(GLUICallbackEvent& ev)
{
	if(ev.widget == collapse_expand_button.ptr())
	{
		if(!expanded)
		{
			// Expand
			collapse_expand_button->setTexture(opengl_engine->getDataDir() + "/gl_data/ui/expanded.png");
			collapse_expand_button->setTooltip("Collapse");

			body_widget->setVisible(true);

			expanded = true;
		}
		else
		{
			// Collapse
			collapse_expand_button->setTexture(opengl_engine->getDataDir() + "/gl_data/ui/collapsed.png");
			collapse_expand_button->setTooltip("Expand");

			body_widget->setVisible(false);

			expanded = false;
		}

		if(m_parent)
			m_parent->containedWidgetChangedSize();
		else
			recomputeLayout();
	}
}


void GLUICollapsableGroupBox::updateWidgetTransforms()
{
	const float title_bar_h = glui->getUIWidthForDevIndepPixelWidth(title_bar_h_px);

	const float padding = glui->getUIWidthForDevIndepPixelWidth(args.padding_px);


	if(body_widget)
	{
		const Vec2f body_botleft = this->getRect().getMin() + Vec2f(padding);
		body_widget->setPos(body_botleft);

		const Vec2f clip_botleft = this->getRect().getMin() + Vec2f(padding);
		const Vec2f clip_topright = max(clip_botleft, this->getRect().getMax() - Vec2f(padding, title_bar_h));
		//body_widget->setClipRegion(Rect2f(clip_botleft, clip_topright));
	}

	{
		const float title_x = this->getRect().getMin().x + getDims().x/2.f - title_text->getDims().x/2.f;
		const float title_y = this->getRect().getMax().y - title_bar_h;// + glui->getUIWidthForDevIndepPixelWidth(8);

		title_text->setPos(Vec2f(title_x, title_y));
	}

	{
		const float button_x = this->getRect().getMin().x + glui->getUIWidthForDevIndepPixelWidth(2);
		const float button_y = this->getRect().getMax().y - title_bar_h + glui->getUIWidthForDevIndepPixelWidth(4);

		collapse_expand_button->setPos(Vec2f(button_x, button_y));
	}

	updateBackgroundOverlayTransform();
}


void GLUICollapsableGroupBox::updateBackgroundOverlayTransform()
{
	const Vec2f botleft = getRect().getMin();
	const Vec2f dims    = getDims();

	const float y_scale = opengl_engine->getViewPortAspectRatio();
	background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, m_z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}
