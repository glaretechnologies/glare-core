/*=====================================================================
GLUIInertWidget.cpp
-------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "GLUIInertWidget.h"


#include "GLUI.h"


GLUIInertWidget::CreateArgs::CreateArgs()
:	background_colour(Colour3f(0.7f)),
	background_alpha(1.f),
	z(0.f)
{}


GLUIInertWidget::GLUIInertWidget(GLUI& glui, Reference<OpenGLEngine>& opengl_engine_, const CreateArgs& args_)
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


GLUIInertWidget::~GLUIInertWidget()
{
	if(background_overlay_ob)
		opengl_engine->removeOverlayObject(background_overlay_ob);
}


void GLUIInertWidget::handleMousePress(MouseEvent& event)
{
	if(!background_overlay_ob->draw)
		return;

	const Vec2f coords = gl_ui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIInertWidget::handleMouseRelease(MouseEvent& event)
{
}


void GLUIInertWidget::handleMouseDoubleClick(MouseEvent& event)
{
	if(!background_overlay_ob->draw)
		return;

	const Vec2f coords = gl_ui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIInertWidget::doHandleMouseMoved(MouseEvent& event)
{
	if(!background_overlay_ob->draw)
		return;

	const Vec2f coords = gl_ui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIInertWidget::doHandleMouseWheelEvent(MouseWheelEvent& event)
{
	if(!background_overlay_ob->draw)
		return;

	const Vec2f coords = gl_ui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIInertWidget::doHandleKeyPressedEvent(KeyEvent& event)
{
}


bool GLUIInertWidget::isVisible()
{
	return background_overlay_ob->draw;
}


void GLUIInertWidget::setVisible(bool visible)
{
	background_overlay_ob->draw = visible;
}


void GLUIInertWidget::updateGLTransform()
{
}


void GLUIInertWidget::setPos(const Vec2f& botleft)
{
	const Vec2f dims = getDims();
	this->rect = Rect2f(botleft, botleft + dims);

	const float y_scale = opengl_engine->getViewPortAspectRatio();
	background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, m_z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}


void GLUIInertWidget::setPosAndDims(const Vec2f& botleft, const Vec2f& dims)
{
	rect = Rect2f(botleft, botleft + dims);

	const float y_scale = opengl_engine->getViewPortAspectRatio();
	background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, m_z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
}


void GLUIInertWidget::setClipRegion(const Rect2f& clip_rect)
{
	background_overlay_ob->clip_region = gl_ui->OpenGLRectCoordsForUICoords(clip_rect);
}
