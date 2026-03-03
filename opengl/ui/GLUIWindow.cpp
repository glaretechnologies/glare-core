/*=====================================================================
GLUIWindow.cpp
--------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "GLUIWindow.h"


#include "GLUI.h"


GLUIWindow::CreateArgs::CreateArgs()
:	title_text_colour(Colour3f(0.9f)),
	background_colour(Colour3f(0.7f)),
	background_alpha(1.f),
	z(0.f),
	padding_px(10),
	background_consumes_events(false)
{}


GLUIWindow::GLUIWindow(GLUI& glui, Reference<OpenGLEngine>& opengl_engine_, const CreateArgs& args_)
:	handler(nullptr)
{
	gl_ui = &glui;
	opengl_engine = opengl_engine_;
	args = args_;
	m_z = args_.z;

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

		title_text = new GLUITextView(*gl_ui, opengl_engine, args.title, Vec2f(0), text_view_args);

		gl_ui->addWidget(title_text);
	}

	//--------------- Create close button ---------------
	{
		GLUIButton::CreateArgs button_args;
		button_args.sizing_type_x = GLUIWidget::SizingType_FixedSizePx;
		button_args.sizing_type_y = GLUIWidget::SizingType_FixedSizePx;
		button_args.fixed_size = Vec2f(22.f); 
		button_args.tooltip = "Close window";
		close_button = new GLUIButton(*gl_ui, opengl_engine, opengl_engine_->getDataDir() + "/gl_data/ui/white_x.png", button_args);
		close_button->setZ(m_z - 0.01f);
		close_button->handler = this;
		gl_ui->addWidget(close_button);
	}


	this->rect = Rect2f(Vec2f(0.f), Vec2f(0.4f));

	updateWidgetTransforms();

	updateBackgroundOverlayTransform();
}


GLUIWindow::~GLUIWindow()
{
	if(background_overlay_ob)
		opengl_engine->removeOverlayObject(background_overlay_ob);

	if(title_text)
		gl_ui->removeWidget(title_text);

	if(close_button)
		gl_ui->removeWidget(close_button);
}


void GLUIWindow::setBodyWidget(const GLUIWidgetRef body_widget_)
{
	body_widget = body_widget_;

	body_widget->setZ(m_z - 0.01f);

	updateWidgetTransforms();
}


void GLUIWindow::handleMousePress(MouseEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = gl_ui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIWindow::handleMouseRelease(MouseEvent& /*event*/)
{
}


void GLUIWindow::handleMouseDoubleClick(MouseEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = gl_ui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIWindow::doHandleMouseMoved(MouseEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = gl_ui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIWindow::doHandleMouseWheelEvent(MouseWheelEvent& event)
{
	if(!background_overlay_ob->draw || !args.background_consumes_events)
		return;

	const Vec2f coords = gl_ui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIWindow::doHandleKeyPressedEvent(KeyEvent& /*event*/)
{
}


bool GLUIWindow::isVisible()
{
	return background_overlay_ob->draw;
}


void GLUIWindow::setVisible(bool visible)
{
	background_overlay_ob->draw = visible;

	if(body_widget)
		body_widget->setVisible(visible);

	title_text->setVisible(visible);
	close_button->setVisible(visible);
}


void GLUIWindow::updateGLTransform()
{
	if(body_widget)
		body_widget->updateGLTransform();

	updateWidgetTransforms();
}


void GLUIWindow::recomputeLayout()
{
	if(body_widget)
		body_widget->recomputeLayout();

	updateWidgetTransforms();
}


void GLUIWindow::setPos(const Vec2f& botleft)
{
	const Vec2f dims = getDims();
	this->rect = Rect2f(botleft, botleft + dims);

	updateWidgetTransforms();
}


void GLUIWindow::setPosAndDims(const Vec2f& botleft, const Vec2f& dims)
{
	rect = Rect2f(botleft, botleft + dims);

	updateWidgetTransforms();
}


void GLUIWindow::setClipRegion(const Rect2f& clip_rect)
{
	// TODO
}


void GLUIWindow::setZ(float new_z)
{
	this->m_z = new_z;

	if(body_widget)
		body_widget->setZ(new_z - 0.01f); // Position nearer to cam
}


void GLUIWindow::removeAllContainedWidgetsFromGLUIAndClear()
{
	if(body_widget)
		body_widget->removeAllContainedWidgetsFromGLUIAndClear();

	checkRemoveAndDeleteWidget(gl_ui, body_widget);
}


void GLUIWindow::eventOccurred(GLUICallbackEvent& ev)
{
	if(handler && (ev.widget == close_button.ptr()))
	{
		GLUICallbackEvent close_ev;
		close_ev.widget = this;
		handler->closeWindowEventOccurred(close_ev);
	}
}


void GLUIWindow::updateWidgetTransforms()
{
	const float title_bar_h = gl_ui->getUIWidthForDevIndepPixelWidth(28);

	const float padding = gl_ui->getUIWidthForDevIndepPixelWidth(args.padding_px);


	if(body_widget)
	{
		const Vec2f body_botleft = Vec2f(this->getRect().getMin().x + padding, this->getRect().getMax().y - title_bar_h - body_widget->getDims().y);
		body_widget->setPos(body_botleft);

		const Vec2f clip_botleft = this->getRect().getMin() + Vec2f(padding);
		const Vec2f clip_topright = max(clip_botleft, this->getRect().getMax() - Vec2f(padding, title_bar_h));
		body_widget->setClipRegion(Rect2f(clip_botleft, clip_topright));
	}

	if(title_text)
	{
		const float title_x = this->getRect().getMin().x + getDims().x/2.f - title_text->getDims().x/2.f;
		const float title_y = this->getRect().getMax().y - title_bar_h + gl_ui->getUIWidthForDevIndepPixelWidth(8);

		title_text->setPos(Vec2f(title_x, title_y));
	}

	if(close_button)
	{
		close_button->setPos(getRect().getMax() - close_button->getDims() - Vec2f(gl_ui->getUIWidthForDevIndepPixelWidth(2)));
	}

	updateBackgroundOverlayTransform();
}


void GLUIWindow::updateBackgroundOverlayTransform()
{
	const Vec2f botleft = getRect().getMin();
	const Vec2f dims    = getDims();

	const float y_scale = opengl_engine->getViewPortAspectRatio();
	background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, m_z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}
