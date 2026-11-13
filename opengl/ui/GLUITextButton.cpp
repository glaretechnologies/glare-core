/*=====================================================================
GLUITextButton.cpp
------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "GLUITextButton.h"


#include "GLUI.h"
#include <graphics/SRGBUtils.h>
#include "../OpenGLMeshRenderData.h"
#include "../MeshPrimitiveBuilding.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


GLUITextButton::CreateArgs::CreateArgs()
{
	sizing_type_x = GLUIWidget::SizingType::SizingType_FixedSizePx;
	sizing_type_y = GLUIWidget::SizingType::SizingType_FixedSizePx;
	fixed_size = Vec2f(100, std::ceil(GLUI::getDefaultFontSizePx() * 2.4f));

	font_size_px = GLUI::getDefaultFontSizePx();

	background_colour           = Colour3f(1.f);
	toggled_background_colour   = toLinearSRGB(Colour3f(0.7f, 0.8f, 1.f));
	mouseover_background_colour = toLinearSRGB(Colour3f(0.8f));

	text_colour           = toLinearSRGB(Colour3f(0.1f));
	toggled_text_colour   = toLinearSRGB(Colour3f(0.1f));
	mouseover_text_colour = toLinearSRGB(Colour3f(0.1f));

	pressed_background_colour = toLinearSRGB(Colour3f(0.6f));
	pressed_text_colour       = toLinearSRGB(Colour3f(0.1f));

	corner_radius_px = 4.f;

	border_width_px         = 1.f;
	border_colour           = toLinearSRGB(Colour3f(0.6f));
	mouseover_border_colour = toLinearSRGB(Colour3f(0.35f));

	shadow_num_layers  = 4;
	shadow_offset_px   = 2.f;
	shadow_layer_alpha = 0.07f;

	z = 0.f;
}


GLUITextButton::GLUITextButton(GLUI& glui_, const std::string& button_text_, const CreateArgs& args_)
:	handler(NULL),
	border_decoration_index(-1),
	toggled(false),
	pressed(false),
	colour_state(ColourState_Normal)
{
	glui = &glui_;
	opengl_engine = glui_.opengl_engine.ptr();
	tooltip = args_.tooltip;
	args = args_;
	m_z = args_.z;
	m_botleft = Vec2f(0.f);
	button_text = button_text_;

	// Drop shadow layers, furthest back.  Each layer extends 1 pixel further than the previous one, past the outline.
	for(int i=0; i<args.shadow_num_layers; ++i)
		addDecoration(/*colour=*/Colour3f(0.f), /*alpha=*/args.shadow_layer_alpha, /*expand_px=*/args.border_width_px + (float)(i + 1), /*offset_px=*/Vec2f(0, -args.shadow_offset_px), /*z_offset=*/0.003f);

	// Outline, just behind the text view background.
	if(args.border_width_px > 0)
	{
		border_decoration_index = (int)decorations.size();
		addDecoration(/*colour=*/args.border_colour, /*alpha=*/1.f, /*expand_px=*/args.border_width_px, /*offset_px=*/Vec2f(0.f), /*z_offset=*/0.002f);
	}

	makeTextView(/*visible=*/true);

	this->setFixedDimsPx(max(fixed_size, Vec2f(glui->getDevIndepPixelWidthForUIWidth(rect.getWidths().x), glui->getDevIndepPixelWidthForUIWidth(rect.getWidths().y))));
}


GLUITextButton::~GLUITextButton()
{
	checkRemoveAndDeleteWidget(glui, text_view);

	for(size_t i=0; i<decorations.size(); ++i)
		checkRemoveOverlayObAndSetRefToNull(opengl_engine, decorations[i].ob);
}


// Creates text_view with the normal colours, adds it to the GLUI, and sets rect from it.
void GLUITextButton::makeTextView(bool visible)
{
	GLUITextView::CreateArgs text_args;
	text_args.background_colour = args.background_colour;
	text_args.background_corner_radius_px = args.corner_radius_px;
	text_args.text_colour = args.text_colour;
	text_args.padding_px = 8;
	text_args.text_selectable = false;
	text_args.font_size_px = args.font_size_px;
	text_args.z = m_z;
	text_args.botleft = m_botleft;
	text_view = new GLUITextView(*glui, button_text, text_args);
	text_view->setVisible(visible);
	glui->addWidget(text_view);

	this->rect = text_view->getRect(); // getBackgroundRect();

	updateDecorationTransforms();
}


