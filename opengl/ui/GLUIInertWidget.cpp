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
{
	sizing_type_x = GLUIWidget::SizingType::SizingType_FixedSizeUICoords;
	sizing_type_y = GLUIWidget::SizingType::SizingType_FixedSizeUICoords;
	fixed_size = Vec2f(0.05f, 0.05f);
}


GLUIInertWidget::GLUIInertWidget(GLUI& glui_, const CreateArgs& args_)
{
	glui = &glui_;
	opengl_engine = glui_.opengl_engine.ptr();
	args = args_;
	m_z = args_.z;

	sizing_type_x = args.sizing_type_x;
	sizing_type_y = args.sizing_type_y;
	fixed_size    = args.fixed_size;

	const Vec2f botleft(0.f);
	const Vec2f dims = computeDims(/*old dims=*/Vec2f(0.1f), *glui);
	rect = Rect2f(botleft, botleft + dims);

	background_overlay_ob = new OverlayObject();
	background_overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	background_overlay_ob->material.albedo_linear_rgb = args.background_colour;
	background_overlay_ob->material.alpha = args.background_alpha;

	opengl_engine->addOverlayObject(background_overlay_ob);
}


GLUIInertWidget::~GLUIInertWidget()
{
	checkRemoveOverlayObAndSetRefToNull(opengl_engine, background_overlay_ob);
}


void GLUIInertWidget::handleMousePress(MouseEvent& event)
{
	if(!background_overlay_ob->draw)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
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

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIInertWidget::doHandleMouseMoved(MouseEvent& event)
{
	if(!background_overlay_ob->draw)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inClosedRectangle(coords))
		event.accepted = true;
}


void GLUIInertWidget::doHandleMouseWheelEvent(MouseWheelEvent& event)
{
	if(!background_overlay_ob->draw)
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
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
	const Vec2f botleft = getRect().getMin();
	const Vec2f dims = computeDims(getRect().getWidths(), *glui);
	rect = Rect2f(botleft, botleft + dims);

	const float y_scale = opengl_engine->getViewPortAspectRatio();
	background_overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, m_z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);
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
	background_overlay_ob->clip_region = glui->OpenGLRectCoordsForUICoords(clip_rect);
}