void GLUITextButton::addDecoration(const Colour3f& colour, float alpha, float expand_px, const Vec2f& offset_px, float z_offset)
{
	Decoration decoration;
	decoration.ob = new OverlayObject();
	decoration.ob->mesh_data = opengl_engine->getUnitQuadMeshData();
	decoration.ob->material.albedo_linear_rgb = colour;
	decoration.ob->material.alpha = alpha;
	decoration.expand_px = expand_px;
	decoration.offset_px = offset_px;
	decoration.z_offset = z_offset;
	decoration.last_mesh_dims = Vec2f(-1.f);

	opengl_engine->addOverlayObject(decoration.ob);

	decorations.push_back(decoration);
}


// Positions the outline and shadow shapes around the text view background.  Rounded-corner meshes are rebuilt if their dims changed.
void GLUITextButton::updateDecorationTransforms()
{
	const Rect2f background_rect = text_view->getRect();
	const float y_scale = opengl_engine->getViewPortAspectRatio(); // scale from GL UI to opengl coords
	const bool use_rounded_corners = args.corner_radius_px > 0;

	for(size_t i=0; i<decorations.size(); ++i)
	{
		Decoration& decoration = decorations[i];

		const float expand = glui->getUIWidthForDevIndepPixelWidth(decoration.expand_px);
		const Vec2f min  = background_rect.getMin() + glui->getUIWidthForDevIndepPixelWidths(decoration.offset_px) - Vec2f(expand);
		const Vec2f dims = background_rect.getWidths() + Vec2f(2 * expand);
		const float z = m_z + decoration.z_offset;

		decoration.ob->ob_to_world_matrix = Matrix4f::translationMatrix(min.x, min.y * y_scale, z) *
			(use_rounded_corners ? Matrix4f::scaleMatrix(1, y_scale, 1) : // For rounded corner geometry
				Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1));

		if(use_rounded_corners && (decoration.last_mesh_dims != dims))
		{
			// The corners are centred on the same points as the background corners, so the shape extends evenly around the background.
			decoration.ob->mesh_data = MeshPrimitiveBuilding::makeRoundedCornerRect(*opengl_engine->vert_buf_allocator, /*i=*/Vec4f(1,0,0,0), /*j=*/Vec4f(0,1,0,0),
				/*w=*/dims.x, /*h=*/dims.y,
				/*corner radius=*/glui->getUIWidthForDevIndepPixelWidth(args.corner_radius_px + decoration.expand_px), /*tris_per_corner=*/8);

			decoration.last_mesh_dims = dims;
		}
	}
}


void GLUITextButton::handleMousePress(MouseEvent& event)
{
	if(!isVisible())
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(event.gl_coords);
	if(rect.inOpenRectangle(coords))
	{
		pressed = true;
		updateColours(/*mouse_in_rect=*/true);

		if(handler)
		{
			GLUICallbackEvent callback_event;
			callback_event.widget = this;
			handler->eventOccurred(callback_event);
			if(callback_event.accepted)
				event.accepted = true;
		}

		if(on_pressed)
		{
			GLUICallbackEvent callback_event;
			callback_event.widget = this;
			on_pressed(callback_event);
			if(callback_event.accepted)
				event.accepted = true;
		}
	}
}


void GLUITextButton::handleMouseRelease(MouseEvent& event)
{
	if(pressed)
	{
		pressed = false;
		updateColours(rect.inOpenRectangle(glui->UICoordsForOpenGLCoords(event.gl_coords)));
	}
}


// The second click of a double-click arrives as a double-click event instead of a press event, so treat it as a press.
// A release event follows, which clears pressed.
void GLUITextButton::handleMouseDoubleClick(MouseEvent& event)
{
	handleMousePress(event);
}


void GLUITextButton::doHandleMouseMoved(MouseEvent& mouse_event)
{
	if(!isVisible())
		return;

	const Vec2f coords = glui->UICoordsForOpenGLCoords(mouse_event.gl_coords);

	const bool mouse_in_rect = rect.inOpenRectangle(coords);
	if(mouse_in_rect)
		mouse_event.accepted = true;

	updateColours(mouse_in_rect);
}


// The pressed colours take priority over the toggled colours, which take priority over the mouse-over colours.
// The button stays pressed if the mouse moves off it while the mouse button is held down, as with GLUIButton.
// Only sets the text view colours when the colour state changes, since this is called on every mouse move.
void GLUITextButton::updateColours(bool mouse_in_rect)
{
	const ColourState new_state =
		pressed ? ColourState_Pressed :
		(toggled ? ColourState_Toggled : 
		(mouse_in_rect ? ColourState_MouseOver : ColourState_Normal));

	if(new_state != colour_state)
	{
		colour_state = new_state;

		switch(new_state)
		{
		case ColourState_Normal:
			text_view->setBackgroundColour(args.background_colour);
			text_view->setTextColour(args.text_colour);
			break;
		case ColourState_MouseOver:
			text_view->setBackgroundColour(args.mouseover_background_colour);
			text_view->setTextColour(args.mouseover_text_colour);
			break;
		case ColourState_Toggled:
			text_view->setBackgroundColour(args.toggled_background_colour);
			text_view->setTextColour(args.toggled_text_colour);
			break;
		case ColourState_Pressed:
			text_view->setBackgroundColour(args.pressed_background_colour);
			text_view->setTextColour(args.pressed_text_colour);
			break;
		}

		if(border_decoration_index >= 0)
			decorations[border_decoration_index].ob->material.albedo_linear_rgb = (new_state == ColourState_MouseOver) ? args.mouseover_border_colour : args.border_colour;
	}
}


void GLUITextButton::viewportResized()
{
	text_view->viewportResized();

	this->rect = text_view->getRect(); // getBackgroundRect();

	updateDecorationTransforms();
}


void GLUITextButton::rebuild()
{
	const bool old_visible = text_view->isVisible();

	glui->removeWidget(text_view);
	text_view = nullptr;

	makeTextView(old_visible);

	// The new text view has the normal colours.  Reapply the colours for the current state.
	colour_state = ColourState_Normal;
	if(border_decoration_index >= 0)
		decorations[border_decoration_index].ob->material.albedo_linear_rgb = args.border_colour;
	updateColours(rect.inOpenRectangle(glui->getLastMouseUICoords()));
}


void GLUITextButton::setText(const std::string& new_text)
{
	if(new_text == button_text)
		return;

	button_text = new_text;
	text_view->setText(button_text);

	this->rect = text_view->getRect();

	updateDecorationTransforms();

	this->setFixedDimsPx(max(args.fixed_size, Vec2f(glui->getDevIndepPixelWidthForUIWidth(rect.getWidths().x), glui->getDevIndepPixelWidthForUIWidth(rect.getWidths().y))));
}


Vec2f GLUITextButton::getMinDims() const
{
	return text_view->getMinDims();
}


void GLUITextButton::setPos(const Vec2f& botleft)
{
	//Vec2f extra = text_vuiewgetRect
	text_view->setPos(botleft/* + Vec2f(text_view->getPaddingWidth())*/);

	rect = text_view->getRect(); // getBackgroundRect();

	updateDecorationTransforms();
}


void GLUITextButton::setAvailableRegionDims(const Vec2f& available_dims)
{
	Vec2f dims = this->getDims();

	if(sizing_type_x == SizingType_Expanding)
		dims.x = myMax(dims.x, available_dims.x);
	if(sizing_type_y == SizingType_Expanding)
		dims.y = myMax(dims.y, available_dims.y);

	rect = Rect2f::fromMinAndSpan(this->getRect().getMin(), dims);
	text_view->setFixedDimsUICoords(dims);

	updateDecorationTransforms();
}


void GLUITextButton::setZ(float new_z)
{
	this->m_z = new_z;

	text_view->setZ(new_z);

	updateDecorationTransforms();
}


void GLUITextButton::setClipRegion(const Rect2f& clip_rect)
{
	text_view->setClipRegion(clip_rect);

	for(size_t i=0; i<decorations.size(); ++i)
		decorations[i].ob->clip_region = glui->OpenGLRectCoordsForUICoords(clip_rect);
}


void GLUITextButton::setToggled(bool toggled_)
{
	this->toggled = toggled_;

	updateColours(rect.inOpenRectangle(glui->getLastMouseUICoords()));
}


void GLUITextButton::setVisible(bool visible)
{
	text_view->setVisible(visible);

	for(size_t i=0; i<decorations.size(); ++i)
		decorations[i].ob->draw = visible;
}


bool GLUITextButton::isVisible()
{
	return text_view->isVisible();
}
